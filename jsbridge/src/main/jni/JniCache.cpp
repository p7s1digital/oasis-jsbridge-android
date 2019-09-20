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
#include "JniCache.h"

#include "JsBridgeContext.h"
#include "jni-helpers/JniContext.h"

JniCache::JniCache(const JsBridgeContext *jsBridgeContext, const JniLocalRef<jobject> &jsBridgeJavaObject)
 : m_jsBridgeContext(jsBridgeContext)
 , m_jniContext(m_jsBridgeContext->getJniContext())
 , m_jsBridgeClass(JniGlobalRef<jclass>(m_jniContext->findClass(JSBRIDGE_PKG_PATH "/JsBridge")))
 , m_jsBridgeInterface(this, jsBridgeJavaObject)
 , m_objectClass(JniGlobalRef<jclass>(m_jniContext->findClass("java/lang/Object")))
 , m_illegalArgumentExceptionClass(JniGlobalRef<jclass>(m_jniContext->findClass("java/lang/IllegalArgumentException")))
 , m_jsExceptionClass(JniGlobalRef<jclass>(m_jniContext->findClass(JSBRIDGE_PKG_PATH "/JsException")))
 , m_jsBridgeMethodClass(JniGlobalRef<jclass>(m_jniContext->findClass(JSBRIDGE_PKG_PATH "/Method")))
 , m_jsBridgeParameterClass(JniGlobalRef<jclass>(m_jniContext->findClass(JSBRIDGE_PKG_PATH "/Parameter")))
 , m_jsBridgeJsValueClass(getJavaClass(JavaTypeId::JsValue))
 , m_jsonObjectWrapperClass(getJavaClass(JavaTypeId::JsonObjectWrapper)) {
}

const JniGlobalRef<jclass> &JniCache::getJavaClass(JavaTypeId id) const {
  auto itFind = m_javaClasses.find(id);
  if (itFind != m_javaClasses.end()) {
    return itFind->second;
  }

  const JniContext *jniContext = m_jsBridgeContext->getJniContext();
  assert(jniContext != nullptr);

  const char *javaName = getJavaNameByJavaTypeId(id).c_str();
  JniLocalRef<jclass> javaClass = jniContext->findClass(javaName);

  // If the above findClass() call throws an exception, try to get the class from the primitive type
  if (jniContext->exceptionCheck()) {
    jniContext->exceptionClear();
    JniLocalRef<jclass> classClass = jniContext->findClass("java/lang/Class");
    jmethodID getPrimitiveClass = jniContext->getStaticMethodID(classClass, "getPrimitiveClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    javaClass = jniContext->callStaticObjectMethod<jclass>(classClass, getPrimitiveClass, JStringLocalRef(jniContext, javaName));
  }
  return m_javaClasses.emplace(id, JniGlobalRef<jclass>(javaClass)).first->second;
}

JStringLocalRef JniCache::getJavaReflectedMethodName(const JniLocalRef<jobject> &javaMethod) const {
  static thread_local jmethodID methodId = m_jniContext->getMethodID(m_jniContext->getObjectClass(javaMethod), "getName", "()Ljava/lang/String;");
  return m_jniContext->callStringMethod(javaMethod, methodId);
}

JniLocalRef<jthrowable> JniCache::newJsException(
    const JStringLocalRef &jsonValue, const JStringLocalRef &detailedMessage,
    const JStringLocalRef &jsStackTrace, const JniRef<jthrowable> &cause) const {

  static thread_local jmethodID methodId = m_jniContext->getMethodID(
      m_jsExceptionClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)V");

  return m_jniContext->newObject<jthrowable>(m_jsExceptionClass, methodId, jsonValue, detailedMessage, jsStackTrace, cause);
}


// JsValue
// ---

JniLocalRef<jobject> JniCache::newJsValue(const JStringLocalRef &name) const {
  static thread_local jmethodID methodId = m_jniContext->getMethodID(m_jsBridgeJsValueClass, "<init>", "(L" JSBRIDGE_PKG_PATH "/JsBridge;Ljava/lang/String;)V");
  return m_jniContext->newObject<jobject>(m_jsBridgeJsValueClass, methodId, m_jsBridgeInterface.object(), name);
}

JStringLocalRef JniCache::getJsValueName(const JniRef<jobject> &jsValue) const {
  static thread_local jmethodID getJsName = m_jniContext->getMethodID(m_jsBridgeJsValueClass, "getAssociatedJsName", "()Ljava/lang/String;");
  return m_jniContext->callStringMethod(jsValue, getJsName);
}


// JsonObjectWrapper
// ---

JniLocalRef<jobject> JniCache::newJsonObjectWrapper(const JStringLocalRef &jsonString) const {
  static thread_local jmethodID methodId = m_jniContext->getMethodID(m_jsonObjectWrapperClass, "<init>", "(Ljava/lang/String;)V");
  return m_jniContext->newObject<jobject>(m_jsonObjectWrapperClass, methodId, jsonString);
}

JStringLocalRef JniCache::getJsonObjectWrapperString(const JniRef<jobject> &jsonObjectWrapper) const {
  static thread_local jmethodID getJsonString = m_jniContext->getMethodID(m_jsonObjectWrapperClass, "getJsonString", "()Ljava/lang/String;");
  return m_jniContext->callStringMethod(jsonObjectWrapper, getJsonString);
}
