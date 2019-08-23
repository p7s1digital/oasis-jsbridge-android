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
#include "StackChecker.h"
#include "custom_stringify.h"
#include "jni-helpers/JniContext.h"

namespace {
  extern "C" {
    duk_ret_t tryJsonDecode(duk_context *ctx, void *) {
      duk_json_decode(ctx, -1);
      return 1;
    }
  }
}


namespace JavaTypes {

JsonObjectWrapper::JsonObjectWrapper(const JsBridgeContext *jsBridgeContext, const JniGlobalRef <jclass> &classRef)
  : JavaType(jsBridgeContext, classRef) {
}

JValue JsonObjectWrapper::pop(bool inScript, const AdditionalData *) const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (!inScript && !duk_is_object(m_ctx, -1) && !duk_is_null(m_ctx, -1)) {
    const auto message = std::string("Cannot convert return value to Object");
    duk_pop(m_ctx);
    throw std::invalid_argument(message);
  }

  // Check if the caller passed in a null string.
  if (duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }

  duk_require_object_coercible(m_ctx, -1);

  if (custom_stringify(m_ctx, -1) != DUK_EXEC_SUCCESS) {
    alog_warn("Could not stringify object!");
    m_jsBridgeContext->queueJavaExceptionForJsError();
    duk_pop(m_ctx);
    return JValue();
  }

  JStringLocalRef str(m_jniContext, duk_require_string(m_ctx, -1));
  duk_pop(m_ctx);

  JniLocalRef<jclass> javaClass = m_jniContext->findClass("de/prosiebensat1digital/oasisjsbridge/JsonObjectWrapper");
  jmethodID method = m_jniContext->getMethodID(javaClass, "<init>", "(Ljava/lang/String;)V");
  JniLocalRef<jobject> localRef = m_jniContext->newObject<jobject>(javaClass, method, str);
  javaClass.release();

  duk_pop(m_ctx);
  return JValue(localRef);
}

duk_ret_t JsonObjectWrapper::push(const JValue &value, bool inScript, const AdditionalData *) const {
  CHECK_STACK_OFFSET(m_ctx, 1);

  const JniLocalRef<jobject> &jWrapper = value.getLocalRef();
  if (jWrapper.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  JniLocalRef<jclass> javaClass = m_jniContext->findClass("de/prosiebensat1digital/oasisjsbridge/JsonObjectWrapper");
  jmethodID method = m_jniContext->getMethodID(javaClass, "getJsonString", "()Ljava/lang/String;");
  JStringLocalRef str(m_jniContext->callObjectMethod<jstring>(jWrapper, method));
  javaClass.release();

  m_jsBridgeContext->checkRethrowJsError();

  // Undefined values are returned as an empty string
  if (strlen(str.c_str()) == 0) {
    duk_push_undefined(m_ctx);
    return 1;
  }

  duk_push_string(m_ctx, str.c_str());
  str.release();

  if (duk_safe_call(m_ctx, tryJsonDecode, nullptr, 1, 1) != DUK_EXEC_SUCCESS) {
    std::string err = duk_safe_to_string(m_ctx, -1);
    alog_warn("Error while pushing JsonObjectWrapper value: %s", err.c_str());
    duk_pop(m_ctx);
    duk_push_undefined(m_ctx);
    if (!inScript) {
      throw std::invalid_argument(err);
    }
  }

  return 1;
}

}  // namespace JavaTypes

