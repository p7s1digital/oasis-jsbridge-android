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
#include "JavaMethod.h"
#include "ArgumentLoader.h"
#include "JavaType.h"
#include "JsBridgeContext.h"
#include "utils.h"
#include "jni-helpers/JniLocalRef.h"
#include "jni-helpers/JniLocalFrame.h"
#include "jni-helpers/JniContext.h"
#include "jni-helpers/JniTypes.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include <string>
#include <stdexcept>

JavaMethod::JavaMethod(const JsBridgeContext *jsBridgeContext, const JniLocalRef<jsBridgeMethod> &method, std::string methodName, bool isLambda)
 : m_methodName(std::move(methodName)),
   m_isLambda(isLambda) {

  JniContext *jniContext = jsBridgeContext->jniContext();

  const JniRef<jclass> &jsBridgeMethodClass = jniContext->getJsBridgeMethodClass();
  const JniRef<jclass> &jsBridgeParameterClass = jniContext->getJsBridgeParameterClass();

  jmethodID isVarArgs = jniContext->getMethodID(jsBridgeMethodClass, "isVarArgs", "()Z");
  m_isVarArgs = jniContext->callBooleanMethod(method, isVarArgs);

  jmethodID getParameters = jniContext->getMethodID(jsBridgeMethodClass, "getParameters", "()[Lde/prosiebensat1digital/oasisjsbridge/Parameter;");
  JObjectArrayLocalRef parameters(jniContext->callObjectMethod<jobjectArray>(method, getParameters));
  const jsize numParameters = parameters.getLength();

  jmethodID getParameterClass = jniContext->getMethodID(jsBridgeParameterClass, "getJava", "()Ljava/lang/Class;");

  m_argumentLoaders.resize((size_t) numParameters);

  // Create ArgumentLoader instances
  for (jsize i = 0; i < numParameters; ++i) {
    JniLocalRef<jsBridgeParameter> parameter = parameters.getElement<jsBridgeParameter>(i);
    JniLocalRef<jclass> javaClass = jniContext->callObjectMethod<jclass>(parameter, getParameterClass);

    if (m_isVarArgs && i == numParameters - 1) {
      jmethodID getComponentType = jniContext->getMethodID(jniContext->getObjectClass(javaClass),
                                                           "getComponentType",
                                                           "()Ljava/lang/Class;");
      JniLocalRef<jclass> componentType = jniContext->callObjectMethod<jclass>(javaClass, getComponentType);
      const JavaType *javaType = jsBridgeContext->getJavaTypes().get(jsBridgeContext, componentType, m_isLambda /*boxed*/);
      m_argumentLoaders[i] = new ArgumentLoader(javaType, parameter, true);
      break;
    }

    const JavaType *javaType = jsBridgeContext->getJavaTypes().get(jsBridgeContext, javaClass, m_isLambda /*boxed*/);
    m_argumentLoaders[i] = new ArgumentLoader(javaType, parameter, true);
  }

  parameters.release();

  {
    // Create return value loader
    jmethodID getReturnParameter = jniContext->getMethodID(jsBridgeMethodClass, "getReturnParameter", "()Lde/prosiebensat1digital/oasisjsbridge/Parameter;");
    JniLocalRef<jsBridgeParameter> returnParameter = jniContext->callObjectMethod<jsBridgeParameter>( method, getReturnParameter);
    JniLocalRef<jclass> returnClass = jniContext->callObjectMethod<jclass>(returnParameter, getParameterClass);
    const JavaType *returnType = jsBridgeContext->getJavaTypes().get(jsBridgeContext, returnClass, m_isLambda /*boxed*/);
    m_returnValueLoader = new ArgumentLoader(returnType, returnParameter, true);
  }

  jmethodID methodId = nullptr;

  if (isLambda) {
    auto methodGlobal = JniGlobalRef<jsBridgeMethod>(method);

    m_methodBody = [methodGlobal, this](const JniRef<jobject> &javaThis, const std::vector<JValue> &args) {
      JValue result = m_returnValueLoader->callLambda(methodGlobal, javaThis, args);
#if defined(DUKTAPE)
      return m_returnValueLoader->push(result);
#elif defined(QUICKJS)
      return m_returnValueLoader->fromJava(result);
#endif
    };
  } else {
    jmethodID getJavaMethod = jniContext->getMethodID(jsBridgeMethodClass, "getJavaMethod", "()Ljava/lang/reflect/Method;");
    JniLocalRef<jclass> javaMethod = jniContext->callObjectMethod<jclass>(method, getJavaMethod);
    methodId = jniContext->fromReflectedMethod(javaMethod);

    m_methodBody = [methodId, this](const JniRef<jobject> &javaThis, const std::vector<JValue> &args) {
      JValue result = m_returnValueLoader->callMethod(methodId, javaThis, args);
#if defined(DUKTAPE)
      return m_returnValueLoader->push(result);
#elif defined(QUICKJS)
      return m_returnValueLoader->fromJava(result);
#endif
    };
  }
}

