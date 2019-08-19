/*
 * Copyright (C) 2019 ProSiebenSat1.Digital GmbH.
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
#include <utils.h>
#include "Deferred.h"

#include "ArgumentLoader.h"
#include "JsBridgeContext.h"
#include "QuickJsUtils.h"
#include "jni-helpers/JniContext.h"

namespace {
  const char *PROMISE_OBJECT_GLOBAL_NAME_PREFIX = "__javaTypes_deferred_promiseobject_";

  struct OnPromisePayload {
    JniGlobalRef<jobject> javaDeferred;
    ArgumentLoader argumentLoader;
  };

  JSValue onPromiseFulfilled(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValueConst *datav) {
    JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
    assert(jsBridgeContext != nullptr);

    JniContext *jniContext = jsBridgeContext->jniContext();

    // data => OnPromisePayload
    auto payload = QuickJsUtils::getCppPtr<OnPromisePayload>(*datav);

    // Pop promise value
    JSValueConst promiseValue = argc >= 1 ? *argv : JS_NULL;
    JValue value = payload->argumentLoader.toJava(promiseValue);

    // Complete the native Deferred
    jniContext->callJsBridgeVoidMethod("resolveDeferred",
                                       "(Lkotlinx/coroutines/CompletableDeferred;Ljava/lang/Object;)V",
                                       payload->javaDeferred, value);
    jsBridgeContext->checkRethrowJsError();

    return JS_UNDEFINED;
  }

  JSValue onPromiseRejected(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValueConst *datav) {
    JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
    assert(jsBridgeContext != nullptr);

    JniContext *jniContext = jsBridgeContext->jniContext();

    // data => OnPromisePayload
    auto payload = QuickJsUtils::getCppPtr<OnPromisePayload>(*datav);

    JS_Throw(ctx, argc > 0 ? JS_DupValue(ctx, *argv) : JS_NULL);
    JValue value(jsBridgeContext->getJavaExceptionForJsError());

    // Reject the native Deferred
    jniContext->callJsBridgeVoidMethod("rejectDeferred", "(Lkotlinx/coroutines/CompletableDeferred;Lde/prosiebensat1digital/oasisjsbridge/JsException;)V", payload->javaDeferred, value);
    jsBridgeContext->checkRethrowJsError();

    return JS_UNDEFINED;
  }

  // Add resolve and reject to bound PromiseObject instance
  JSValue promiseFunction(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic, JSValueConst *datav) {
    JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
    assert(jsBridgeContext != nullptr);

    JSValueConst promiseObject = *datav;

    // Set PromiseObject.resolve and PromiseObject.reject
    JS_SetPropertyStr(ctx, promiseObject, "resolve", argc >= 1 ? JS_DupValue(ctx, argv[0]) : JS_NULL);
    JS_SetPropertyStr(ctx, promiseObject, "reject", argc >= 2 ? JS_DupValue(ctx, argv[1]) : JS_NULL);

    return JS_UNDEFINED;
  }
}


namespace JavaTypes {

Deferred::Deferred(const JsBridgeContext *jsBridgeContext, const JniGlobalRef <jclass> &classRef)
 : JavaType(jsBridgeContext, classRef)
 , m_objectType(m_jsBridgeContext->getJavaTypes().getObjectType(jsBridgeContext)) {
}

class Deferred::AdditionalPopData: public JavaType::AdditionalData {
public:
  AdditionalPopData() = default;

  const JavaType *genericArgumentType = nullptr;
  JniGlobalRef<jsBridgeParameter> genericArgumentParameter;
};

JavaType::AdditionalData *Deferred::createAdditionalPopData(const JniRef<jsBridgeParameter> &parameter) const {
  auto data = new AdditionalPopData();

  const JniRef<jclass> &jsBridgeMethodClass = m_jniContext->getJsBridgeMethodClass();
  const JniRef<jclass> &parameterClass = m_jniContext->getJsBridgeParameterClass();

  jmethodID getGenericParameter = m_jniContext->getMethodID(parameterClass, "getGenericParameter", "()Lde/prosiebensat1digital/oasisjsbridge/Parameter;");
  JniLocalRef<jsBridgeParameter> genericParameter = m_jniContext->callObjectMethod<jsBridgeParameter>(parameter, getGenericParameter);

  jmethodID getGenericJavaClass = m_jniContext->getMethodID(parameterClass, "getJava", "()Ljava/lang/Class;");
  JniLocalRef<jclass> genericJavaClass = m_jniContext->callObjectMethod<jclass>(genericParameter, getGenericJavaClass);

  data->genericArgumentType = m_jsBridgeContext->getJavaTypes().get(m_jsBridgeContext, genericJavaClass, true /*boxed*/);
  data->genericArgumentParameter = JniGlobalRef<jsBridgeParameter>(genericParameter);
  return data;
}

