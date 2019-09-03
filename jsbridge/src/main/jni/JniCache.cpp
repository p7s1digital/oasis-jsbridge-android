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

#define JSBRIDGE_PKG_PATH "de/prosiebensat1digital/oasisjsbridge"


JniCache::JniCache(const JsBridgeContext *jsBridgeContext, const JniLocalRef<jobject> &jsBridgeJavaObject)
 : m_jsBridgeContext(jsBridgeContext)
 , m_jniContext(m_jsBridgeContext->jniContext())
 , m_jsBridgeClass(JniGlobalRef<jclass>(m_jniContext->findClass(JSBRIDGE_PKG_PATH "/JsBridge")))
 , m_jsBridge(this, jsBridgeJavaObject)
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

  const JniContext *jniContext = m_jsBridgeContext->jniContext();
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

JStringLocalRef JniCache::getJavaMethodName(const JniLocalRef<jobject> &javaMethod) const {
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


// JniCache::JsBridge
// ---

JniCache::JsBridge::JsBridge(const JniCache *cache, const JniRef<jobject> &object)
 : JavaInterface(cache, cache->m_jsBridgeClass, object) {
}

void JniCache::JsBridge::checkJsThread() const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(m_class, "checkJsThread", "()V");
  p->m_jniContext->callVoidMethod(m_object, methodId);
}

void JniCache::JsBridge::onDebuggerPending() const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(m_class, "onDebuggerPending", "()V");
  p->m_jniContext->callVoidMethod(m_object, methodId);
}

void JniCache::JsBridge::onDebuggerReady() const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(m_class, "onDebuggerReady", "()V");
  p->m_jniContext->callVoidMethod(m_object, methodId);
}

JniLocalRef<jobject> JniCache::JsBridge::createJsLambdaProxy(
    const JStringLocalRef &globalName, const JniRef<jsBridgeMethod> &method) const {

  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(
      m_class, "createJsLambdaProxy",
      "(Ljava/lang/String;Lde/prosiebensat1digital/oasisjsbridge/Method;)Lkotlin/Function;");

  return p->m_jniContext->callObjectMethod(m_object, methodId, globalName, method);
}

void JniCache::JsBridge::consoleLogHelper(const JStringLocalRef &logType, const JStringLocalRef &msg) const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(
      m_class, "consoleLogHelper", "(Ljava/lang/String;Ljava/lang/String;)V");

  p->m_jniContext->callVoidMethod(m_object, methodId, logType, msg);
}

void JniCache::JsBridge::resolveDeferred(const JniRef<jobject> &javaDeferred, const JValue &value) const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(
      m_class, "resolveDeferred",
      "(Lkotlinx/coroutines/CompletableDeferred;Ljava/lang/Object;)V");

  p->m_jniContext->callVoidMethod(m_object, methodId, javaDeferred, value);
}

void JniCache::JsBridge::rejectDeferred(const JniRef<jobject> &javaDeferred, const JValue &exception) const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(
      m_class, "rejectDeferred",
      "(Lkotlinx/coroutines/CompletableDeferred;Lde/prosiebensat1digital/oasisjsbridge/JsException;)V");

  p->m_jniContext->callVoidMethod(m_object, methodId, javaDeferred, exception);
}

JniLocalRef<jobject> JniCache::JsBridge::createCompletableDeferred() const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(
        m_class, "createCompletableDeferred", "()Lkotlinx/coroutines/CompletableDeferred;");
  return p->m_jniContext->callObjectMethod(m_object, methodId);
}

void JniCache::JsBridge::setUpJsPromise(const JStringLocalRef &name, const JniRef<jobject> &deferred) const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(
        m_class, "setUpJsPromise", "(Ljava/lang/String;Lkotlinx/coroutines/Deferred;)V");
  p->m_jniContext->callVoidMethod(m_object, methodId, name, deferred);
}


// Method
// ---

