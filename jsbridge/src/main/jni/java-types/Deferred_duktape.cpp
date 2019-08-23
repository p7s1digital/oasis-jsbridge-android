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
#include "java-types/Deferred.h"

#include "ArgumentLoader.h"
#include "JsBridgeContext.h"
#include "StackChecker.h"
#include "jni-helpers/JniContext.h"
#include "../../../main/jni/JsBridgeContext.h"

namespace {
  const char *PAYLOAD_PROP_NAME = "\xff\xffpayload";
  const char *PROMISE_OBJECT_PROP_NAME = "\xff\xff" "promise_object";
  const char *PROMISE_OBJECT_GLOBAL_NAME_PREFIX = "javaTypes_deferred_promiseobject_";  // Note: initial "\xff\xff" removed because of JNI string conversion issues

  struct OnPromisePayload {
    JniGlobalRef<jobject> javaDeferred;
    ArgumentLoader argumentLoader;
  };

  extern "C" {
    duk_ret_t onPromiseFulfilled(duk_context *ctx) {
      int argCount = duk_get_top(ctx);
      assert(argCount <= 1);
      CHECK_STACK_OFFSET(ctx, 0 - argCount);

      JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
      assert(jsBridgeContext != nullptr);

      JniContext *jniContext = jsBridgeContext->jniContext();

      // Get the bound native Deferred instance and the generic argument loader
      duk_push_current_function(ctx);

      if (!duk_get_prop_string(ctx, -1, PAYLOAD_PROP_NAME)) {
        duk_pop_n(ctx, 2 + argCount);  // (undefined) OnPromiseFulfilledPayload + current function + function args
        return DUK_RET_ERROR;
      }

      auto payload = reinterpret_cast<const OnPromisePayload *>(duk_get_pointer(ctx, -1));
      duk_pop_2(ctx);  // OnPromiseFulfilledPayload + current function

      // Pop promise value
      JValue value;
      if (argCount == 1) {
        value = payload->argumentLoader.pop();
      } else if (argCount > 1) {
        duk_pop_n(ctx, argCount);
      }

      // Complete the native Deferred
      jniContext->callJsBridgeVoidMethod("resolveDeferred",
                                         "(Lkotlinx/coroutines/CompletableDeferred;Ljava/lang/Object;)V",
                                         payload->javaDeferred, value);
      return 0;
    }

    duk_ret_t onPromiseRejected(duk_context *ctx) {
      int argCount = duk_get_top(ctx);
      assert(argCount <= 1);
      CHECK_STACK_OFFSET(ctx, 0 - argCount);

      JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
      assert(jsBridgeContext != nullptr);

      JniContext *jniContext = jsBridgeContext->jniContext();

      // Get the bound native Deferred instance and the generic argument loader
      duk_push_current_function(ctx);

      if (!duk_get_prop_string(ctx, -1, PAYLOAD_PROP_NAME)) {
        duk_pop_n(ctx, 2 + argCount);  // (undefined) OnPromiseRejectedPayload + current function + function args
        return DUK_RET_ERROR;
      }
      auto payload = reinterpret_cast<const OnPromisePayload *>(duk_require_pointer(ctx, -1));

      duk_pop_2(ctx);  // OnPromiseRejectedPayload + current function

      //JValue value = payload->argumentLoader.pop();
      JValue value;
      if (argCount == 1) {
        value = JValue(jsBridgeContext->getJavaExceptionForJsError());
      }
      duk_pop_n(ctx, argCount);  // function args

      // Reject the native Deferred
      jniContext->callJsBridgeVoidMethod("rejectDeferred", "(Lkotlinx/coroutines/CompletableDeferred;Lde/prosiebensat1digital/oasisjsbridge/JsException;)V", payload->javaDeferred, value);
      jsBridgeContext->checkRethrowJsError();

      return 0;
    }

    duk_ret_t finalizeOnPromise(duk_context *ctx) {
      CHECK_STACK(ctx);

      JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
      assert(jsBridgeContext != nullptr);

      if (duk_get_prop_string(ctx, -1, PAYLOAD_PROP_NAME)) {
        delete reinterpret_cast<OnPromisePayload *>(duk_require_pointer(ctx, -1));
      }

      duk_pop(ctx);  // OnPromisePayload
      return 0;
    }

    // Add resolve and reject to bound PromiseObject instance
    duk_ret_t promiseFunction(duk_context *ctx) {
      CHECK_STACK(ctx);

      JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
      assert(jsBridgeContext != nullptr);

      duk_require_function(ctx, 0);
      duk_require_function(ctx, 1);

      duk_push_current_function(ctx);

      if (!duk_get_prop_string(ctx, -1, PROMISE_OBJECT_PROP_NAME)) {
        duk_pop_2(ctx);  // (undefined) PromiseObject + current function
        return DUK_RET_ERROR;
      }

      // Set PromiseObject.resolve and PromiseObject.reject
      duk_dup(ctx, 0);
      duk_put_prop_string(ctx, -2, "resolve");
      duk_dup(ctx, 1);
      duk_put_prop_string(ctx, -2, "reject");

      duk_pop_2(ctx);  // PromiseObject + current function
      return 0;
    }
  }
}


