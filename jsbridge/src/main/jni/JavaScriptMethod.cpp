/*
 * Copyright (C) 2019 ProSiebenSat1.Digital GmbH.
 *
 * Originally based on Duktape Android:
 * Copyright (C) 2015 Square, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "JavaScriptMethod.h"
#include "ArgumentLoader.h"
#include "JsBridgeContext.h"
#include "JavaType.h"
#include "StackChecker.h"
#include "jni-helpers/JniContext.h"
#include "jni-helpers/JniTypes.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include <stdexcept>
#include <string>

JavaScriptMethod::JavaScriptMethod(const JsBridgeContext *jsBridgeContext, const JniRef<jsBridgeMethod> &method, std::string methodName, bool isLambda)
 : m_methodName(std::move(methodName))
 , m_isLambda(isLambda) {

  JniContext *jniContext = jsBridgeContext->jniContext();
  const JavaTypeMap &javaTypes = jsBridgeContext->getJavaTypes();

  const JniRef<jclass> &jsBridgeMethodClass = jniContext->getJsBridgeMethodClass();
  const JniRef<jclass> &jsBridgeParameterClass = jniContext->getJsBridgeParameterClass();

  jmethodID getParameterClass = jniContext->getMethodID(jsBridgeParameterClass, "getJava", "()Ljava/lang/Class;");

  {
    // Create return value loader
    jmethodID getReturnParameter = jniContext->getMethodID(jsBridgeMethodClass, "getReturnParameter", "()Lde/prosiebensat1digital/oasisjsbridge/Parameter;");
    JniLocalRef<jsBridgeParameter> returnParameter = jniContext->callObjectMethod<jsBridgeParameter>(method, getReturnParameter);
    JniLocalRef<jclass> returnClass = jniContext->callObjectMethod<jclass>(returnParameter, getParameterClass);
    const JavaType *returnType = jsBridgeContext->getJavaTypes().get(jsBridgeContext, returnClass, true /*boxed*/);
    m_returnValueLoader = new ArgumentLoader(returnType, returnParameter, false);
  }

  jmethodID isVarArgsMethod = jniContext->getMethodID(jsBridgeMethodClass, "isVarArgs", "()Z");
  m_isVarArgs = (bool) jniContext->callBooleanMethod(method, isVarArgsMethod);

  jmethodID getParameters = jniContext->getMethodID(jsBridgeMethodClass, "getParameters", "()[Lde/prosiebensat1digital/oasisjsbridge/Parameter;");
  JObjectArrayLocalRef parameters(jniContext->callObjectMethod<jobjectArray>(method, getParameters));
  const auto numParameters = (size_t) parameters.getLength();

  // Release any local objects allocated in this frame when we leave this scope.
  //const JniLocalFrame localFrame(jniContext, numArgs);

  m_argumentLoaders.resize(numParameters);

  // Create ArgumentLoader instances
  for (jsize i = 0; i < numParameters; ++i) {
    JniLocalRef<jsBridgeParameter> parameter = parameters.getElement<jsBridgeParameter >(i);
    JniLocalRef<jclass> javaClass = jniContext->callObjectMethod<jclass>(parameter, getParameterClass);

    if (m_isVarArgs && i == numParameters - 1) {
        const JavaType *javaType = javaTypes.get(jsBridgeContext, javaClass);
        m_argumentLoaders[i] = new ArgumentLoader(javaType, parameter, false);
        break;
    }

    // Always load the boxed type instead of the primitive type (e.g. Integer vs int)
    // because we are going to a Proxy object
    const JavaType *javaType = javaTypes.get(jsBridgeContext, javaClass, true /*boxed*/);

    m_argumentLoaders[i] = new ArgumentLoader(javaType, parameter, false);
  }
}

JavaScriptMethod::JavaScriptMethod(JavaScriptMethod &&other) noexcept
 : m_methodName(std::move(other.m_methodName))
 , m_returnValueLoader(other.m_returnValueLoader)
 , m_argumentLoaders(std::move(other.m_argumentLoaders))
 , m_isVarArgs(other.m_isVarArgs)
 , m_isLambda(other.m_isLambda) {

  other.m_returnValueLoader = nullptr;
  other.m_argumentLoaders = std::vector<ArgumentLoader *>();
}

JavaScriptMethod &JavaScriptMethod::operator=(JavaScriptMethod &&other) noexcept {
  m_methodName = std::move(other.m_methodName);
  m_returnValueLoader = other.m_returnValueLoader;
  other.m_returnValueLoader = nullptr;
  m_argumentLoaders = std::move(other.m_argumentLoaders);
  other.m_argumentLoaders = std::vector<ArgumentLoader *>();
  m_isVarArgs = other.m_isVarArgs;
  m_isLambda = other.m_isLambda;

  return *this;
}

JavaScriptMethod::~JavaScriptMethod() {
  for (auto argumentLoader : m_argumentLoaders) {
    delete argumentLoader;
  }

  delete m_returnValueLoader;
}

