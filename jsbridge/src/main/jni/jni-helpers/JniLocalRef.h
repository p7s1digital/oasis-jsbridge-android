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
#include "RefCounter.h"
#include <jni.h>
#include <type_traits>
#include <cassert>

class JniContext;

// Wrapper around JNI local references using the RAII idiom for clearing resource.
template <class T>
class JniLocalRef : public JniRef<T> {

public:
  JniLocalRef()
    : m_refCounter(nullptr)
    , m_jniContext(nullptr)
    , m_object(nullptr) {
  }

  JniLocalRef(const JniContext *jniContext, jobject o, bool fromJniParam = false)
    : m_refCounter(o == nullptr ? nullptr : new RefCounter(1))
    , m_jniContext(jniContext)
    , m_object(static_cast<T>(o))
    , m_fromJniParam(fromJniParam) {

    if (!m_fromJniParam && m_object != nullptr) {
      assert(m_jniContext != nullptr);
    }
  }

  JniLocalRef(JniLocalRef &&other)
    : m_refCounter(other.m_refCounter)
    , m_jniContext(other.m_jniContext)
    , m_object(other.m_object)
    , m_fromJniParam(other.m_fromJniParam) {

    other.m_refCounter = nullptr;  // make sure that the old instance does not trigger delete
  }

  JniLocalRef(const JniLocalRef &other)
    : m_refCounter(other.m_refCounter)
    , m_jniContext(other.m_jniContext)
    , m_object(other.m_object)
    , m_fromJniParam(other.m_fromJniParam) {

    if (m_refCounter != nullptr) {
      m_refCounter->increment();
    }
  }

  JniLocalRef(const JniRef<T> &other)
    : JniLocalRef(
        other.getJniContext(),
        other.isNull() ? nullptr : JniRefHelper::getJNIEnv(other.getJniContext())->NewLocalRef(other.get())
  ) {
  }

  JniLocalRef &operator=(JniLocalRef &&other) noexcept {
    if (other.m_object == m_object) {
      // Same object
      return *this;
    }

    m_jniContext = other.m_jniContext;
    m_object = other.m_object;
    m_fromJniParam = other.m_fromJniParam;
    m_refCounter = other.m_refCounter;

    other.m_refCounter = nullptr;
    return *this;
  }

  JniLocalRef &operator=(const JniLocalRef &other) {
    if (other.m_object == m_object) {
      // Same object
      return *this;
    }

    releaseIfNeeded();

    m_jniContext = other.m_jniContext;
    m_object = other.m_object;
    m_fromJniParam = other.m_fromJniParam;
    m_refCounter = other.m_refCounter;

    if (m_refCounter != nullptr) {
      m_refCounter->increment();
    }

    return *this;
  }

  virtual ~JniLocalRef() {
    releaseIfNeeded();
  }

  static JniLocalRef<T> fromRawGlobalRef(const JniContext *jniContext, T o) {
    JNIEnv *env = JniRefHelper::getJNIEnv(jniContext);
    assert(env != nullptr);

    return JniLocalRef<T>(jniContext, env->NewLocalRef(o));
  }

  bool isNull() const override {
    return m_object == nullptr;
  }

  //operator T () const override {
  //  return m_object;
  //}

  T get() const override {
    return m_object;
  }

  T toNewRawLocalRef() const override {
    JNIEnv *env = JniRefHelper::getJNIEnv(m_jniContext);
    assert(env != nullptr);

    return m_object == nullptr ? nullptr : static_cast<T>(env->NewLocalRef(m_object));
  }

  T toNewRawGlobalRef() const override {
    JNIEnv *env = JniRefHelper::getJNIEnv(m_jniContext);
    assert(env != nullptr);

    return m_object == nullptr ? nullptr : static_cast<T>(env->NewGlobalRef(m_object));
  }

  T toNewRawWeakGlobalRef() const override {
    JNIEnv *env = JniRefHelper::getJNIEnv(m_jniContext);
    assert(env != nullptr);

    return m_object == nullptr ? nullptr : static_cast<T>(env->NewWeakGlobalRef(m_object));
  }

  void release() {
    releaseIfNeeded();
  }

  void detach() {
    if (m_refCounter) {
      m_refCounter->disable();  // make sure that other instance with same counter don't get destroyed
    }

    delete m_refCounter;
    m_refCounter = nullptr;
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
    return m_object == nullptr ? JniLocalRef<T2>() : JniLocalRef<T2>(m_jniContext, JniRefHelper::getJNIEnv(m_jniContext)->NewLocalRef(m_object));
  }

  const JniContext *getJniContext() const override { return m_jniContext; }

  // Internal
  RefCounter *m_refCounter;

protected:
  friend class JValue;

  virtual void releaseNow() {
    if (m_object != nullptr && !m_fromJniParam) {
      JniRefHelper::getJNIEnv(m_jniContext)->DeleteLocalRef(m_object);
      //m_object = nullptr;
    }

    delete m_refCounter;
    m_refCounter = nullptr;
  }

private:
  void releaseIfNeeded() {
    if (m_refCounter == nullptr) {
      return;
    }

    m_refCounter->decrement();
    if (m_refCounter->isZero()) {
      releaseNow();
    }
  }

  const JniContext *m_jniContext;
  T m_object;
  bool m_fromJniParam = false;
};

#endif
