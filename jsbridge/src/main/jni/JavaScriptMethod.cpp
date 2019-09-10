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
#include "JsBridgeContext.h"
#include "JavaType.h"
#include "jni-helpers/JniContext.h"
#include "jni-helpers/JniTypes.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include <stdexcept>
#include <string>

JavaScriptMethod::JavaScriptMethod(const JsBridgeContext *jsBridgeContext, const JniRef<jsBridgeMethod> &method, std::string methodName, bool isLambda)
 : m_methodName(std::move(methodName))
 , m_isLambda(isLambda) {

  JniContext *jniContext = jsBridgeContext->jniContext();
  const JavaTypeProvider &javaTypeProvider = jsBridgeContext->getJavaTypeProvider();

  const JniRef<jclass> &jsBridgeMethodClass = jniContext->getJsBridgeMethodClass();
  const JniRef<jclass> &jsBridgeParameterClass = jniContext->getJsBridgeParameterClass();

  {
    // Create return value loader
    jmethodID getReturnParameter = jniContext->getMethodID(jsBridgeMethodClass, "getReturnParameter", "()Lde/prosiebensat1digital/oasisjsbridge/Parameter;");
    JniLocalRef<jsBridgeParameter> returnParameter = jniContext->callObjectMethod<jsBridgeParameter>(method, getReturnParameter);
    m_returnValueType = std::move(javaTypeProvider.makeUniqueType(returnParameter, true /*boxed*/));
    m_returnValueParameter = JniGlobalRef<jsBridgeParameter>(returnParameter);
  }

  jmethodID isVarArgsMethod = jniContext->getMethodID(jsBridgeMethodClass, "isVarArgs", "()Z");
  m_isVarArgs = (bool) jniContext->callBooleanMethod(method, isVarArgsMethod);

  jmethodID getParameters = jniContext->getMethodID(jsBridgeMethodClass, "getParameters", "()[Lde/prosiebensat1digital/oasisjsbridge/Parameter;");
  JObjectArrayLocalRef parameters(jniContext->callObjectMethod<jobjectArray>(method, getParameters));
  const auto numParameters = (size_t) parameters.getLength();

  // Release any local objects allocated in this frame when we leave this scope.
  //const JniLocalFrame localFrame(jniContext, numArgs);

  m_argumentTypes.resize(numParameters);

  // Create ArgumentLoader instances
  for (jsize i = 0; i < numParameters; ++i) {
    JniLocalRef<jsBridgeParameter> parameter = parameters.getElement<jsBridgeParameter >(i);

    if (m_isVarArgs && i == numParameters - 1) {
        auto javaType = javaTypeProvider.makeUniqueType(parameter, false /*boxed*/);
        m_argumentTypes[i] = std::move(javaType);
        break;
    }

    // Always load the boxed type instead of the primitive type (e.g. Integer vs int)
    // because we are going to a Proxy object
    auto javaType = javaTypeProvider.makeUniqueType(parameter, true /*boxed*/);
    m_argumentTypes[i] = std::move(javaType);
  }
}

JavaScriptMethod::JavaScriptMethod(JavaScriptMethod &&other) noexcept
 : m_methodName(std::move(other.m_methodName))
 , m_returnValueType(std::move(other.m_returnValueType))
 , m_returnValueParameter(std::move(other.m_returnValueParameter))
 , m_argumentTypes(std::move(other.m_argumentTypes))
 , m_isVarArgs(other.m_isVarArgs)
 , m_isLambda(other.m_isLambda) {
}

JavaScriptMethod &JavaScriptMethod::operator=(JavaScriptMethod &&other) noexcept {
  m_methodName = std::move(other.m_methodName);
  m_returnValueType = std::move(other.m_returnValueType);
  m_returnValueParameter = std::move(other.m_returnValueParameter);
  m_argumentTypes = std::move(other.m_argumentTypes);
  m_isVarArgs = other.m_isVarArgs;
  m_isLambda = other.m_isLambda;

  return *this;
}