#if defined(DUKTAPE)

JValue JavaScriptMethod::invoke(const JsBridgeContext *jsBridgeContext, void *jsHeapPtr, const JObjectArrayLocalRef &args, bool awaitJsPromise) const {
  duk_context *ctx = jsBridgeContext->getCContext();
  CHECK_STACK(ctx);

  JValue result;

  // Set up the call - push the object, method name, and arguments onto the stack
  duk_push_heapptr(ctx, jsHeapPtr);
  duk_idx_t jsObjectIdx = duk_normalize_index(ctx, -1);

  if (m_isLambda) {
    duk_require_function(ctx, jsObjectIdx);
    duk_dup(ctx, jsObjectIdx);
  } else {
    duk_push_string(ctx, m_methodName.c_str());
  }

  jsize numArguments = args.isNull() ? 0U : args.getLength();

  for (jsize i = 0; i < numArguments; ++i) {
    JValue arg(args.getElement(i));
    const ArgumentLoader *argumentLoader = m_argumentLoaders[i];
    try {
      if (m_isVarArgs && i == numArguments - 1) {
        numArguments = i + argumentLoader->pushArray(arg.getLocalRef().staticCast<jarray>(), true);
        break;
      }
      argumentLoader->push(arg);
    } catch (const std::invalid_argument &e) {
      // Pop the stack entries pushed above, and any args pushed so far.
      duk_pop_n(ctx, m_isLambda ? 3 : 2 + i);
      throw e;
    }
  }

  duk_ret_t ret;
  if (m_isLambda) {
    ret = duk_pcall(ctx, numArguments);
  } else {
    ret = duk_pcall_prop(ctx, jsObjectIdx, numArguments);
  }
  if (ret == DUK_EXEC_SUCCESS) {
    try {
      bool isDeferred = awaitJsPromise && duk_is_object(ctx, -1) && duk_has_prop_string(ctx, -1, "then");
      if (isDeferred && !m_returnValueLoader->getJavaType()->isDeferred()) {
        const JavaTypeMap &javaTypeMap = jsBridgeContext->getJavaTypes();
        result = m_returnValueLoader->popDeferred(&javaTypeMap);
      } else {
        result = m_returnValueLoader->pop();
      }
    } catch (const std::invalid_argument &e) {
      duk_pop(ctx);  // jsHeapPtr
      throw e;
    }
  } else {
    std::string strError = duk_safe_to_string(ctx, -1);
    duk_pop_2(ctx);  // pcall error + jsHeapPtr
    throw std::runtime_error(std::string("Error when calling JS lambda: ") + strError);
  }

  duk_pop(ctx);  // jsHeapPtr
  return result;
};

#elif defined(QUICKJS)

#include "QuickJsUtils.h"

JValue JavaScriptMethod::invoke(const JsBridgeContext *jsBridgeContext, JSValueConst jsMethod, JSValueConst jsThis, const JObjectArrayLocalRef &javaArgs, bool awaitJsPromise) const {
  JValue result;
  JSContext *ctx = jsBridgeContext->getCContext();

  int numArguments = javaArgs.isNull() ? 0 : (int) javaArgs.getLength();

  JSValue jsArgs[numArguments];
  for (jsize i = 0; i < numArguments; ++i) {
    JValue javaArg(javaArgs.getElement(i));
    const ArgumentLoader *argumentLoader = m_argumentLoaders[i];
    try {
      if (m_isVarArgs && i == numArguments - 1) {
        //TODO: numArguments = i + argumentLoader->pushArray(arg.getLocalRef().staticCast<jarray>(), true);
        break;
      }
      jsArgs[i] = argumentLoader->fromJava(javaArg);
    } catch (const std::invalid_argument &e) {
      // Free all the JSValue instances which had been added until now
      for (int j = 0; j < i; ++j) {
        JS_FreeValue(ctx, jsArgs[j]);
      }
      jsBridgeContext->queueIllegalArgumentException(e.what());
      return result;
    }
  }

  JSValue ret = JS_Call(ctx, jsMethod, jsThis, numArguments, jsArgs);

  if (JS_IsException(ret)) {
    JS_FreeValue(ctx, ret);
    throw std::runtime_error("Error when calling JS lambda");
  }

  try {
    bool isDeferred = awaitJsPromise && JS_IsObject(ret) && jsBridgeContext->getUtils()->hasPropertyStr(ret, "then");
    if (isDeferred && !m_returnValueLoader->getJavaType()->isDeferred()) {
      const JavaTypeMap &javaTypeMap = jsBridgeContext->getJavaTypes();
      result = m_returnValueLoader->toJavaDeferred(ret, &javaTypeMap);
    } else {
      result = m_returnValueLoader->toJava(ret);
    }
    JS_FreeValue(ctx, ret);
  } catch (std::runtime_error& e) {
    JS_FreeValue(ctx, ret);
    throw e;
  }

  return result;
};

#endif
