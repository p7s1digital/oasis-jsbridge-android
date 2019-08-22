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

// Manages a global reference to a jobject. Copying a JniGlobalRef increments the
// underlying global reference count on the object in the JVM.
template <class T>
class JniGlobalRef: public JniRef<T> {
public:
  JniGlobalRef()
    : m_jniContext(nullptr)
    , m_object(nullptr)
    , m_refCounter(nullptr) {
  }

  explicit JniGlobalRef(const JniRef<T> &other)
    : m_jniContext(other.isNull() ? nullptr : other.getJniContext())
    , m_object(nullptr)
    , m_refCounter(nullptr) {

    if (m_jniContext == nullptr) {
      return;
    }

    auto otherAsGlobalRef = dynamic_cast<const JniGlobalRef *>(&other);

    if (otherAsGlobalRef == nullptr) {
      // Another JniRef instance (probably a JniLocalRef)
      if (!other.isNull()) {
        m_object = static_cast<T>(JniRefHelper::getJNIEnv(m_jniContext)->NewGlobalRef(other.get()));
        m_refCounter = new RefCounter(1);
      }
    } else {
      // Other is already a JniGlobalRef instance, use its JNI object and increment ref counter
      m_object = otherAsGlobalRef->m_object;
      m_refCounter = otherAsGlobalRef->m_refCounter;
      if (m_refCounter != nullptr) {
        m_refCounter->increment();
      }
    }
  }

  JniGlobalRef(JniGlobalRef<T> &&other)
   : m_jniContext(other.m_jniContext)
   , m_object(other.m_object)
   , m_refCounter(other.m_refCounter) {

    // Make sure that the "old" instance does not delete the global ref which is used in the new one
    other.m_object = nullptr;
    other.m_refCounter = nullptr;
  }

  JniGlobalRef(const JniGlobalRef<T> &other)
    : m_jniContext(other.m_jniContext)
    , m_object(other.m_object)
    , m_refCounter(other.m_refCounter) {
    if (m_refCounter != nullptr) {
      m_refCounter->increment();
    }
  }

  JniGlobalRef &operator=(const JniGlobalRef<T> &other) {
    if (&other == this) {
      return *this;
    }

    if (m_object != other.m_object) {
      release();
    }

    m_jniContext = other.getJniContext();
    m_object = other.m_object;
    m_refCounter = other.m_refCounter;
    if (m_refCounter != nullptr) {
      m_refCounter->increment();
    }

    return *this;
  }

  JniGlobalRef &operator=(JniGlobalRef<T> &&other) noexcept {
    if (&other == this) {
      return *this;
    }

    if (m_object != other.m_object) {
      release();
    }

    m_jniContext = other.m_jniContext;
    m_object = other.m_object;
    m_refCounter = other.m_refCounter;

    other.m_jniContext = nullptr;
    other.m_object = nullptr;
    other.m_refCounter = nullptr;

    return *this;
  }

  ~JniGlobalRef() override {
    release();
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

  bool isNull() const override {
    return m_object == nullptr;
  }

  const JniContext *getJniContext() const override { return m_jniContext; }

  //operator T () const override {
  //    return m_object;
  //}

  T get() const override {
    return m_object;
  }

  JniLocalRef<T> toLocalRef() const {
    return JniLocalRef<T>(m_jniContext, toNewRawLocalRef());
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

  void detach() {
    if (m_refCounter) {
      m_refCounter->disable();  // make sure that other instance with same counter don't get destroyed
    }

    delete m_refCounter;
    m_refCounter = nullptr;
  }

private:
  void release() {
    if (m_refCounter == nullptr) {
      return;
    }

    m_refCounter->decrement();
    if (!m_refCounter->isZero()) {
      return;
    }

    delete m_refCounter;
    m_refCounter = nullptr;

    if (m_object != nullptr) {
      assert(m_jniContext != nullptr);
      JniRefHelper::getJNIEnv(m_jniContext)->DeleteGlobalRef(m_object);
    }
  }

  const JniContext *m_jniContext;
  T m_object;
  RefCounter *m_refCounter;
};

#endif

