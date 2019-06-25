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
#include "JniContext.h"
#include "JStringLocalRef.h"


// Internal

namespace {
  const char *JSBRIDGE_JAVA_CLASS_NAME = "de/prosiebensat1digital/oasisjsbridge/JsBridge";
  const char *JSBRIDGE_METHOD_CLASS_NAME = "de/prosiebensat1digital/oasisjsbridge/Method";
  const char *JSBRIDGE_PARAMETER_CLASS_NAME = "de/prosiebensat1digital/oasisjsbridge/Parameter";
  const char* JAVA_EXCEPTION_PROP_NAME = "\xff\xffjava_exception";
}


// Class methods

JniContext::JniContext(JNIEnv *env, jobject jsBridgeJavaObject)
 : m_jniEnv(env)
 , m_localRefStats(new JniLocalRefStats())
 , m_jsBridgeJavaClass(JniGlobalRef<jclass>(findClass(JSBRIDGE_JAVA_CLASS_NAME)))
 , m_jsBridgeJavaObject(JniGlobalRef<jobject>(JniLocalRef<jobject>(this, jsBridgeJavaObject, true))) {

    m_jsBridgeMethodClass = JniGlobalRef<jclass>(findClass(JSBRIDGE_METHOD_CLASS_NAME));
    m_jsBridgeParameterClass = JniGlobalRef<jclass>(findClass(JSBRIDGE_PARAMETER_CLASS_NAME));
}

JniContext::~JniContext() {
  delete m_localRefStats;
}

jmethodID JniContext::getMethodID(const JniRef<jclass> &clazz, const char *name, const char *sig) const {
  assert(m_jniEnv);
  return m_jniEnv->GetMethodID(clazz.get(), name, sig);
}

jmethodID JniContext::getStaticMethodID(const JniRef<jclass> &clazz, const char *name, const char *sig) const {
  assert(m_jniEnv);
  return m_jniEnv->GetStaticMethodID(clazz.get(), name, sig);
}

jfieldID JniContext::getStaticFieldID(const JniRef<jclass> &clazz, const char *name, const char *sig) const {
  assert(m_jniEnv);
  return m_jniEnv->GetStaticFieldID(clazz.get(), name, sig);
}

JniLocalRef<jclass> JniContext::findClass(const char *name) const {
  return JniLocalRef<jclass>(this, m_jniEnv->FindClass(name));
}

void JniContext::throw_(const JniRef<jthrowable> &throwable) {
  assert(m_jniEnv);
  m_jniEnv->Throw(throwable.get());
}

void JniContext::throwNew(const JniRef<jclass> &clazz, const char *s) {
  assert(m_jniEnv);
  m_jniEnv->ThrowNew(clazz.get(), s);
}

jboolean JniContext::exceptionCheck() const {
  assert(m_jniEnv);
  return m_jniEnv->ExceptionCheck();
}

jthrowable JniContext::exceptionOccurred() const {
  assert(m_jniEnv);
  return m_jniEnv->ExceptionOccurred();
}

void JniContext::exceptionClear() {
  assert(m_jniEnv);
  m_jniEnv->ExceptionClear();
}

