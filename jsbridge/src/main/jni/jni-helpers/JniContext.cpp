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

JniContext::JniContext(JNIEnv *env, EnvironmentSource jniEnvSetup)
 : m_currentJniEnv(jniEnvSetup == EnvironmentSource::JvmAuto ? nullptr : env)
 , m_jniEnvSetup(jniEnvSetup) {

  env->GetJavaVM(&m_jvm);
  assert(m_jvm != nullptr);
}

JniContext::~JniContext() {
}

jmethodID JniContext::getMethodID(const JniRef<jclass> &clazz, const char *name, const char *sig) const {
  JNIEnv *env = getJNIEnv();
  return env->GetMethodID(clazz.get(), name, sig);
}

jmethodID JniContext::getStaticMethodID(const JniRef<jclass> &clazz, const char *name, const char *sig) const {
  JNIEnv *env = getJNIEnv();
  return env->GetStaticMethodID(clazz.get(), name, sig);
}

jfieldID JniContext::getStaticFieldID(const JniRef<jclass> &clazz, const char *name, const char *sig) const {
  JNIEnv *env = getJNIEnv();
  return env->GetStaticFieldID(clazz.get(), name, sig);
}

JniLocalRef<jclass> JniContext::findClass(const char *name) const {
  JNIEnv *env = getJNIEnv();
  return JniLocalRef<jclass>(this, env->FindClass(name));
}

void JniContext::throw_(const JniRef<jthrowable> &throwable) const {
  JNIEnv *env = getJNIEnv();
  env->Throw(throwable.get());
}

void JniContext::throwNew(const JniRef<jclass> &clazz, const char *s) const {
  JNIEnv *env = getJNIEnv();
  env->ThrowNew(clazz.get(), s);
}

jboolean JniContext::exceptionCheck() const {
  JNIEnv *env = getJNIEnv();
  return env->ExceptionCheck();
}

jthrowable JniContext::exceptionOccurred() const {
  JNIEnv *env = getJNIEnv();
  return env->ExceptionOccurred();
}

void JniContext::exceptionClear() const {
  JNIEnv *env = getJNIEnv();
  env->ExceptionClear();
}

static JNIEnv *getEnvFromJvm(JavaVM *jvm) {
  JNIEnv *jvmEnv;

  jvm->AttachCurrentThread(&jvmEnv, nullptr);
  return jvmEnv;
}

JNIEnv *JniContext::getJNIEnv() const {
  assert(this);
  JNIEnv *env;

  switch (m_jniEnvSetup) {
    case EnvironmentSource::JvmAuto:
      env = getEnvFromJvm(m_jvm);
      break;

    case EnvironmentSource::Manual:
      env = m_currentJniEnv;
#ifndef NDEBUG
      assert(env == getEnvFromJvm(m_jvm));
#endif
      break;
  }

  assert(env);
  return env;
}

void JniContext::setCurrentJNIEnv(JNIEnv *env) {
  assert(m_jniEnvSetup == EnvironmentSource::Manual);
  m_currentJniEnv = env;
}