// JS Promise to native Deferred
JValue Deferred::toJava(JSValueConst v, bool inScript, const AdditionalData *additionalData) const {
  auto additionalPopData = dynamic_cast<const AdditionalPopData *>(additionalData);
  assert(additionalPopData != nullptr);

  return toJava(v, inScript, additionalPopData->genericArgumentType, additionalPopData->genericArgumentParameter);
}

JValue Deferred::toJava(JSValueConst v, bool inScript, const JavaType *genericArgumentType,
                        const JniRef <jsBridgeParameter> &genericArgumentParameter) const {

  const QuickJsUtils *utils = m_jsBridgeContext->getUtils();
  assert(utils != nullptr);

  // Create a native Deferred instance
  JniLocalRef<jobject> javaDeferred =
      m_jniContext->callJsBridgeObjectMethod("createCompletableDeferred", "()Lkotlinx/coroutines/CompletableDeferred;");
  m_jsBridgeContext->checkRethrowJsError();

  bool isPromise = JS_IsObject(v) && utils->hasPropertyStr(v, "then");
  if (!isPromise) {
    // Not a Promise => directly resolve the native Deferred with the value
    ArgumentLoader argumentLoader(genericArgumentType, genericArgumentParameter, inScript);
    JValue value = argumentLoader.toJava(v);

    m_jniContext->callJsBridgeVoidMethod("resolveDeferred",
                                         "(Lkotlinx/coroutines/CompletableDeferred;Ljava/lang/Object;)V",
                                         javaDeferred, value);
    m_jsBridgeContext->checkRethrowJsError();
    return JValue(javaDeferred);
  }

  // onPromiseFulfilledFunc with data
  auto onPromiseFulfilledPayload = new OnPromisePayload { JniGlobalRef<jobject>(javaDeferred), ArgumentLoader(genericArgumentType, genericArgumentParameter, inScript) };
  JSValue onPromiseFulfilledPayloadValue = utils->createCppPtrValue<OnPromisePayload>(onPromiseFulfilledPayload, true);
  JSValue onPromiseFulfilledValue = JS_NewCFunctionData(
      m_ctx, onPromiseFulfilled, 1 /*length*/, 0 /*magic*/, 1, &onPromiseFulfilledPayloadValue);
  JS_FreeValue(m_ctx, onPromiseFulfilledPayloadValue);

  // onPromiseRejectedFunc with data
  auto onPromiseRejectedPayload = new OnPromisePayload { JniGlobalRef<jobject>(javaDeferred), ArgumentLoader(m_objectType, JniLocalRef<jsBridgeParameter>(), inScript) };
  JSValue onPromiseRejectedPayloadValue = utils->createCppPtrValue<OnPromisePayload>(onPromiseRejectedPayload, true);
  JSValue onPromiseRejectedValue = JS_NewCFunctionData(
      m_ctx, onPromiseRejected, 1 /*length*/, 0 /*magic*/, 1, &onPromiseRejectedPayloadValue);
  JS_FreeValue(m_ctx, onPromiseRejectedPayloadValue);

  // JsPromise.then()
  JSValue thenValue = JS_GetPropertyStr(m_ctx, v, "then");
  assert(JS_IsFunction(m_ctx, thenValue));

  // Call JsPromise.then(onPromiseFulfilled, onPromiseRejected)
  JSValueConst thenArgs[2];
  thenArgs[0] = onPromiseFulfilledValue;
  thenArgs[1] = onPromiseRejectedValue;
  JSValue ret = JS_Call(m_ctx, thenValue, v, 2, thenArgs);

  if (JS_IsException(ret)) {
    alog("Error while calling JSPromise.then()");

    JniLocalRef<jthrowable> javaException = m_jsBridgeContext->getJavaExceptionForJsError();
    m_jniContext->callJsBridgeVoidMethod("rejectDeferred",
                                         "(Lkotlinx/coroutines/CompletableDeferred;Ljava/lang/Object;)V",
                                         javaDeferred, javaException);
    m_jsBridgeContext->checkRethrowJsError();
  }

  JS_FreeValue(m_ctx, ret);
  JS_FreeValue(m_ctx, onPromiseFulfilledValue);
  JS_FreeValue(m_ctx, onPromiseRejectedValue);
  JS_FreeValue(m_ctx, thenValue);

  return JValue(javaDeferred);
}

