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
#include "JavaType.h"

#include "JsBridgeContext.h"
#include "QuickJsUtils.h"
#include "java-types/Deferred.h"
#include "java-types/FunctionX.h"
#include "java-types/JsValue.h"
#include "java-types/JsonObjectWrapper.h"
#include "java-types/Object.h"
#include "jni-helpers/JArrayLocalRef.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include "jni-helpers/JStringLocalRef.h"
#include "jni-helpers/JValue.h"
#include "jni-helpers/JniLocalRef.h"
#include <string>
#include <vector>

using namespace JavaTypes;

JavaType::JavaType(const JsBridgeContext *jsBridgeContext, JniGlobalRef<jclass> classRef)
 : m_jsBridgeContext(jsBridgeContext)
 , m_jniContext(jsBridgeContext->jniContext())
 , m_ctx(m_jsBridgeContext->getCContext())
 , m_classRef(std::move(classRef)) {
}

JValue JavaType::toJavaArray(JSValueConst jsValue, bool inScript, const AdditionalData *additionalData) const {
  if (JS_IsNull(jsValue) || JS_IsUndefined(jsValue)) {
    return JValue();
  }

  JSValue lengthValue = JS_GetPropertyStr(m_ctx, jsValue, "length");
  assert(JS_IsNumber(lengthValue));
  uint32_t count = JS_VALUE_GET_INT(lengthValue);
  JS_FreeValue(m_ctx, lengthValue);

  JObjectArrayLocalRef objectArray(m_jniContext, count, m_classRef);

  assert(JS_IsArray(m_ctx, jsValue));
  for (uint32_t i = 0; i < count; ++i) {
    JValue elementValue = toJava(JS_GetPropertyUint32(m_ctx, jsValue, i), inScript, additionalData);
    const JniLocalRef<jobject> &jElement = elementValue.getLocalRef();
    objectArray.setElement(i, jElement);
    m_jsBridgeContext->checkRethrowJsError();
  }

  return JValue(objectArray);
}

JSValue JavaType::fromJavaArray(const JniLocalRef<jarray>& values, bool inScript, const AdditionalData *additionalData) const {
  JObjectArrayLocalRef objectArray(values.staticCast<jobjectArray>());
  const auto size = objectArray.getLength();

  JSValue jsArray = JS_NewArray(m_ctx);

  for (jsize i = 0; i < size; ++i) {
    JniLocalRef<jobject> object = objectArray.getElement(i);
    try {
      JSValue elementValue = fromJava(JValue(object), inScript, additionalData);
      JS_SetPropertyUint32(m_ctx, jsArray, i, elementValue);
    } catch (std::invalid_argument &e) {
      JS_FreeValue(m_ctx, jsArray);
      throw e;
    }
  }

  return jsArray;
}

JniLocalRef<jclass> JavaType::getArrayClass() const {
  return m_jniContext->getObjectClass(JObjectArrayLocalRef(m_jniContext, 0, getClass()));
}

JValue JavaType::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                            const std::vector<JValue> &args) const {

  JniLocalRef<jobject> returnValue = m_jniContext->callObjectMethodA(javaThis, methodId, args);
  m_jsBridgeContext->checkRethrowJsError();

  // Release all values now because they won't be used afterwards
  JValue::releaseAll(args);

  return JValue(returnValue);
}

std::string getName(const JsBridgeContext *jsBridgeContext, const JniRef<jclass> &javaClass) {
  const JniContext *jniContext = jsBridgeContext->jniContext();

  JniLocalRef<jclass> objectClass = jniContext->getObjectClass(javaClass);
  jmethodID method = jniContext->getMethodID(objectClass, "getName", "()Ljava/lang/String;");
  return JStringLocalRef(jniContext->callObjectMethod<jstring>(javaClass, method)).str();
}

