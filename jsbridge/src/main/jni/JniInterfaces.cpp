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
#include "JniInterfaces.h"
#include "JniCache.h"
#include "jni-helpers/JniContext.h"


// JsBridgeInterface
// ---

JsBridgeInterface::JsBridgeInterface(const JniCache *cache, const JniRef<jobject> &object)
 : JniInterface(cache, cache->getJsBridgeClass(), object) {
}

void JsBridgeInterface::checkJsThread() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(m_class, "checkJsThread", "()V");
  m_jniCache->getJniContext()->callVoidMethod(m_object, methodId);
}

void JsBridgeInterface::onDebuggerPending() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(m_class, "onDebuggerPending", "()V");
  m_jniCache->getJniContext()->callVoidMethod(m_object, methodId);
}

void JsBridgeInterface::onDebuggerReady() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(m_class, "onDebuggerReady", "()V");
  m_jniCache->getJniContext()->callVoidMethod(m_object, methodId);
}

JniLocalRef<jobject> JsBridgeInterface::createJsLambdaProxy(
    const JStringLocalRef &globalName, const JniRef<jsBridgeMethod> &method) const {

  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(
      m_class, "createJsLambdaProxy",
      "(Ljava/lang/String;L" JSBRIDGE_PKG_PATH "/Method;)Lkotlin/Function;");

  return m_jniCache->getJniContext()->callObjectMethod(m_object, methodId, globalName, method);
}

void JsBridgeInterface::consoleLogHelper(const JStringLocalRef &logType, const JStringLocalRef &msg) const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(
      m_class, "consoleLogHelper", "(Ljava/lang/String;Ljava/lang/String;)V");

  m_jniCache->getJniContext()->callVoidMethod(m_object, methodId, logType, msg);
}

void JsBridgeInterface::resolveDeferred(const JniRef<jobject> &javaDeferred, const JValue &value) const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(
      m_class, "resolveDeferred",
      "(Lkotlinx/coroutines/CompletableDeferred;Ljava/lang/Object;)V");

  m_jniCache->getJniContext()->callVoidMethod(m_object, methodId, javaDeferred, value);
}

void JsBridgeInterface::rejectDeferred(const JniRef<jobject> &javaDeferred, const JValue &exception) const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(
      m_class, "rejectDeferred",
      "(Lkotlinx/coroutines/CompletableDeferred;L" JSBRIDGE_PKG_PATH "/JsException;)V");

  m_jniCache->getJniContext()->callVoidMethod(m_object, methodId, javaDeferred, exception);
}

JniLocalRef<jobject> JsBridgeInterface::createCompletableDeferred() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(
        m_class, "createCompletableDeferred", "()Lkotlinx/coroutines/CompletableDeferred;");
  return m_jniCache->getJniContext()->callObjectMethod(m_object, methodId);
}

void JsBridgeInterface::setUpJsPromise(const JStringLocalRef &name, const JniRef<jobject> &deferred) const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(
        m_class, "setUpJsPromise", "(Ljava/lang/String;Lkotlinx/coroutines/Deferred;)V");
  m_jniCache->getJniContext()->callVoidMethod(m_object, methodId, name, deferred);
}


// MethodInterface
// ---

MethodInterface::MethodInterface(const JniCache *cache, const JniRef<jsBridgeMethod> &method)
 : JniInterface(cache, cache->getJsBridgeMethodClass(), method) {
}

JniLocalRef<jobject> MethodInterface::getJavaMethod() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(m_class, "getJavaMethod", "()Ljava/lang/reflect/Method;");
  return m_jniCache->getJniContext()->callObjectMethod(m_object, methodId);
}

JStringLocalRef MethodInterface::getName() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(m_class, "getName", "()Ljava/lang/String;");
  return m_jniCache->getJniContext()->callStringMethod(m_object, methodId);
}

JniLocalRef<jobject> MethodInterface::callNativeLambda(const JniRef<jobject> &lambda, const JObjectArrayLocalRef &args) const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(
          m_class, "callNativeLambda",
          "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");

  return m_jniCache->getJniContext()->callObjectMethod(m_object, methodId, lambda, args);
}

JniLocalRef<jsBridgeParameter> MethodInterface::getReturnParameter() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(
        m_class, "getReturnParameter",
        "()L" JSBRIDGE_PKG_PATH "/Parameter;");

  return m_jniCache->getJniContext()->callObjectMethod<jsBridgeParameter>(m_object, methodId);
}

JObjectArrayLocalRef MethodInterface::getParameters() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(
        m_class, "getParameters", "()[L" JSBRIDGE_PKG_PATH "/Parameter;");

  return JObjectArrayLocalRef(m_jniCache->getJniContext()->callObjectMethod<jobjectArray>(m_object, methodId));
}

jboolean MethodInterface::isVarArgs() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(m_class, "isVarArgs", "()Z");
  return m_jniCache->getJniContext()->callBooleanMethod(m_object, methodId);
}

// ParameterInterface
// ---

ParameterInterface::ParameterInterface(const JniCache *cache, const JniRef<jsBridgeParameter> &parameter)
 : JniInterface(cache, cache->getJsBridgeParameterClass(), parameter) {
}


JniLocalRef<jsBridgeMethod> ParameterInterface::getInvokeMethod() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(
      m_class,
      "getInvokeMethod",
      "()L" JSBRIDGE_PKG_PATH "/Method;"
  );

  return m_jniCache->getJniContext()->callObjectMethod<jsBridgeMethod>(m_object, methodId);
}

JniLocalRef<jobject> ParameterInterface::getJava() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(m_class, "getJava", "()Ljava/lang/Class;");
  return m_jniCache->getJniContext()->callObjectMethod(m_object, methodId);
}

JStringLocalRef ParameterInterface::getJavaName() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(m_class, "getJavaName", "()Ljava/lang/String;");
  return m_jniCache->getJniContext()->callStringMethod(m_object, methodId);
}

jboolean ParameterInterface::isNullable() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(m_class, "isNullable", "()Z");
  return m_jniCache->getJniContext()->callBooleanMethod(m_object, methodId);
}

JniLocalRef<jsBridgeParameter> ParameterInterface::getComponentType() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(m_class, "getComponentType", "()L" JSBRIDGE_PKG_PATH "/Parameter;");
  return m_jniCache->getJniContext()->callObjectMethod<jsBridgeParameter>(m_object, methodId);
}

JniLocalRef<jsBridgeParameter> ParameterInterface::getGenericParameter() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(m_class, "getGenericParameter", "()L" JSBRIDGE_PKG_PATH "/Parameter;");
  return m_jniCache->getJniContext()->callObjectMethod<jsBridgeParameter>(m_object, methodId);
}

JStringLocalRef ParameterInterface::getName() const {
  static thread_local jmethodID methodId = m_jniCache->getJniContext()->getMethodID(m_class, "getName", "()Ljava/lang/String;");
  return m_jniCache->getJniContext()->callStringMethod(m_object, methodId);
}