class Deferred::AdditionalPushData: public JavaType::AdditionalData {
public:
  AdditionalPushData() = default;

  JniGlobalRef<jclass> promiseJavaClass;
};

JavaType::AdditionalData *Deferred::createAdditionalPushData(const JniRef<jsBridgeParameter> &parameter) const {
  auto data = new AdditionalPushData();

  const JniRef<jclass> &jsBridgeMethodClass = m_jniContext->getJsBridgeMethodClass();
  const JniRef<jclass> &parameterClass = m_jniContext->getJsBridgeParameterClass();

  jmethodID getGenericParameter = m_jniContext->getMethodID(parameterClass, "getGenericParameter", "()Lde/prosiebensat1digital/oasisjsbridge/Parameter;");
  JniLocalRef<jsBridgeParameter> genericParameter = m_jniContext->callObjectMethod<jsBridgeParameter>(parameter, getGenericParameter);

  jmethodID getGenericJavaClass = m_jniContext->getMethodID(parameterClass, "getJava", "()Ljava/lang/Class;");
  JniLocalRef<jclass> genericJavaClass = m_jniContext->callObjectMethod<jclass>(genericParameter, getGenericJavaClass);

  data->promiseJavaClass = JniGlobalRef<jclass>(genericJavaClass);
  return data;
}

// Native Deferred to JS Promise
JSValue Deferred::fromJava(const JValue &value, bool inScript,
                           const AdditionalData *additionalData) const {
  auto additionalPushData = dynamic_cast<const AdditionalPushData *>(additionalData);
  assert(additionalPushData != nullptr);

  const JniLocalRef<jobject> &jDeferred = value.getLocalRef();

  if (jDeferred.isNull()) {
    return JS_NULL;
  }

  // Create a PromiseObject which will be eventually filled with {resolve, reject}
  JSValue promiseObject = JS_NewObject(m_ctx);

  static int promiseCount = 0;
  std::string promiseObjectGlobalName = PROMISE_OBJECT_GLOBAL_NAME_PREFIX + std::to_string(++promiseCount);

  // Put it to the global stash
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JS_SetPropertyStr(m_ctx, globalObj, promiseObjectGlobalName.c_str(), JS_DupValue(m_ctx, promiseObject));
  JS_FreeValue(m_ctx, globalObj);

  // promiseFunction = function(resolve, reject) + data (promiseObject)
  JSValue promiseFunctionValue = JS_NewCFunctionData(m_ctx, promiseFunction, 1, 0, 1, &promiseObject);
  JS_FreeValue(m_ctx, promiseObject);

  // Create a new JS promise with the promiseFunction as parameter
  // => new Promise(promiseFunction)
  JSValue promiseCtor = JS_GetPropertyStr(m_ctx, globalObj, "Promise");
  JSValue promiseInstance = JS_CallConstructor(m_ctx, promiseCtor, 1, &promiseFunctionValue);
  assert(JS_IsObject(promiseInstance));
  JS_FreeValue(m_ctx, promiseCtor);

  // Call Java setUpJsPromise()
  m_jniContext->callJsBridgeVoidMethod("setUpJsPromise", "(Ljava/lang/String;Lkotlinx/coroutines/Deferred;Ljava/lang/Class;)V",
                                       JStringLocalRef(m_jniContext, promiseObjectGlobalName.c_str()), jDeferred, additionalPushData->promiseJavaClass);
  m_jsBridgeContext->checkRethrowJsError();

  return promiseInstance;
}

}  // namespace JavaTypes
