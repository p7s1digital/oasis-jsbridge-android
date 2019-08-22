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

  //JObjectArrayLocalRef(JniContext *jniContext, jobjectArray o, bool fromJniParam = false)
  //    : JniLocalRef<jobjectArray>(jniContext, o, fromJniParam)
  //    , m_str(o == nullptr ? nullptr : JniRefHelper::getJNIEnv(jniContext)->GetStringUTFChars(o, nullptr))
  //    , m_needsReleaseArray(true) {
  //}

  JArrayLocalRef(const JArrayLocalRef &other) = default;

  explicit JArrayLocalRef(const JniLocalRef<jarray> &localRef): JniLocalRef(localRef) {
  }

  //explicit JObjectArrayLocalRef(const JniLocalRef<jobjectArray > &localRef)
  //    : JniLocalRef<jobjectArray>(localRef)
  //    , m_str(localRef.get() ? JniRefHelper::getJNIEnv(localRef.getJniContext())->GetStringUTFChars(localRef.create(), nullptr) : nullptr)
  //    , m_needsReleaseChars(true) {
  //}

  template<typename U = T>
  JArrayLocalRef(const JniContext *jniContext, jsize count, typename std::enable_if_t<std::is_same<U, jboolean>::value>* = nullptr)
      : JniLocalRef<jarray>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewBooleanArray(count)) {
  }

  template<typename U = T>
  JArrayLocalRef(const JniContext *jniContext, jsize count, typename std::enable_if_t<std::is_same<U, jint>::value>* = nullptr)
      : JniLocalRef<jarray>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewIntArray(count)) {
  }

  template<typename U = T>
  JArrayLocalRef(const JniContext *jniContext, jsize count, typename std::enable_if_t<std::is_same<U, jlong>::value>* = nullptr)
      : JniLocalRef<jarray>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewLongArray(count)) {
  }

  template<typename U = T>
  JArrayLocalRef(const JniContext *jniContext, jsize count, typename std::enable_if_t<std::is_same<U, jdouble>::value>* = nullptr)
      : JniLocalRef<jarray>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewDoubleArray(count)) {
  }

  template<typename U = T>
  JArrayLocalRef(const JniContext *jniContext, jsize count, typename std::enable_if_t<std::is_same<U, jfloat>::value>* = nullptr)
      : JniLocalRef<jarray>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewFloatArray(count)) {
  }

  // Explicitly disable a dangerous cast to bool
  operator bool() const = delete;

  jsize getLength() const {
    JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
    assert(env != nullptr);

    return env->GetArrayLength(get());
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jboolean>::value>>
  jboolean getElement(jsize index) const {
    JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
    assert(env != nullptr);

    jboolean buffer[1];
    env->GetBooleanArrayRegion((jbooleanArray) get(), index, 1, buffer);
    return buffer[0];
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jint>::value>>
  jint getElement(jsize index) const {
    JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
    assert(env != nullptr);

    jint buffer[1];
    env->GetIntArrayRegion((jintArray) get(), index, 1, buffer);
    return buffer[0];
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jlong>::value>>
  jlong getElement(jsize index) const {
    JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
    assert(env != nullptr);

    jlong buffer[1];
    env->GetLongArrayRegion((jlongArray) get(), index, 1, buffer);
    return buffer[0];
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jdouble>::value>>
  jdouble getElement(jsize index) const {
    JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
    assert(env != nullptr);

    jdouble buffer[1];
    env->GetDoubleArrayRegion((jdoubleArray) get(), index, 1, buffer);
    return buffer[0];
  }

  template<typename U = T, typename = std::enable_if_t<std::is_same<U, jfloat>::value>>
  jfloat getElement(jsize index) const {
    JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
    assert(env != nullptr);

    jfloat buffer[1];
    env->GetFloatArrayRegion((jfloatArray) get(), index, 1, buffer);
    return buffer[0];
  }

  void setElement(jsize index, jboolean v) {
    JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
    assert(env != nullptr);

    env->SetBooleanArrayRegion((jbooleanArray) get(), index, 1, &v);
  }

  void setElement(jsize index, jint v) {
      JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
      assert(env != nullptr);

      env->SetIntArrayRegion((jintArray) get(), index, 1, &v);
  }

  void setElement(jsize index, jlong v) {
      JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
      assert(env != nullptr);

      env->SetLongArrayRegion((jlongArray) get(), index, 1, &v);
  }

  void setElement(jsize index, jdouble v) {
      JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
      assert(env != nullptr);

      env->SetDoubleArrayRegion((jdoubleArray) get(), index, 1, &v);
  }

  void setElement(jsize index, jfloat v) {
    JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
    assert(env != nullptr);

    env->SetFloatArrayRegion((jfloatArray) get(), index, 1, &v);
  }
};

#endif
