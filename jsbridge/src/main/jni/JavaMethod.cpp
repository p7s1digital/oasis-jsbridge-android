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

#include "JavaType.h"
#include "JniCache.h"
#include "JniTypes.h"
#include "JsBridgeContext.h"
#include "jni-helpers/JniLocalRef.h"
#include "jni-helpers/JniLocalFrame.h"
#include "jni-helpers/JniContext.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include <string>
#include <stdexcept>

JavaMethod::JavaMethod(const JsBridgeContext *jsBridgeContext, const JniLocalRef<jsBridgeMethod> &method, std::string methodName, bool isLambda)
 : m_methodName(std::move(methodName)),
   m_isLambda(isLambda) {

  const JniContext *jniContext = jsBridgeContext->getJniContext();
  MethodInterface methodInterface = jsBridgeContext->getJniCache()->getMethodInterface(method);

  m_isVarArgs = methodInterface.isVarArgs();
  JObjectArrayLocalRef parameters = methodInterface.getParameters();
  const jsize numParameters = parameters.getLength();

  m_argumentTypes.resize((size_t) numParameters);

  // Create JavaType instances
  for (jsize i = 0; i < numParameters; ++i) {
    JniLocalRef<jsBridgeParameter> parameter = parameters.getElement<jsBridgeParameter>(i);

    if (m_isVarArgs && i == numParameters - 1) {
      // TODO: create the array component Parameter, maybe sth like Parameter.getArrayComponent()
      //Parameter parameterInterface = jsBridgeContext->getJniCache()->getParameterInterface(parameter);
      //jmethodID getVarArgParameter = jniContext->getMethodID(
      //    jsBridgeParameterClass, "getVarArgParameter", "()Lde/prosiebensat1digital/oasisjsbridge/Parameter;");
      //JniLocalRef<jsBridgeParameter> varArgParameter = jniContext->callObjectMethod<jsBridgeParameter>(javaClass, getVarArgParameter);
      //auto javaType = jsBridgeContext->getJavaTypeProvider().makeUniqueType(varArgParameter, m_isLambda /*boxed*/);
      //m_argumentTypes[i] = std::move(javaType);
      break;
    }

    m_argumentTypes[i] = jsBridgeContext->getJavaTypeProvider().makeUniqueType(parameter, m_isLambda /*boxed*/);
  }

  parameters.release();

  {
    // Create return value loader
    JniLocalRef<jsBridgeParameter> returnParameter = methodInterface.getReturnParameter();
    m_returnValueType = jsBridgeContext->getJavaTypeProvider().makeUniqueType(returnParameter, m_isLambda /*boxed*/);
  }

  jmethodID methodId = nullptr;

  if (isLambda) {
    auto methodGlobal = JniGlobalRef<jsBridgeMethod>(method);

    m_methodBody = [=](const JniRef<jobject> &javaThis, const std::vector<JValue> &args) {
      JValue result = callLambda(jsBridgeContext, methodGlobal, javaThis, args);
#if defined(DUKTAPE)
      return m_returnValueType->push(result, true /*boxed*/);
#elif defined(QUICKJS)
      return m_returnValueType->fromJava(result, true);
#endif
    };
  } else {
    JniLocalRef<jobject> javaMethod = methodInterface.getJavaMethod();
    methodId = jniContext->fromReflectedMethod(javaMethod);

    m_methodBody = [methodId, this](const JniRef<jobject> &javaThis, const std::vector<JValue> &args) {
      JValue result = m_returnValueType->callMethod(methodId, javaThis, args);
#if defined(DUKTAPE)
      return m_returnValueType->push(result, true);
#elif defined(QUICKJS)
      return m_returnValueType->fromJava(result, true);
#endif
    };
  }
}

#if defined(DUKTAPE)

#include "StackChecker.h"