JniCache::MethodInterface::MethodInterface(const JniCache *cache, const JniRef<jsBridgeMethod> &method)
 : JavaInterface(cache, cache->m_jsBridgeMethodClass, method) {
}

JniLocalRef<jobject> JniCache::MethodInterface::getJavaMethod() const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(m_class, "getJavaMethod", "()Ljava/lang/reflect/Method;");
  return p->m_jniContext->callObjectMethod(m_object, methodId);
}

JStringLocalRef JniCache::MethodInterface::getName() const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(m_class, "getName", "()Ljava/lang/String;");
  return p->m_jniContext->callStringMethod(m_object, methodId);
}

JniLocalRef<jobject> JniCache::MethodInterface::callNativeLambda(const JniRef<jobject> &lambda, const JObjectArrayLocalRef &args) const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(
          m_class, "callNativeLambda",
          "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");

  return p->m_jniContext->callObjectMethod(m_object, methodId, lambda, args);
}

JniLocalRef<jsBridgeParameter> JniCache::MethodInterface::getReturnParameter() const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(
        m_class, "getReturnParameter",
        "()Lde/prosiebensat1digital/oasisjsbridge/Parameter;");

  return p->m_jniContext->callObjectMethod<jsBridgeParameter>(m_object, methodId);
}

JObjectArrayLocalRef JniCache::MethodInterface::getParameters() const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(
        m_class, "getParameters", "()[Lde/prosiebensat1digital/oasisjsbridge/Parameter;");

  return JObjectArrayLocalRef(p->m_jniContext->callObjectMethod<jobjectArray>(m_object, methodId));
}

jboolean JniCache::MethodInterface::isVarArgs() const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(m_class, "isVarArgs", "()Z");
  return p->m_jniContext->callBooleanMethod(m_object, methodId);
}

// Parameter
// ---

JniCache::ParameterInterface::ParameterInterface(const JniCache *cache, const JniRef<jsBridgeParameter> &parameter)
 : JavaInterface(cache, cache->m_jsBridgeParameterClass, parameter) {
}


JniLocalRef<jsBridgeMethod> JniCache::ParameterInterface::getInvokeMethod() const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(
      m_class,
      "getInvokeMethod",
      "()Lde/prosiebensat1digital/oasisjsbridge/Method;"
  );

  return p->m_jniContext->callObjectMethod<jsBridgeMethod>(m_object, methodId);
}

JniLocalRef<jobject> JniCache::ParameterInterface::getJava() const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(m_class, "getJava", "()Ljava/lang/Class;");
  return p->m_jniContext->callObjectMethod(m_object, methodId);
}

JStringLocalRef JniCache::ParameterInterface::getJavaName() const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(m_class, "getJavaName", "()Ljava/lang/String;");
  return p->m_jniContext->callStringMethod(m_object, methodId);
}

JniLocalRef<jsBridgeParameter> JniCache::ParameterInterface::getGenericParameter() const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(m_class, "getGenericParameter", "()L" JSBRIDGE_PKG_PATH "/Parameter;");
  return p->m_jniContext->callObjectMethod<jsBridgeParameter>(m_object, methodId);
}

JStringLocalRef JniCache::ParameterInterface::getName() const {
  static thread_local jmethodID methodId = p->m_jniContext->getMethodID(m_class, "getName", "()Ljava/lang/String;");
  return p->m_jniContext->callStringMethod(m_object, methodId);
}


// JsValue
// ---

JniLocalRef<jobject> JniCache::newJsValue(const JStringLocalRef &name) const {
  static thread_local jmethodID methodId = m_jniContext->getMethodID(m_jsBridgeJsValueClass, "<init>", "(L" JSBRIDGE_PKG_PATH "/JsBridge;Ljava/lang/String;)V");
  return m_jniContext->newObject<jobject>(m_jsBridgeJsValueClass, methodId, m_jsBridge.object(), name);
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
