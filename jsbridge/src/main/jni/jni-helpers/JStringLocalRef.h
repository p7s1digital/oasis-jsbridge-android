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
#include <cstring>
#include <string>

// Same as LocalRef<jstring> with additional conversion from/to native string
class JStringLocalRef : public JniLocalRef<jstring> {
public:
  JStringLocalRef()
      : JniLocalRef() {
  }

  // From Java String
  JStringLocalRef(const JniContext *jniContext, jstring o, Mode mode = Mode::AutoReleased)
      : JniLocalRef<jstring>(jniContext, o, mode) {
  }

  // From null-terminated UTF-8 string
  JStringLocalRef(const JniContext *jniContext, const char *s)
      : JniLocalRef<jstring>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewStringUTF(s)) {
  }

  // From UTF-16 string view
  JStringLocalRef(const JniContext *jniContext, std::u16string_view sv)
      : JniLocalRef<jstring>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewString(
          reinterpret_cast<const jchar *>(sv.data()), sv.length())) {
  }

  // From UTF-16 string
  JStringLocalRef(const JniContext *jniContext, const jchar *s, jsize len)
      : JniLocalRef<jstring>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewString(s, len)) {
  }

  explicit JStringLocalRef(const JniLocalRef<jstring> &localRef)
      : JniLocalRef<jstring>(localRef) {
  }

  explicit JStringLocalRef(JniLocalRef<jstring> &&localRef)
      : JniLocalRef<jstring>(std::forward<JniLocalRef<jstring>>(localRef)) {
  }

  JStringLocalRef(const JStringLocalRef &other)
      : JniLocalRef(other) {
  }

  ~JStringLocalRef() {
    releaseChars();
  }

  // Explicitly disable a dangerous cast to bool
  operator bool() const = delete;

  void release() {
    releaseChars();
    JniLocalRef::release();
  }

  void releaseChars() const {
    if (m_utf8Chars != nullptr) {
      getJniEnv()->ReleaseStringUTFChars(jstr(), m_utf8Chars);
      m_utf8Chars = nullptr;
    }

    if (m_utf16Chars != nullptr) {
      getJniEnv()->ReleaseStringChars(jstr(), reinterpret_cast<const jchar *>(m_utf16Chars));
      m_utf16Chars = nullptr;
    }
  }

  // Return a pointer to a new null-terminated UTF-8 string converted from the UTF-16 Java string
  // WARNING: the returned const char * is invalid after the JStringLocalRef instance has been released!
  // Note: when called multiple times, only 1 Java string -> UTF8 string conversion will be done
  const char *toUtf8Chars() const {
    if (jstr() == nullptr) {
      return nullptr;
    }

    if (m_utf8Chars == nullptr) {
      m_utf8Chars = getJniEnv()->GetStringUTFChars(jstr(), nullptr);
    }

    return m_utf8Chars;
  }

  // Return a string_view to the Java UTF-16 string. This is indeed a direct pointer to the Java String
  // (which is *not* null-terminated)
  // WARNING: the returned string_view is invalid after the JStringLocalRef instance has been released!
  std::u16string_view getUtf16View() const {
    if (jstr() == nullptr) {
      return std::u16string_view();
    }

    if (m_utf16Chars == nullptr) {
      m_utf16Chars = reinterpret_cast<const char16_t *>(getJniEnv()->GetStringChars(jstr(), nullptr));
    }

    return std::u16string_view(m_utf16Chars, static_cast<size_t>(utf16Length()));
  }

  size_t utf8Length() const {
     return m_utf8Chars ? strlen(m_utf8Chars) :
           jstr() ? getJniEnv()->GetStringUTFLength(jstr()) : 0;
  }

  jsize utf16Length() const {
    return jstr() ? getJniEnv()->GetStringLength(jstr()) : 0;
  }

  jstring jstr() const {
    return static_cast<jstring>(get());
  }

private:
  mutable const char *m_utf8Chars = nullptr;  // null-terminated
  mutable const char16_t *m_utf16Chars = nullptr;  // *not* null-terminated
};

#endif