duk_ret_t JavaMethod::invoke(const JsBridgeContext *jsBridgeContext, const JniRef<jobject> &javaThis) const {
  duk_context *ctx = jsBridgeContext->getDuktapeContext();
  CHECK_STACK(ctx);

  const JniContext *jniContext = jsBridgeContext->getJniContext();

  const auto argCount = duk_get_top(ctx);
  const auto minArgs = m_isVarArgs
      ? m_argumentTypes.size() - 1
      : m_argumentTypes.size();

  if (argCount < minArgs || (!m_isVarArgs && argCount > minArgs)) {
    // Wrong number of arguments given - throw an error.
    duk_error(ctx, DUK_ERR_ERROR, "wrong number of arguments when calling Java method %s", m_methodName.c_str());
    // unreachable - duk_error never returns.
    return DUK_RET_ERROR;
  }

  // Release any local objects allocated in this frame when we leave this scope.
  const JniLocalFrame localFrame(jniContext, m_argumentTypes.size());

  std::vector<JValue> args(m_argumentTypes.size());

  CHECK_STACK_NOW();

  // Load the arguments off the stack and convert to Java types.
  // Note we're going backwards since the last argument is at the top of the stack.
  if (m_isVarArgs) {
    const auto &argumentType = m_argumentTypes.back();
    args[args.size() - 1] = argumentType->popArray(argCount - minArgs, true /*expanded*/, true /*inScript*/);
  }
  for (ssize_t i = minArgs - 1; i >= 0; --i) {
    const auto &argumentType = m_argumentTypes[i];
    JValue value = argumentType->pop(true /*inScript*/);
    args[i] = std::move(value);
  }

  return m_methodBody(javaThis, args);
}

#elif defined(QUICKJS)

JSValue JavaMethod::invoke(const JsBridgeContext *jsBridgeContext, const JniRef<jobject> &javaThis, int argc, JSValueConst *argv) const {

  JSContext *ctx = jsBridgeContext->getQuickJsContext();
  const JniContext *jniContext = jsBridgeContext->getJniContext();

  const int minArgs = m_isVarArgs
      ? m_argumentTypes.size() - 1
      : m_argumentTypes.size();

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
  const JniLocalFrame localFrame(jniContext, m_argumentTypes.size());

  std::vector<JValue> args(m_argumentTypes.size());

  // Load the arguments off the stack and convert to Java types.
  if (m_isVarArgs) {
    const auto &argumentType = m_argumentTypes.back();
    // TODO: QuickJS varargs
    assert(false);
    //args[args.size() - 1] = argumentLoader->toJavaArray(argv[argc - 1], argc - minArgs, true);
  }

  for (int i = 0; i < minArgs; ++i) {
    const auto &argumentType = m_argumentTypes[i];
    JValue value = argumentType->toJava(argv[i], true /*inScript*/);
    args[i] = std::move(value);
  }

  return m_methodBody(javaThis, args);
}

#endif

// static
JValue JavaMethod::callLambda(const JsBridgeContext *jsBridgeContext, const JniRef<jsBridgeMethod> &method, const JniRef<jobject> &javaThis, const std::vector<JValue> &args) {
  const JniContext *jniContext = jsBridgeContext->getJniContext();
  assert(jniContext != nullptr);

  const JniCache *jniCache = jsBridgeContext->getJniCache();

  JniLocalRef<jclass> objectClass = jniCache->getObjectClass();
  JObjectArrayLocalRef argArray(jniContext, args.size(), objectClass);
  int i = 0;
  for (const auto &arg : args) {
    const auto &argLocalRef = arg.getLocalRef();
    argArray.setElement(i++, argLocalRef);
  }

  JniLocalRef<jobject> ret = jniCache->getMethodInterface(method).callNativeLambda(javaThis, argArray);

  if (jsBridgeContext->hasPendingJniException()) {
    jsBridgeContext->rethrowJniException();
    return JValue();
  }
  return JValue(ret);
}


