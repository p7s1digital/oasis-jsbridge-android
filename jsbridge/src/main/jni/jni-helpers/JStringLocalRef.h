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
#ifndef _JSBRIDGE_JSTRINGLOCALREF_H
#define _JSBRIDGE_JSTRINGLOCALREF_H

#include "JniLocalRef.h"
#include "JniRefHelper.h"
#include <jni.h>
#include <string>

// Same as LocalRef<jstring> with additional conversion from/to native string
class JStringLocalRef : public JniLocalRef<jstring> {
public:
  JStringLocalRef()
      : JniLocalRef()
      , m_str(nullptr)
      , m_needsReleaseChars(false) {
  }

  JStringLocalRef(const JniContext *jniContext, jstring o, bool fromJniParam = false)
      : JniLocalRef<jstring>(jniContext, o, fromJniParam)
      , m_str(o == nullptr ? nullptr : JniRefHelper::getJNIEnv(jniContext)->GetStringUTFChars(o, nullptr))
      , m_needsReleaseChars(true) {
  }

  JStringLocalRef(const JniContext *jniContext, const char *s)
      : JniLocalRef<jstring>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewStringUTF(s))
      , m_str(s)
      , m_needsReleaseChars(false) {
  }

  explicit JStringLocalRef(const JniLocalRef<jstring> &localRef)
      : JniLocalRef<jstring>(localRef)
      , m_str(localRef.get() ? JniRefHelper::getJNIEnv(localRef.getJniContext())->GetStringUTFChars(localRef.get(), nullptr) : nullptr)
      , m_needsReleaseChars(true) {
  }

  JStringLocalRef(const JStringLocalRef &other)
      : JniLocalRef(other)
      , m_str(other.m_needsReleaseChars ? JniRefHelper::getJNIEnv(getJniContext())->GetStringUTFChars(get(), nullptr) : other.m_str)
      , m_needsReleaseChars(other.m_needsReleaseChars) {
  }

  // Explicitly disable a dangerous cast to bool
  operator bool() const = delete;

  void releaseNow() override {
    if (m_str && m_needsReleaseChars) {
      assert(getJniContext() != nullptr);
      JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
      assert(env != nullptr);
      env->ReleaseStringUTFChars(get(), m_str);
    }

    JniLocalRef<jstring>::releaseNow();
  }

  std::string str() const {
      return m_str;
  }

  const char *c_str() const {
      return m_str;
  }

  size_t length() const {
    return strlen(m_str);
  }

  jstring jstr() const {
    return static_cast<jstring>(get());
  }

private:
  const char *m_str;
  bool m_needsReleaseChars;
};

#endif