namespace JavaTypes {

Deferred::Deferred(const JsBridgeContext *jsBridgeContext, const JniGlobalRef <jclass> &classRef)
  : JavaType(jsBridgeContext, classRef)
  , m_objectType(m_jsBridgeContext->getJavaTypes().getObjectType(m_jsBridgeContext)){
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
JValue Deferred::pop(bool inScript, const AdditionalData *additionalData) const {
  auto additionalPopData = dynamic_cast<const AdditionalPopData *>(additionalData);
  assert(additionalPopData != nullptr);

  return pop(inScript, additionalPopData->genericArgumentType, additionalPopData->genericArgumentParameter);
}

JValue Deferred::pop(bool inScript, const JavaType *genericArgumentType, const JniRef<jsBridgeParameter> &genericArgumentParameter) const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  // Create a native Deferred instance
  JniLocalRef<jobject> javaDeferred =
      m_jniContext->callJsBridgeObjectMethod("createCompletableDeferred", "()Lkotlinx/coroutines/CompletableDeferred;");
  m_jsBridgeContext->checkRethrowJsError();

  if (!duk_is_object(m_ctx, -1) || !duk_has_prop_string(m_ctx, -1, "then")) {
    // Not a Promise => directly resolve the native Deferred with the value
    ArgumentLoader argumentLoader(genericArgumentType, genericArgumentParameter, inScript);
    JValue value = argumentLoader.pop();

    m_jniContext->callJsBridgeVoidMethod("resolveDeferred",
                                         "(Lkotlinx/coroutines/CompletableDeferred;Ljava/lang/Object;)V",
                                         javaDeferred, value);
    m_jsBridgeContext->checkRethrowJsError();
    return JValue(javaDeferred);
  }

  const duk_idx_t jsPromiseObjectIdx = duk_normalize_index(m_ctx, -1);
  const duk_idx_t onPromiseFulfilledIdx = duk_push_c_function(m_ctx, onPromiseFulfilled, 1);
  const duk_idx_t onPromiseRejectedIdx = duk_push_c_function(m_ctx, onPromiseRejected, 1);

  // Call JsPromise.then(onPromiseFulfilled, onPromiseRejected)
  duk_push_string(m_ctx, "then");
  duk_dup(m_ctx, onPromiseFulfilledIdx);
  duk_dup(m_ctx, onPromiseRejectedIdx);
  if (duk_pcall_prop(m_ctx, jsPromiseObjectIdx, 2) != DUK_EXEC_SUCCESS) {
    const char *errStr = duk_to_string(m_ctx, -1);
    alog("Error while calling JSPromise.then(): %s", errStr);
    JniLocalRef<jthrowable> javaException = m_jsBridgeContext->getJavaExceptionForJsError();
    m_jniContext->callJsBridgeVoidMethod("rejectDeferred",
                                         "(Lkotlinx/coroutines/CompletableDeferred;Ljava/lang/Object;)V",
                                         javaDeferred, javaException);
    m_jsBridgeContext->checkRethrowJsError();
    duk_pop_2(m_ctx);  // (undefined) ret val + JsPromiseObject
    return JValue(javaDeferred);
  }
  duk_pop(m_ctx);  // ignored ret val

  // Bind the payload to the onPromiseFulfilled function
  auto onPromiseFulfilledPayload = new OnPromisePayload { JniGlobalRef<jobject>(javaDeferred), ArgumentLoader(genericArgumentType, genericArgumentParameter, inScript) };
  duk_push_pointer(m_ctx, reinterpret_cast<void *>(onPromiseFulfilledPayload));
  duk_put_prop_string(m_ctx, onPromiseFulfilledIdx, PAYLOAD_PROP_NAME);

  // Bind the payload to the onPromiseRejected function
  auto onPromiseRejectedPayload = new OnPromisePayload { JniGlobalRef<jobject>(javaDeferred), ArgumentLoader(m_objectType, JniLocalRef<jsBridgeParameter>(), inScript) };
  duk_push_pointer(m_ctx, reinterpret_cast<void *>(onPromiseRejectedPayload));
  duk_put_prop_string(m_ctx, onPromiseRejectedIdx, PAYLOAD_PROP_NAME);

  // Finalizer (which releases the JavaDeferred global ref and the ArgumentLoaderPtr pointer)
  duk_push_c_function(m_ctx, finalizeOnPromise, 1);
  duk_set_finalizer(m_ctx, onPromiseFulfilledIdx);
  duk_push_c_function(m_ctx, finalizeOnPromise, 1);
  duk_set_finalizer(m_ctx, onPromiseRejectedIdx);

  duk_pop_3(m_ctx);  // onPromiseFulfilled + onPromiseRejected + JS Promise object
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
duk_ret_t Deferred::push(const JValue &value, bool inScript, const AdditionalData *additionalData) const {
  CHECK_STACK_OFFSET(m_ctx, 1);

  auto additionalPushData = dynamic_cast<const AdditionalPushData *>(additionalData);
  assert(additionalPushData != nullptr);

  const JniLocalRef<jobject> &jDeferred = value.getLocalRef();

  if (jDeferred.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  // promiseFunction = function (resolve, reject)
  duk_push_c_function(m_ctx, promiseFunction, 2);
  // => STASH: [... promiseFunction]

  // Create a PromiseObject which will be eventually filled with {resolve, reject}
  duk_push_object(m_ctx);
  // => STASH: [... promiseFunction PromiseObject]

  static int promiseCount = 0;
  std::string promiseObjectGlobalName = PROMISE_OBJECT_GLOBAL_NAME_PREFIX + std::to_string(++promiseCount);

  // Put it to the global stash
  duk_dup_top(m_ctx);
  duk_put_global_string(m_ctx, promiseObjectGlobalName.c_str());
  // => STASH: [... promiseFunction PromiseObject]

  // Bind the PromiseObject to the promiseFunction
  duk_put_prop_string(m_ctx, -2 /*promiseFunction*/, PROMISE_OBJECT_PROP_NAME);
  // => STASH: [... promiseFunction]

  // new Promise(promiseFunction)
  if (!duk_get_global_string(m_ctx, "Promise")) {
    // TODO: error
    duk_pop_2(m_ctx);  // (undefined) "Promise" + promiseFunction
    duk_push_null(m_ctx);
    return 1;
  }
  duk_dup(m_ctx, -2 /*promiseFunction*/);
  duk_new(m_ctx, 1);  // [... "Promise" promiseFunction] => [... Promise]
  // => STASH: [... promiseFunction, Promise]

  duk_remove(m_ctx, -2);  // promiseFunction
  // => STASH: [... Promise]

  // Call Java setUpJsPromise()
  m_jniContext->callJsBridgeVoidMethod("setUpJsPromise", "(Ljava/lang/String;Lkotlinx/coroutines/Deferred;Ljava/lang/Class;)V",
                                       JStringLocalRef(m_jniContext, promiseObjectGlobalName.c_str()), jDeferred, additionalPushData->promiseJavaClass);
    m_jsBridgeContext->checkRethrowJsError();

  return 1;
}

}  // namespace JavaType

