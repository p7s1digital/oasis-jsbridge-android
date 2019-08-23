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

#include "jni-helpers/JniContext.h"
#include "jni-helpers/JStringLocalRef.h"
#include "../JsBridgeContext.h"

namespace {
  const char *JSVALUE_GLOBAL_NAME_PREFIX = "javaTypes_jsValue_";
}


namespace JavaTypes {

JsValue::JsValue(const JsBridgeContext *jsBridgeContext, const JniGlobalRef<jclass> &classRef)
  : JavaType(jsBridgeContext, classRef) {
}

#if defined(DUKTAPE)

#include "StackChecker.h"

// JS to native JsValue
JValue JsValue::pop(bool inScript, const AdditionalData *) const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  JNIEnv *env = m_jniContext->getJNIEnv();
  assert(env != nullptr);

  static int jsValueCount = 0;
  std::string jsValueGlobalName = JSVALUE_GLOBAL_NAME_PREFIX + std::to_string(++jsValueCount);

  // Create a new JS value with a new global name
  const JniRef<jobject> &jsBridgeObject = m_jniContext->getJsBridgeObject();
  JStringLocalRef jsValueName(m_jniContext, jsValueGlobalName.c_str());
  jmethodID newJsValue = m_jniContext->getMethodID(getClass(), "<init>", "(Lde/prosiebensat1digital/oasisjsbridge/JsBridge;Ljava/lang/String;)V");
  JniLocalRef<jobject> jsValue = m_jniContext->newObject<jobject>(getClass(), newJsValue, jsBridgeObject, jsValueName);

  // Set value
  duk_put_global_string(m_ctx, jsValueGlobalName.c_str());
  return JValue(jsValue);
}

// Native JsValue to JS
duk_ret_t JsValue::push(const JValue &value, bool inScript, const AdditionalData *) const {
  CHECK_STACK_OFFSET(m_ctx, 1);

  const JniLocalRef<jobject> &jValue = value.getLocalRef();

  if (jValue.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  // Get JsValue JS name from Java
  jmethodID getJsName = m_jniContext->getMethodID(getClass(), "getAssociatedJsName", "()Ljava/lang/String;");
  JStringLocalRef jsName(m_jniContext->callObjectMethod<jstring>(jValue, getJsName));
    m_jsBridgeContext->checkRethrowJsError();

  // Push the global JS value with that name
  duk_get_global_string(m_ctx, jsName.c_str());
  return 1;
}

#elif defined(QUICKJS)

JValue JsValue::toJava(JSValueConst v, bool inScript, const AdditionalData *) const {
  JNIEnv *env = m_jniContext->getJNIEnv();
  assert(env != nullptr);

  static int jsValueCount = 0;
  std::string jsValueGlobalName = JSVALUE_GLOBAL_NAME_PREFIX + std::to_string(++jsValueCount);

  // Create a new JS value with a new global name
  const JniRef<jobject> &jsBridgeObject = m_jniContext->getJsBridgeObject();
  JStringLocalRef jsValueName(m_jniContext, jsValueGlobalName.c_str());
  jmethodID newJsValue = m_jniContext->getMethodID(getClass(), "<init>", "(Lde/prosiebensat1digital/oasisjsbridge/JsBridge;Ljava/lang/String;)V");
  JniLocalRef<jobject> jsValue = m_jniContext->newObject<jobject>(getClass(), newJsValue, jsBridgeObject, jsValueName);

  // Set value
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JS_SetPropertyStr(m_ctx, globalObj, jsValueGlobalName.c_str(), JS_DupValue(m_ctx, v));
  JS_FreeValue(m_ctx, globalObj);
  return JValue(jsValue);
}

JSValue JsValue::fromJava(const JValue &value, bool inScript, const AdditionalData *) const {
  const JniLocalRef<jobject> &jValue = value.getLocalRef();

  if (jValue.isNull()) {
    return JS_NULL;
  }

  // Get JsValue JS name from Java
  jmethodID getJsName = m_jniContext->getMethodID(getClass(), "getAssociatedJsName", "()Ljava/lang/String;");
  JStringLocalRef jsName(m_jniContext->callObjectMethod<jstring>(jValue, getJsName));
  m_jsBridgeContext->checkRethrowJsError();

  // Get the global JS value with that name
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSValue ret = JS_GetPropertyStr(m_ctx, globalObj, jsName.c_str());
  JS_FreeValue(m_ctx, globalObj);
  return ret;
}

#endif

}  // namespace JavaTypes

