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

JsonObjectWrapper::JsonObjectWrapper(const JsBridgeContext *jsBridgeContext, const JniGlobalRef <jclass> &classRef)
  : JavaType(jsBridgeContext, classRef) {
}

JValue JsonObjectWrapper::toJava(JSValueConst v, bool inScript, const AdditionalData *) const {
  if (!inScript && !JS_IsObject(v) && !JS_IsNull(v)) {
    const auto message = "Cannot convert return value to Object";
    throw std::invalid_argument(message);
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

  JniLocalRef<jclass> javaClass = m_jniContext->findClass("de/prosiebensat1digital/oasisjsbridge/JsonObjectWrapper");
  jmethodID method = m_jniContext->getMethodID(javaClass, "<init>", "(Ljava/lang/String;)V");
  JniLocalRef<jobject> localRef = m_jniContext->newObject<jobject>(javaClass, method, str);
  javaClass.release();

  return JValue(localRef);
}

JSValue JsonObjectWrapper::fromJava(const JValue &value, bool inScript, const AdditionalData *) const {

  const JniLocalRef<jobject> &jWrapper = value.getLocalRef();
  if (jWrapper.isNull()) {
    return JS_NULL;
  }

  JniLocalRef<jclass> javaClass = m_jniContext->findClass("de/prosiebensat1digital/oasisjsbridge/JsonObjectWrapper");
  jmethodID method = m_jniContext->getMethodID(javaClass, "getJsonString", "()Ljava/lang/String;");
  JStringLocalRef str(m_jniContext->callObjectMethod<jstring>(jWrapper, method));
  javaClass.release();

  m_jsBridgeContext->checkRethrowJsError();

  // Undefined values are returned as an empty string
  if (strlen(str.c_str()) == 0) {
    return JS_UNDEFINED;
  }

  JSValue decodedValue = JS_ParseJSON(m_ctx, str.c_str(), str.length(), "JsonObjectWrapper.cpp");
  str.release();

  if (JS_IsException(decodedValue)) {
    alog_warn("Error while reading JsonObjectWrapper value");
    JS_FreeValue(m_ctx, decodedValue);
    if (!inScript) {
      throw std::invalid_argument("Could not decode JsonObjectWrapper");
    }
    return JS_UNDEFINED;
  }

  return decodedValue;
}

}  // namespace JavaTypes

