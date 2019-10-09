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
#ifndef _JSBRIDGE_LOCALREF_H
#define _JSBRIDGE_LOCALREF_H

#include "JniRef.h"
#include "JniRefHelper.h"
#include <jni.h>
#include <memory>
#include <optional>
#include <type_traits>
#include <cassert>

class JniContext;

#ifndef NDEBUG
 #define CHECK_ENV
#endif

enum class JniLocalRefMode {
  AutoReleased,  // JNI ref will be released when the JniLocalRef instance has been destroyed
  NewLocalRef,  // initial JNI ref will be copied via JNIEnv::NewLocalRef() and the copy will be auto-released
  Borrowed,  // JNI ref will never be released (e.g. for local references given to entry JNI functions)
};

// Wrapper around JNI local references using the RAII idiom for clearing resource.
// It has been designed to limit the number of local references, even when copying instances.
// Compared to a "raw" local references, it has the (small) overhead of storing the JNI environment
// and has a shared pointer to manage shared references
template <class T>
class JniLocalRef : public JniRef<T> {

protected:
  typedef JniLocalRefMode Mode;

public:
  JniLocalRef()
    : JniRef<T>(nullptr, nullptr) {
  }

  JniLocalRef(const JniContext *jniContext, jobject o, Mode mode = Mode::AutoReleased)
    : JniRef<T>(jniContext, static_cast<T>(mode == Mode::NewLocalRef ? copyRawLocalRef(jniContext, o) : o)) {

    if (mode != Mode::Borrowed) {
      // TODO(bwa): we could theoritically avoid 1 heap allocation for JniLocalRefs which are
      // referenced only once and create the shared pointer only when creating the 2nd copy
      m_sharedAutoRelease = makeSharedAutoRelease(true);
    }
  }

  JniLocalRef(JniLocalRef &&other)
    : JniRef<T>(other.m_jniContext, other.m_object) {
    std::swap(m_sharedAutoRelease, other.m_sharedAutoRelease);
  }

  JniLocalRef(const JniLocalRef &other)
    : JniRef<T>(other.m_jniContext, other.m_object)
    , m_sharedAutoRelease(other.m_sharedAutoRelease) {
  }

  JniLocalRef(const JniRef<T> &other)
    : JniLocalRef(
        other.getJniContext(),
        other.isNull() ? nullptr : JniRefHelper::getJNIEnv(other.getJniContext())->NewLocalRef(other.get())
  ) {
  }

  JniLocalRef &operator=(JniLocalRef &&other) noexcept {
    m_jniContext = other.m_jniContext;
    m_object = other.m_object;
    m_sharedAutoRelease.reset();
    std::swap(m_sharedAutoRelease, other.m_sharedAutoRelease);

    other.m_object = nullptr;
    return *this;
  }

  JniLocalRef &operator=(const JniLocalRef &other) {
    if (other.m_object == m_object) {
      // Same object
      return *this;
    }

    m_jniContext = other.m_jniContext;
    m_object = other.m_object;
    m_sharedAutoRelease = other.m_sharedAutoRelease;

    return *this;
  }

  void reset() {
    m_sharedAutoRelease.reset();
    m_jniContext = nullptr;
    m_object = nullptr;
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

  void swap(JniLocalRef &other) {
    std::swap(m_jniContext, other.m_jniContext);
    std::swap(m_object, other.m_object);
    std::swap(m_sharedAutoRelease, other.m_sharedAutoRelease);
  }

  // JniLocalRef<T2> staticCast() const  WHEN T2 == T
  template <class T2, class Q = T>
  typename std::enable_if<std::is_same<Q, T2>::value, JniLocalRef<T2>>::type
  staticCast() const {
    return *this;
  }

  // JniLocalRef<T2> staticCast() const  WHEN T2 != T
  template <class T2, class Q = T>
  typename std::enable_if<!std::is_same<Q, T2>::value, JniLocalRef<T2>>::type
  staticCast() const {
    // Create a new local ref, initially disabling auto-release
    auto ret = JniLocalRef<T2>(m_jniContext, static_cast<T2>(m_object), Mode::Borrowed);

    // Now, share ownership of the new local ref with this
    ret.m_sharedAutoRelease = this->m_sharedAutoRelease;

    return ret;
  }

protected:
  using JniRef<T>::m_jniContext;
  using JniRef<T>::m_object;

private:
  friend class JValue;

  static jobject copyRawLocalRef(const JniContext *jniContext, jobject src) {
    if (src == nullptr) {
      return nullptr;
    }

    assert(jniContext != nullptr);
    return JniRefHelper::getJNIEnv(jniContext)->NewLocalRef(src);
  }

  std::shared_ptr<bool> makeSharedAutoRelease(bool autoRelease) {
    if (m_object == nullptr) {
      return std::shared_ptr<bool>();
    }

    assert(m_jniContext);

    // Copy the variables needed to be captured in the lambda
    JNIEnv *jniEnv = JniRefHelper::getJNIEnv(m_jniContext);
    jobject object = m_object;
#ifdef CHECK_ENV
    const JniContext *jniContext = m_jniContext;
#endif

    return std::shared_ptr<bool>(new bool(autoRelease),
#ifdef CHECK_ENV
        [jniEnv, object, jniContext]
#else
        [jniEnv, object]
#endif
        (bool *pAutoRelease) {

#ifdef CHECK_ENV
      // Check that the environments has not changed since the creation
      assert(jniContext);
      JNIEnv *currentJniEnv = JniRefHelper::getJNIEnv(jniContext);
      assert(currentJniEnv == jniEnv);
#endif

      if (*pAutoRelease) {
        jniEnv->DeleteLocalRef(object);
      }
      delete pAutoRelease;
    });
  }

public:
  // Internal
  mutable std::shared_ptr<bool> m_sharedAutoRelease;
};

#endif
