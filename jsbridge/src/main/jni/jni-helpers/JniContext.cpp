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

#define JSBRIDGE_PKG_PATH "de/prosiebensat1digital/oasisjsbridge"

JniContext::JniContext(JNIEnv *env)
 : m_jniEnv(env) {
}

JniContext::~JniContext() {
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

void JniContext::throw_(const JniRef<jthrowable> &throwable) const {
  assert(m_jniEnv);
  m_jniEnv->Throw(throwable.get());
}

void JniContext::throwNew(const JniRef<jclass> &clazz, const char *s) const {
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

void JniContext::exceptionClear() const {
  assert(m_jniEnv);
  m_jniEnv->ExceptionClear();
}
