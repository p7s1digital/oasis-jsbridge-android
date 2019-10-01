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
#ifndef JSBRIDGE_JNIREF_H
#define JSBRIDGE_JNIREF_H

#include "JniRefHelper.h"
#include <jni.h>
#include <cassert>

enum class JniRefReleaseMode {
  Auto,  // JNI ref will be released when the JniRef instance has been destroyed
  Never  // JNI ref will never be released (e.g. for local references given to entry JNI functions)
};

class JniContext;

class JniRefBase {
public:
  const JniContext *getJniContext() const { return m_jniContext; }

  bool isNull() const { return m_object == nullptr; }

  // Explicitly disable a dangerous cast to bool
  explicit operator bool() const = delete;

protected:
  JniRefBase(const JniContext *jniContext, jobject object)
      : m_jniContext(jniContext)
      , m_object(object) {
  }

  JNIEnv *getJniEnv() const {
    assert(m_jniContext);

    JNIEnv *env = JniRefHelper::getJNIEnv(m_jniContext);
    assert(env);

    return env;
  }

  const JniContext *m_jniContext;
  jobject m_object;
};

// Common functionalities of JniLocalRef and JniGlobalRef.
// Note: no virtual method to avoid vtable and improve performance
template <class T>
class JniRef : JniRefBase {
public:
  using JniRefBase::getJniContext;
  using JniRefBase::isNull;

  T get() const { return static_cast<T>(m_object); }

protected:
  using JniRefBase::getJniEnv;
  using JniRefBase::m_jniContext;
  using JniRefBase::m_object;

  JniRef(const JniContext *jniContext, jobject object)
   : JniRefBase(jniContext, object) {
  }
};

#endif

