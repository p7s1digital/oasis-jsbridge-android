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
#include "JsValue.h"

#include "JniCache.h"
#include "JsBridgeContext.h"
#include "exceptions/JniException.h"
#include "jni-helpers/JniContext.h"
#include "jni-helpers/JStringLocalRef.h"

namespace {
  const char *JSVALUE_GLOBAL_NAME_PREFIX = "javaTypes_jsValue_";
}


namespace JavaTypes {

JsValue::JsValue(const JsBridgeContext *jsBridgeContext, bool isNullable)
 : JavaType(jsBridgeContext, JavaTypeId::JsValue)
 , m_isNullable(isNullable) {
}

#if defined(DUKTAPE)

#include "StackChecker.h"

// JS to native JsValue
JValue JsValue::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  JNIEnv *env = m_jniContext->getJNIEnv();
  assert(env != nullptr);

  // If the Java JsValue instance is nullable and the JS value is null or undefined, return a null JsValue
  // (but if the JsValue is not nullable, we still return a new JsValue instance)
  if (m_isNullable && duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }

  static int jsValueCount = 0;
  std::string jsValueGlobalName = JSVALUE_GLOBAL_NAME_PREFIX + std::to_string(++jsValueCount);

  // Create a new JS value with a new global name
  JStringLocalRef jsValueName(m_jniContext, jsValueGlobalName.c_str());
  JniLocalRef<jobject> jsValue = getJniCache()->newJsValue(jsValueName);
  jsValueName.release();

  // Set value
  duk_put_global_string(m_ctx, jsValueGlobalName.c_str());
  return JValue(jsValue);
}

// Native JsValue to JS
duk_ret_t JsValue::push(const JValue &value) const {
  CHECK_STACK_OFFSET(m_ctx, 1);

  const JniLocalRef<jobject> &jValue = value.getLocalRef();

  if (jValue.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  // Get JsValue JS name from Java
  JStringLocalRef jsValueName = getJniCache()->getJsValueName(jValue);
  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  // Push the global JS value with that name
  duk_get_global_string(m_ctx, jsValueName.toUtf8Chars());
  return 1;
}

#elif defined(QUICKJS)

JValue JsValue::toJava(JSValueConst v) const {
  JNIEnv *env = m_jniContext->getJNIEnv();
  assert(env != nullptr);

  // If the Java JsValue instance is nullable and the JS value is null or undefined, return a null JsValue
  // (but if the JsValue is not nullable, we still return a new JsValue instance)
  if (m_isNullable && (JS_IsNull(v) || JS_IsUndefined(v))) {
    return JValue();
  }

  static int jsValueCount = 0;
  std::string jsValueGlobalName = JSVALUE_GLOBAL_NAME_PREFIX + std::to_string(++jsValueCount);

  JniLocalRef<jclass> javaClass = getJavaClass();

  // Create a new JS value with a new global name
  JStringLocalRef jsValueName(m_jniContext, jsValueGlobalName.c_str());
  JniLocalRef<jobject> jsValue = getJniCache()->newJsValue(jsValueName);
  jsValueName.release();

  // Set value
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JS_SetPropertyStr(m_ctx, globalObj, jsValueGlobalName.c_str(), JS_DupValue(m_ctx, v));
  JS_FreeValue(m_ctx, globalObj);
  return JValue(jsValue);
}

JSValue JsValue::fromJava(const JValue &value) const {
  const JniLocalRef<jobject> &jValue = value.getLocalRef();

  if (jValue.isNull()) {
    return JS_NULL;
  }

  // Get JsValue JS name from Java
  std::string jsValueName = getJniCache()->getJsValueName(jValue).toStdString();
  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  // Get the global JS value with that name
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSValue ret = JS_GetPropertyStr(m_ctx, globalObj, jsValueName.c_str());
  JS_FreeValue(m_ctx, globalObj);
  return ret;
}

#endif

}  // namespace JavaTypes

