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
#include "log.h"

JniCache::JniCache(const JsBridgeContext *jsBridgeContext, const JniLocalRef<jobject> &jsBridgeJavaObject)
 : m_jsBridgeContext(jsBridgeContext)
 , m_jniContext(m_jsBridgeContext->getJniContext())
 , m_objectClass(m_jniContext->findClass("java/lang/Object"))
 , m_arrayListClass(m_jniContext->findClass("java/util/ArrayList"))
 , m_listClass(m_jniContext->findClass("java/util/List"))
 , m_jsBridgeClass(m_jniContext->findClass(JSBRIDGE_PKG_PATH "/JsBridge"))
 , m_jsExceptionClass(m_jniContext->findClass(JSBRIDGE_PKG_PATH "/JsException"))
 , m_illegalArgumentExceptionClass(m_jniContext->findClass("java/lang/IllegalArgumentException"))
 , m_runtimeExceptionClass(m_jniContext->findClass("java/lang/RuntimeException"))
 , m_jsBridgeMethodClass(m_jniContext->findClass(JSBRIDGE_PKG_PATH "/Method"))
 , m_jsBridgeParameterClass(m_jniContext->findClass(JSBRIDGE_PKG_PATH "/Parameter"))
 , m_jsBridgeDebugStringClass(getJavaClass(JavaTypeId::DebugString))
 , m_jsBridgeJsValueClass(getJavaClass(JavaTypeId::JsValue))
 , m_jsonObjectWrapperClass(getJavaClass(JavaTypeId::JsonObjectWrapper))
 , m_jsBridgeInterface(this, jsBridgeJavaObject) {
}

const JniGlobalRef<jclass> &JniCache::getJavaClass(JavaTypeId id) const {
  auto itFind = m_javaClasses.find(id);
  if (itFind != m_javaClasses.end()) {
    return itFind->second;
  }

  const JniContext *jniContext = m_jsBridgeContext->getJniContext();
  assert(jniContext != nullptr);

  const std::string &javaName = getJniClassNameByJavaTypeId(id);

  JniLocalRef<jclass> javaClass = jniContext->findClass(javaName.c_str());

  // If the above findClass() call throws an exception, try to get the class from the primitive type
  if (jniContext->exceptionCheck()) {
    jniContext->exceptionClear();
    JniLocalRef<jclass> classClass = jniContext->findClass("java/lang/Class");
    jmethodID getPrimitiveClass = jniContext->getStaticMethodID(classClass, "getPrimitiveClass", "(Ljava/lang/String;)Ljava/lang/Class;");

    javaClass = jniContext->callStaticObjectMethod<jclass>(classClass, getPrimitiveClass, JStringLocalRef(jniContext, javaName.c_str()));
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


// DebugString
// ---

JniLocalRef<jobject> JniCache::newDebugString(const char *s) const {
  return newDebugString(JStringLocalRef(m_jniContext, s));
}

JniLocalRef<jobject> JniCache::newDebugString(const JStringLocalRef &s) const {
  static thread_local jmethodID methodId = m_jniContext->getMethodID(m_jsBridgeDebugStringClass, "<init>", "(Ljava/lang/String;)V");
  return m_jniContext->newObject<jobject>(m_jsBridgeDebugStringClass, methodId, s);
}

JStringLocalRef JniCache::getDebugStringString(const JniRef<jobject> &debugString) const {
  static thread_local jmethodID getString = m_jniContext->getMethodID(m_jsBridgeDebugStringClass, "getString", "()Ljava/lang/String;");
  return m_jniContext->callStringMethod(debugString, getString);
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

// List
// ---

JniLocalRef<jobject> JniCache::newList() const {
  static thread_local jmethodID methodId = m_jniContext->getMethodID(m_arrayListClass, "<init>", "()V");
  return m_jniContext->newObject<jobject>(m_arrayListClass, methodId);
}

void JniCache::addToList(const JniLocalRef<jobject> &list, const JniLocalRef<jobject> &element) const {
  static thread_local jmethodID methodId = m_jniContext->getMethodID(m_listClass, "add", "(Ljava/lang/Object;)Z");
  m_jniContext->callBooleanMethod(list, methodId, element);
}

int JniCache::getListLength(const JniLocalRef<jobject> &list) const {
  static thread_local jmethodID methodId = m_jniContext->getMethodID(m_listClass, "size", "()I");
  return m_jniContext->callIntMethod(list, methodId);
}

JniLocalRef<jobject> JniCache::getListElement(const JniLocalRef<jobject> &list, int i) const {
  static thread_local jmethodID methodId = m_jniContext->getMethodID(m_listClass, "get", "(I)Ljava/lang/Object;");
  return m_jniContext->callObjectMethod(list, methodId, i);
}


// Parameter
// ---

JniLocalRef<jsBridgeParameter> JniCache::newParameter(const JniLocalRef<jclass> &javaClass) const {
  static thread_local jmethodID getCustomClassLoader = m_jniContext->getMethodID(m_jsBridgeClass, "getCustomClassLoader", "()Ljava/lang/ClassLoader;");
  const auto bridgeCustomClassLoader = m_jniContext->callObjectMethod(m_jsBridgeInterface.object(), getCustomClassLoader);

  static thread_local jmethodID parameterInit = m_jniContext->getMethodID(m_jsBridgeParameterClass, "<init>", "(Ljava/lang/Class;Ljava/lang/ClassLoader;)V");
  return m_jniContext->newObject<jsBridgeParameter>(m_jsBridgeParameterClass, parameterInit, javaClass, bridgeCustomClassLoader);
}