#if defined(DUKTAPE)

#include "StackChecker.h"

JValue JavaScriptMethod::invoke(const JsBridgeContext *jsBridgeContext, void *jsHeapPtr, const JObjectArrayLocalRef &args, bool awaitJsPromise) const {
  duk_context *ctx = jsBridgeContext->getCContext();
  CHECK_STACK(ctx);

  JValue result;

  // Set up the call - push the object, method name, and arguments onto the stack
  duk_push_heapptr(ctx, jsHeapPtr);
  duk_idx_t jsLambdaOrObjectIdx = duk_normalize_index(ctx, -1);

  if (m_isLambda) {
    duk_require_function(ctx, jsLambdaOrObjectIdx);
  } else {
    duk_require_object(ctx, jsLambdaOrObjectIdx);
    duk_push_string(ctx, m_methodName.c_str());
  }

  jsize numArguments = args.isNull() ? 0U : args.getLength();

  for (jsize i = 0; i < numArguments; ++i) {
    JValue arg(args.getElement(i));
    const auto &argumentType = m_argumentTypes[i];
    try {
      if (m_isVarArgs && i == numArguments - 1) {
        numArguments = i + argumentType->pushArray(arg.getLocalRef().staticCast<jarray>(), true /*expand*/, false /*inScript*/);
        break;
      }
      argumentType->push(arg, false /*inScript*/);
    } catch (const std::invalid_argument &e) {
      duk_pop_n(ctx, (m_isLambda ? 1 : 2) + i);  // lambda: func + args, method: obj + methodName + args
      throw e;
    }
  }

  duk_ret_t ret;
  if (m_isLambda) {
    ret = duk_pcall(ctx, numArguments);  // [... func arg1 ... argN] -> [... retval]
  } else {
    ret = duk_pcall_prop(ctx, jsLambdaOrObjectIdx, numArguments);  // [... obj ... key arg1 ... argN] -> [... obj ... retval]
    duk_remove(ctx, jsLambdaOrObjectIdx);
  }
  if (ret == DUK_EXEC_SUCCESS) {
    try {
      bool isDeferred = awaitJsPromise && duk_is_object(ctx, -1) && duk_has_prop_string(ctx, -1, "then");
      if (isDeferred && !m_returnValueType->isDeferred()) {
        result = jsBridgeContext->getJavaTypeProvider().getDeferredType(m_returnValueParameter)->pop(false /*inScript*/);
      } else {
        result = m_returnValueType->pop(false /*inScript*/);
      }
    } catch (const std::invalid_argument &e) {
      throw e;
    }
  } else {
    std::string strError = duk_safe_to_string(ctx, -1);
    duk_pop(ctx);  // ret (pcall error)
    throw std::runtime_error(std::string("Error while calling JS ") + (m_isLambda ? "lambda" : "method") + ": " + strError);
  }

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
    const auto &argumentType = m_argumentTypes[i];
    try {
      if (m_isVarArgs && i == numArguments - 1) {
        // TODO: QuickJS varargs
        //numArguments = i + argumentLoader->pushArray(arg.getLocalRef().staticCast<jarray>(), true);
        break;
      }
      jsArgs[i] = argumentType->fromJava(javaArg, false /*inScript*/);
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
    throw std::runtime_error("Error while calling JS lambda");
  }

  try {
    bool isDeferred = awaitJsPromise && JS_IsObject(ret) && jsBridgeContext->getUtils()->hasPropertyStr(ret, "then");
    if (isDeferred && !m_returnValueType->isDeferred()) {
      result = jsBridgeContext->getJavaTypeProvider().getDeferredType(m_returnValueParameter)->toJava(ret, false /*inScript*/);
    } else {
      result = m_returnValueType->toJava(ret, false /*inScript*/);
    }
    JS_FreeValue(ctx, ret);
  } catch (std::runtime_error& e) {
    JS_FreeValue(ctx, ret);
    throw e;
  }

  return result;
};

#endif
