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
#ifndef _JSBRIDGE_GLOBALREF_H
#define _JSBRIDGE_GLOBALREF_H

#include "JniGlobalRef.h"
#include "JniRef.h"
#include "JniRefHelper.h"
#include "JniLocalRef.h"
#include <jni.h>

class JniContext;

enum class JniGlobalRefMode {
  AutoReleased,  // JNI ref will be released when the JniGlobalRef instance has been destroyed
  Leaked,  // JNI ref will never be released (use with care!)
};

// Manages a global reference to a jobject. Copying a JniGlobalRef increments the
// underlying global reference count on the object in the JVM.
template <class T>
class JniGlobalRef: public JniRef<T> {
  using JniRef<T>::m_jniContext;
  using JniRef<T>::m_object;

  typedef JniGlobalRefMode Mode;

public:
  JniGlobalRef()
    : JniRef<T>(nullptr, nullptr) {
  }

  explicit JniGlobalRef(const JniLocalRef<T> &localRef, Mode mode = Mode::AutoReleased)
   : JniRef<T>(localRef.getJniContext(), nullptr) {

    if (!localRef.isNull()) {
      m_object = static_cast<T>(JniRefHelper::getJNIEnv(m_jniContext)->NewGlobalRef(localRef.get()));
      if (mode == Mode::AutoReleased) {
        m_sharedAutoRelease = makeSharedAutoRelease(true);
      }
    }
  }

  JniGlobalRef(JniGlobalRef<T> &&other)
     : JniRef<T>(other.m_jniContext, other.m_object) {

    std::swap(m_sharedAutoRelease, other.m_sharedAutoRelease);
  }

  JniGlobalRef(const JniGlobalRef<T> &other)
    : JniRef<T>(other.isNull() ? nullptr : other.getJniContext(), other.m_object)
    , m_sharedAutoRelease(other.m_sharedAutoRelease) {
  }

  JniGlobalRef &operator=(JniGlobalRef<T> &&other) noexcept {
    m_jniContext = other.m_jniContext;
    m_object = other.m_object;
    m_sharedAutoRelease.reset();
    std::swap(m_sharedAutoRelease, other.m_sharedAutoRelease);

    other.m_object = nullptr;
    return *this;
  }

  JniGlobalRef &operator=(const JniGlobalRef<T> &other) {
    if (other.m_object == m_object) {
      // Same object
      return *this;
    }

    m_jniContext = other.m_jniContext;
    m_object = other.m_object;
    m_sharedAutoRelease = other.m_sharedAutoRelease;

    return *this;
  }

  static void deleteRawGlobalRef(const JniContext *jniContext, jobject object) {
    JNIEnv *env = JniRefHelper::getJNIEnv(jniContext);
    assert(env != nullptr);

    env->DeleteGlobalRef(object);
  }

  static void deleteRawWeakGlobalRef(const JniContext *jniContext, jobject object) {
    JNIEnv *env = JniRefHelper::getJNIEnv(jniContext);
    assert(env != nullptr);

    env->DeleteWeakGlobalRef(object);
  }

  void release() {
    if (m_sharedAutoRelease.get() != nullptr) {
      *m_sharedAutoRelease = true;
      m_sharedAutoRelease.reset();  // delete local ref (when the last shared ref has been deleted)
    }
  }

  void detach() {
    if (m_sharedAutoRelease.get() != nullptr) {
      *m_sharedAutoRelease = false;
      m_sharedAutoRelease.reset();
    }
  }

private:
  std::shared_ptr<bool> makeSharedAutoRelease(bool autoRelease) const {
    if (m_object == nullptr) {
      return std::shared_ptr<bool>();
    }

    // Copy the variables needed to be captured in the lambda
    auto jniContext = m_jniContext;
    auto object = m_object;

    return std::shared_ptr<bool>(new bool(autoRelease), [jniContext, object](bool *pAutoRelease) {
      if (*pAutoRelease) {
        JniRefHelper::getJNIEnv(jniContext)->DeleteGlobalRef(object);
      }
      delete pAutoRelease;
    });
  }

public:
  // Internal
  mutable std::shared_ptr<bool> m_sharedAutoRelease;
};

#endif

