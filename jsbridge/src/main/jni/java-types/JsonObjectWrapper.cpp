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

#include "ExceptionHandler.h"
#include "JniCache.h"
#include "JsBridgeContext.h"
#include "custom_stringify.h"
#include "log.h"
#include "exceptions/JniException.h"
#include "exceptions/JsException.h"
#include "jni-helpers/JniContext.h"

namespace JavaTypes {

JsonObjectWrapper::JsonObjectWrapper(const JsBridgeContext *jsBridgeContext, bool isNullable)
 : JavaType(jsBridgeContext, JavaTypeId::JsonObjectWrapper)
 , m_isNullable(isNullable) {
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

JValue JsonObjectWrapper::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  // Check if the caller passed in a null string.
  if (m_isNullable && duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }

  if (custom_stringify(m_ctx, -1, true /*keepErrorStack*/) != DUK_EXEC_SUCCESS) {
    duk_remove(m_ctx, -2);
    throw getExceptionHandler()->getCurrentJsException();
  }

  JStringLocalRef str(m_jniContext, duk_require_string(m_ctx, -1));
  duk_pop(m_ctx);

  JniLocalRef<jobject> localRef = getJniCache()->newJsonObjectWrapper(str);

  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  duk_pop(m_ctx);
  return JValue(localRef);
}

duk_ret_t JsonObjectWrapper::push(const JValue &value) const {
  CHECK_STACK_OFFSET(m_ctx, 1);

  const JniLocalRef<jobject> &jWrapper = value.getLocalRef();
  if (jWrapper.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  JStringLocalRef strRef = getJniCache()->getJsonObjectWrapperString(jWrapper);
  const char *str = strRef.toUtf8Chars();

  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  // Undefined values are returned as an empty string
  if (!str || strlen(str) == 0) {
    duk_push_undefined(m_ctx);
    return 1;
  }

  duk_push_string(m_ctx, str);

  if (duk_safe_call(m_ctx, tryJsonDecode, nullptr, 1, 1) != DUK_EXEC_SUCCESS) {
    CHECK_STACK_NOW();
    duk_pop(m_ctx);
    auto msg = std::string() + "Error while reading JsonObjectWrapper value (\"" + str + "\")";
    throw std::invalid_argument(msg);
  }

  return 1;
}

#elif defined(QUICKJS)

JValue JsonObjectWrapper::toJava(JSValueConst v) const {
  if (m_isNullable && (JS_IsNull(v) || JS_IsUndefined(v))) {
    return JValue();
  }

  JSValue jsonValue = custom_stringify(m_ctx, v, true /*keepErrorStack*/);
  if (JS_IsException(jsonValue)) {
    throw getExceptionHandler()->getCurrentJsException();
  }

  const char *jsonCStr = JS_ToCString(m_ctx, jsonValue);
  JStringLocalRef str(m_jniContext, jsonCStr);
  JS_FreeCString(m_ctx, jsonCStr);
  JS_FreeValue(m_ctx, jsonValue);

  JniLocalRef<jobject> localRef = getJniCache()->newJsonObjectWrapper(str);
  return JValue(localRef);
}

JSValue JsonObjectWrapper::fromJava(const JValue &value) const {

  const JniLocalRef<jobject> &jWrapper = value.getLocalRef();
  if (jWrapper.isNull()) {
    return JS_NULL;
  }

  JStringLocalRef strRef = getJniCache()->getJsonObjectWrapperString(jWrapper);

  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  const char *str = strRef.toUtf8Chars();

  // Undefined values are returned as an empty string
  if (!str || strlen(str) == 0) {
    return JS_UNDEFINED;
  }

  JSValue decodedValue = JS_ParseJSON(m_ctx, str, strlen(str), "JsonObjectWrapper.cpp");

  if (JS_IsException(decodedValue)) {
    JS_GetException(m_ctx);
    throw std::invalid_argument(std::string("Error while reading JsonObjectWrapper value (\"") + str + "\")");
  }

  strRef.release();
  return decodedValue;
}

#endif

}  // namespace JavaTypes

