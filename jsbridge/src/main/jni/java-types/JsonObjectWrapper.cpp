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
#include "JsonObjectWrapper.h"

#include "JsBridgeContext.h"
#include "custom_stringify.h"
#include "log.h"
#include "jni-helpers/JniContext.h"

namespace JavaTypes {

JsonObjectWrapper::JsonObjectWrapper(const JsBridgeContext *jsBridgeContext)
 : JavaType(jsBridgeContext, JavaTypeId::JsonObjectWrapper) {
}

#if defined(DUKTAPE)

#include "StackChecker.h"

namespace {
  extern "C"
  duk_ret_t tryJsonDecode(duk_context *ctx, void *) {
    duk_json_decode(ctx, -1);
    return 1;
  }
}

JValue JsonObjectWrapper::pop(bool inScript) const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  alog("BW - JOW::pop1");
  alog("BW - JOW::pop2");

  // Check if the caller passed in a null string.
  if (duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }

  alog("BW - JOW::pop3");
  duk_require_object_coercible(m_ctx, -1);
  alog("BW - JOW::pop4");

  if (custom_stringify(m_ctx, -1) != DUK_EXEC_SUCCESS) {
    alog_warn("Could not stringify object!");
    m_jsBridgeContext->queueJavaExceptionForJsError();
    duk_pop(m_ctx);
    return JValue();
  }

  alog("BW - JOW::pop5");

  JStringLocalRef str(m_jniContext, duk_require_string(m_ctx, -1));
  duk_pop(m_ctx);

  alog("BW - JOW::pop6");

  JniLocalRef<jclass> javaClass = getJavaClass();
  jmethodID method = m_jniContext->getMethodID(javaClass, "<init>", "(Ljava/lang/String;)V");
  JniLocalRef<jobject> localRef = m_jniContext->newObject<jobject>(javaClass, method, str);
  javaClass.release();

  alog("BW - JOW::pop7");

  duk_pop(m_ctx);
  return JValue(localRef);
}

duk_ret_t JsonObjectWrapper::push(const JValue &value, bool inScript) const {
  CHECK_STACK_OFFSET(m_ctx, 1);

  const JniLocalRef<jobject> &jWrapper = value.getLocalRef();
  if (jWrapper.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  jmethodID method = m_jniContext->getMethodID(getJavaClass(), "getJsonString", "()Ljava/lang/String;");
  JStringLocalRef str(m_jniContext->callObjectMethod<jstring>(jWrapper, method));

  m_jsBridgeContext->checkRethrowJsError();

  // Undefined values are returned as an empty string
  if (strlen(str.c_str()) == 0) {
    duk_push_undefined(m_ctx);
    return 1;
  }

  duk_push_string(m_ctx, str.c_str());
  str.release();

  if (duk_safe_call(m_ctx, tryJsonDecode, nullptr, 1, 1) != DUK_EXEC_SUCCESS) {
    const char *err = duk_safe_to_string(m_ctx, -1);
    const auto message = std::string("Error while pushing JsonObjectWrapper value: ") + err;
    duk_pop(m_ctx);
    CHECK_STACK_NOW();
    m_jsBridgeContext->throwTypeException(message, inScript);
  }

  return 1;
}

#elif defined(QUICKJS)

JValue JsonObjectWrapper::toJava(JSValueConst v, bool inScript) const {
  if (!JS_IsObject(v) && !JS_IsNull(v)) {
    const char *message = "Cannot convert return value to Object";
    m_jsBridgeContext->throwTypeException(message, inScript);
    return JValue();
  }

  // Check if the caller passed in a null string.
  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  JSValue jsonValue = custom_stringify(m_ctx, v);
  if (JS_IsUndefined(jsonValue)) {
    JS_FreeValue(m_ctx, jsonValue);
    return JValue();
  }

  const char *jsonCStr = JS_ToCString(m_ctx, jsonValue);
  JStringLocalRef str(m_jniContext, jsonCStr);
  JS_FreeCString(m_ctx, jsonCStr);
  JS_FreeValue(m_ctx, jsonValue);

  JniLocalRef<jclass> javaClass = getJavaClass();
  jmethodID method = m_jniContext->getMethodID(javaClass, "<init>", "(Ljava/lang/String;)V");
  JniLocalRef<jobject> localRef = m_jniContext->newObject<jobject>(javaClass, method, str);
  javaClass.release();

  return JValue(localRef);
}

JSValue JsonObjectWrapper::fromJava(const JValue &value, bool inScript) const {

  const JniLocalRef<jobject> &jWrapper = value.getLocalRef();
  if (jWrapper.isNull()) {
    return JS_NULL;
  }

  jmethodID method = m_jniContext->getMethodID(getJavaClass(), "getJsonString", "()Ljava/lang/String;");
  JStringLocalRef str(m_jniContext->callObjectMethod<jstring>(jWrapper, method));

  m_jsBridgeContext->checkRethrowJsError();

  // Undefined values are returned as an empty string
  if (strlen(str.c_str()) == 0) {
    return JS_UNDEFINED;
  }

  JSValue decodedValue = JS_ParseJSON(m_ctx, str.c_str(), str.length(), "JsonObjectWrapper.cpp");
  str.release();

  if (JS_IsException(decodedValue)) {
    const char *message = "Error while reading JsonObjectWrapper value";
    JS_FreeValue(m_ctx, decodedValue);
    m_jsBridgeContext->throwTypeException(message, inScript);
    return JS_EXCEPTION;
  }

  return decodedValue;
}

#endif

}  // namespace JavaTypes

