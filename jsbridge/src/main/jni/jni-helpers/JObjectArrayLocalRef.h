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
#ifndef _JSBRIDGE_JOBJECTLOCALREF_H
#define _JSBRIDGE_JOBJECTLOCALREF_H

#include "JniLocalRef.h"
#include "JniContext.h"
#include "JniRefHelper.h"
#include <jni.h>
#include <string>

// Same as LocalRef<jobjectArray> with additional array-specific utilities
class JObjectArrayLocalRef : public JniLocalRef<jobjectArray> {

public:
  JObjectArrayLocalRef() = delete;

  JObjectArrayLocalRef(const JniContext *jniContext, jobjectArray o, bool fromJniParam = false)
      : JniLocalRef<jobjectArray>(jniContext, o, fromJniParam) {
  }

  JObjectArrayLocalRef(const JniContext *jniContext, jsize count, const JniRef<jclass> &elementClass)
      : JniLocalRef<jobjectArray>(jniContext, JniRefHelper::getJNIEnv(jniContext)->NewObjectArray(count, elementClass.get(), nullptr)) {
  }

  explicit JObjectArrayLocalRef(const JniLocalRef<jobjectArray> &other)
    : JniLocalRef<jobjectArray>(other.getJniContext(), newObjectArrayLocalRef(other)) {

    if (other.isNull()) {
      return;
    }

    const JniContext *jniContext = other.getJniContext();
    assert(jniContext != nullptr);

    JNIEnv *env = JniRefHelper::getJNIEnv(other.getJniContext());
    assert(env != nullptr);

    // Copy elements to make sure that we have our local references
    auto count = env->GetArrayLength(other.get());
    for (auto i = 0L; i < count; ++i) {
      jobject otherElement = env->GetObjectArrayElement(other.get(), i);
      if (otherElement != nullptr) {
        setElement(i, JniLocalRef<jobject>(jniContext, env->NewLocalRef(otherElement)));
      }
    }
  }

  JObjectArrayLocalRef(const JObjectArrayLocalRef &other) = default;
  JObjectArrayLocalRef &operator=(const JObjectArrayLocalRef &) = default;

  // Explicitly disable a dangerous cast to bool
  operator bool() const = delete;

  jsize getLength() const {
    JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
    assert(env != nullptr);

    return env->GetArrayLength(get());
  }

  template <typename T = jobject>
  JniLocalRef<T> getElement(jsize index) const {
    JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
    assert(env != nullptr);

    jobject object = env->GetObjectArrayElement(get(), index);
    return JniLocalRef<T>(getJniContext(), object);
  }

  template <typename T = jobject>
  void setElement(jsize index, const JniRef<T> &element) {
    JNIEnv *env = JniRefHelper::getJNIEnv(getJniContext());
    assert(env != nullptr);

    env->SetObjectArrayElement(get(), index, element.get());
  }

private:
  jobjectArray newObjectArrayLocalRef(const JniLocalRef<jobjectArray> &other) const {
    if (other.isNull()) {
      return nullptr;
    }

    const JniContext *jniContext = other.getJniContext();
    assert(jniContext != nullptr);

    JNIEnv *env = jniContext->getJNIEnv();
    assert(env != nullptr);

    return (jobjectArray) env->NewLocalRef(other.get());
  }
};

#endif
