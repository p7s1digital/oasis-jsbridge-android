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
#ifndef _JSBRIDGE_JARRAYLOCALREF_H
#define _JSBRIDGE_JARRAYLOCALREF_H

#include "JniLocalRef.h"
#include "JniRefHelper.h"
#include <jni.h>
#include <string>

// Same as LocalRef<jarray> with additional array-specific utilities
// Note: only works for jboolean, jlong, jint, jdouble, jfloat; use JObjectArrayLocalRef for jobject!
template <typename T>
class JArrayLocalRef : public JniLocalRef<jarray> {

public:
  JArrayLocalRef() = delete;

  JArrayLocalRef(const JArrayLocalRef &other)
      : JniLocalRef(other)
      , m_jniReleaseArrayMode(JNI_ABORT) {
  }

  explicit JArrayLocalRef(const JniLocalRef<jarray> &localRef)
      : JniLocalRef(localRef)
      , m_jniReleaseArrayMode(JNI_ABORT) {
  }

  explicit JArrayLocalRef(JniLocalRef<jarray> &&localRef)
      : JniLocalRef(std::forward<JniLocalRef<jarray>>(localRef))
      , m_jniReleaseArrayMode(JNI_ABORT) {
  }

  template<typename U = T>
  JArrayLocalRef(const JniContext *jniContext, jsize count, Mode mode = Mode::AutoReleased, typename std::enable_if_t<std::is_same<U, jboolean>::value>* = nullptr)
      : JniLocalRef<jarray>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewBooleanArray(count), mode)
      , m_jniReleaseArrayMode(JNI_ABORT) {
  }

  template<typename U = T>
  JArrayLocalRef(const JniContext *jniContext, jsize count, Mode mode = Mode::AutoReleased, typename std::enable_if_t<std::is_same<U, jbyte>::value>* = nullptr)
       : JniLocalRef<jarray>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewByteArray(count), mode)
      , m_jniReleaseArrayMode(JNI_ABORT) {
  }

    template<typename U = T>
  JArrayLocalRef(const JniContext *jniContext, jsize count, Mode mode = Mode::AutoReleased, typename std::enable_if_t<std::is_same<U, jint>::value>* = nullptr)
      : JniLocalRef<jarray>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewIntArray(count), mode)
      , m_jniReleaseArrayMode(JNI_ABORT) {
  }

  template<typename U = T>
  JArrayLocalRef(const JniContext *jniContext, jsize count, Mode mode = Mode::AutoReleased, typename std::enable_if_t<std::is_same<U, jlong>::value>* = nullptr)
      : JniLocalRef<jarray>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewLongArray(count), mode)
      , m_jniReleaseArrayMode(JNI_ABORT) {
  }

  template<typename U = T>
  JArrayLocalRef(const JniContext *jniContext, jsize count, Mode mode = Mode::AutoReleased, typename std::enable_if_t<std::is_same<U, jdouble>::value>* = nullptr)
      : JniLocalRef<jarray>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewDoubleArray(count), mode)
      , m_jniReleaseArrayMode(JNI_ABORT) {
  }

  template<typename U = T>
  JArrayLocalRef(const JniContext *jniContext, jsize count, Mode mode = Mode::AutoReleased, typename std::enable_if_t<std::is_same<U, jfloat>::value>* = nullptr)
      : JniLocalRef<jarray>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewFloatArray(count), mode)
      , m_jniReleaseArrayMode(JNI_ABORT) {
  }

  ~JArrayLocalRef() {
    releaseArrayElements();
  }


  // Explicitly disable a dangerous cast to bool
  operator bool() const = delete;

  void release() {
    releaseArrayElements();
    JniLocalRef::release();
  }

  jsize getLength() const {
    JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
    assert(env != nullptr);

    return env->GetArrayLength(get());
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jboolean>::value>>
  const jboolean *getElements() const {
    if (!m_elements) {
      m_elements = getJniEnv()->GetBooleanArrayElements(static_cast<jbooleanArray>(get()), nullptr);
      m_jniReleaseArrayMode = JNI_ABORT;  // no copy  back
    }

    return static_cast<const T *>(m_elements);
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jboolean>::value>>
  jboolean *getMutableElements() {
    if (!m_elements) {
      m_elements = getJniEnv()->GetBooleanArrayElements(static_cast<jbooleanArray>(get()), nullptr);
    }

    m_jniReleaseArrayMode = 0;  // copy back + free
    return static_cast<T *>(m_elements);
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jbyte>::value>>
  const jbyte *getElements() const {
    if (!m_elements) {
      m_elements = getJniEnv()->GetByteArrayElements(static_cast<jbyteArray>(get()), nullptr);
      m_jniReleaseArrayMode = JNI_ABORT;  // no copy back
    }

    return static_cast<const T *>(m_elements);
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jbyte>::value>>
  jbyte *getMutableElements() {
    if (!m_elements) {
      m_elements = getJniEnv()->GetByteArrayElements(static_cast<jbyteArray>(get()), nullptr);
    }

    m_jniReleaseArrayMode = 0;  // copy back + free
    return static_cast<T *>(m_elements);
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jint>::value>>
  const jint *getElements() const {
    if (!m_elements) {
      m_elements = getJniEnv()->GetIntArrayElements(static_cast<jintArray>(get()), nullptr);
      m_jniReleaseArrayMode = JNI_ABORT;  // no copy back
    }
   return static_cast<const T *>(m_elements);
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jint>::value>>
  jint *getMutableElements() {
    if (!m_elements) {
      m_elements = getJniEnv()->GetIntArrayElements(static_cast<jintArray>(get()), nullptr);
    }

    m_jniReleaseArrayMode = 0;  // copy back + free
    return static_cast<T *>(m_elements);
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jlong>::value>>
  const jlong *getElements() const {
    if (!m_elements) {
      m_elements = getJniEnv()->GetLongArrayElements(static_cast<jlongArray>(get()), nullptr);
      m_jniReleaseArrayMode = JNI_ABORT;  // no copy  back
    }

    return static_cast<const T *>(m_elements);
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jlong>::value>>
  jlong *getMutableElements() {
    if (!m_elements) {
      m_elements = getJniEnv()->GetLongArrayElements(static_cast<jlongArray>(get()), nullptr);
    }

    m_jniReleaseArrayMode = 0;  // copy back + free
    return static_cast<T *>(m_elements);
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jfloat>::value>>
  const jfloat *getElements() const {
    if (!m_elements) {
      m_elements = getJniEnv()->GetFloatArrayElements(static_cast<jfloatArray>(get()), nullptr);
    }

    return static_cast<const T *>(m_elements);
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jfloat>::value>>
  jfloat *getMutableElements() {
    if (!m_elements) {
      m_elements = getJniEnv()->GetFloatArrayElements(static_cast<jfloatArray>(get()), nullptr);
      m_jniReleaseArrayMode = JNI_ABORT;  // no copy  back
    }

    m_jniReleaseArrayMode = 0;  // copy back + free
    return static_cast<T *>(m_elements);
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jdouble>::value>>
  const jdouble *getElements() const {
    if (!m_elements) {
      m_elements = getJniEnv()->GetDoubleArrayElements(static_cast<jdoubleArray>(get()), nullptr);
    }

    m_jniReleaseArrayMode = 0;  // copy back + free
    return static_cast<const T *>(m_elements);
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jdouble>::value>>
  jdouble *getMutableElements() {
    if (!m_elements) {
      m_elements = getJniEnv()->GetDoubleArrayElements(static_cast<jdoubleArray>(get()), nullptr);
      m_jniReleaseArrayMode = JNI_ABORT;  // no copy  back
    }

    m_jniReleaseArrayMode = 0;  // copy back + free
    return static_cast<T *>(m_elements);
  }

  // Release / copy back array elements returned by get(Mutable)Elements()
  template <typename V = void, class Q = T>
  typename std::enable_if<std::is_same<Q, jboolean>::value, V>::type
  releaseArrayElements() {
    if (m_elements != nullptr) {
      getJniEnv()->ReleaseBooleanArrayElements(
          static_cast<jbooleanArray>(get()),
          static_cast<jboolean *>(m_elements), m_jniReleaseArrayMode);
      m_elements = nullptr;
    }
  }

  // Release / copy back array elements returned by get(Mutable)Elements()
  template <typename V = void, class Q = T>
  typename std::enable_if<std::is_same<Q, jbyte>::value, V>::type
  releaseArrayElements() {
    if (m_elements != nullptr) {
      getJniEnv()->ReleaseByteArrayElements(
              static_cast<jbyteArray >(get()),
              static_cast<T *>(m_elements), m_jniReleaseArrayMode);
      m_elements = nullptr;
    }
  }

  // Release / copy back array elements returned by get(Mutable)Elements()
  template <typename V = void, class Q = T>
  typename std::enable_if<std::is_same<Q, jint>::value, V>::type
  releaseArrayElements() {
    if (m_elements != nullptr) {
      getJniEnv()->ReleaseIntArrayElements(
          static_cast<jintArray >(get()),
          static_cast<T *>(m_elements), m_jniReleaseArrayMode);
      m_elements = nullptr;
    }
  }

  // Release / copy back array elements returned by get(Mutable)Elements()
  template <typename V = void, class Q = T>
  typename std::enable_if<std::is_same<Q, jlong>::value, V>::type
  releaseArrayElements() {
    if (m_elements != nullptr) {
      getJniEnv()->ReleaseLongArrayElements(
          static_cast<jlongArray >(get()),
          static_cast<T *>(m_elements), m_jniReleaseArrayMode);
      m_elements = nullptr;
    }
  }

  // Release / copy back array elements returned by get(Mutable)Elements()
  template <typename V = void, class Q = T>
  typename std::enable_if<std::is_same<Q, jfloat>::value, V>::type
  releaseArrayElements() {
    if (m_elements != nullptr) {
      getJniEnv()->ReleaseFloatArrayElements(
          static_cast<jfloatArray >(get()),
          static_cast<T *>(m_elements), m_jniReleaseArrayMode);
      m_elements = nullptr;
    }
  }

  // Release / copy back array elements returned by get(Mutable)Elements()
  template <typename V = void, class Q = T>
  typename std::enable_if<std::is_same<Q, jdouble>::value, V>::type
  releaseArrayElements() {
    if (m_elements != nullptr) {
      getJniEnv()->ReleaseDoubleArrayElements(
          static_cast<jdoubleArray >(get()),
          static_cast<T *>(m_elements), m_jniReleaseArrayMode);
      m_elements = nullptr;
    }
  }

private:
  mutable void *m_elements = nullptr;
  mutable jint m_jniReleaseArrayMode;
};

#endif