JavaMethod::~JavaMethod() {
  for (auto argumentLoader : m_argumentLoaders) {
    delete argumentLoader;
  }

  delete m_returnValueLoader;
}

#if defined(DUKTAPE)

duk_ret_t JavaMethod::invoke(const JsBridgeContext *jsBridgeContext, const JniRef<jobject> &javaThis) const {

  duk_context *ctx = jsBridgeContext->getCContext();
  JniContext *jniContext = jsBridgeContext->jniContext();

  const auto argCount = duk_get_top(ctx);
  const auto minArgs = m_isVarArgs
      ? m_argumentLoaders.size() - 1
      : m_argumentLoaders.size();

  if (argCount < minArgs || (!m_isVarArgs && argCount > minArgs)) {
    // Wrong number of arguments given - throw an error.
    duk_error(ctx, DUK_ERR_ERROR, "wrong number of arguments when calling Java method %s", m_methodName.c_str());
    // unreachable - duk_error never returns.
    return DUK_RET_ERROR;
  }

  // Release any local objects allocated in this frame when we leave this scope.
  const JniLocalFrame localFrame(jniContext, m_argumentLoaders.size());

  std::vector<JValue> args(m_argumentLoaders.size());

  // Load the arguments off the stack and convert to Java types.
  // Note we're going backwards since the last argument is at the top of the stack.
  if (m_isVarArgs) {
    const ArgumentLoader *argumentLoader = m_argumentLoaders.back();
    args[args.size() - 1] = argumentLoader->popArray(argCount - minArgs, true);
  }
  for (ssize_t i = minArgs - 1; i >= 0; --i) {
    const ArgumentLoader *argumentLoader = m_argumentLoaders[i];
    JValue value = argumentLoader->pop();
    args[i] = value;
  }

  return m_methodBody(javaThis, args);
}

#elif defined(QUICKJS)

JSValue JavaMethod::invoke(const JsBridgeContext *jsBridgeContext, const JniRef<jobject> &javaThis, int argc, JSValueConst *argv) const {

  JSContext *ctx = jsBridgeContext->getCContext();
  JniContext *jniContext = jsBridgeContext->jniContext();

  const int minArgs = m_isVarArgs
      ? m_argumentLoaders.size() - 1
      : m_argumentLoaders.size();

  if (argc < minArgs) {
    // Not enough arguments
    JS_ThrowRangeError(ctx, "Not enough parameters when calling Java method %s (got %d, expected %d)", m_methodName.c_str(), argc, minArgs);
    return JS_UNDEFINED;
  }

  if (!m_isVarArgs && argc > minArgs) {
    // Too many arguments
    JS_ThrowRangeError(ctx, "Too many parameters when calling Java method %s (got %d, expected %d)", m_methodName.c_str(), argc, minArgs);
    return JS_UNDEFINED;
  }

  // Release any local objects allocated in this frame when we leave this scope.
  const JniLocalFrame localFrame(jniContext, m_argumentLoaders.size());

  std::vector<JValue> args(m_argumentLoaders.size());

  // Load the arguments off the stack and convert to Java types.
  if (m_isVarArgs) {
    const ArgumentLoader *argumentLoader = m_argumentLoaders.back();
    // TODO:
    assert(false);
    //args[args.size() - 1] = argumentLoader->toJavaArray(argv[argc - 1], argc - minArgs, true);  // TODO!!!
  }

  for (int i = 0; i < minArgs; ++i) {
    const ArgumentLoader *argumentLoader = m_argumentLoaders[i];
    JValue value = argumentLoader->toJava(argv[i]);
    args[i] = value;
  }

  return m_methodBody(javaThis, args);
}

#endif

