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
#ifndef _JSBRIDGE_JVALUE_H
#define _JSBRIDGE_JVALUE_H

#include "JniLocalRef.h"
#include <jni.h>
#include <vector>

// Small wrapper around JNI value using LocalRef
class JValue {
public:
  JValue()
    : m_value() {
    m_value.l = nullptr;
  }

  explicit JValue(jboolean b)
    : m_value() {
    m_value.z = b;
  }

  explicit JValue(jint i)
    : m_value() {
    m_value.i = i;
  }

  explicit JValue(jlong l)
   : m_value() {
    m_value.j = l;
  }

  explicit JValue(jdouble d)
   : m_value() {
    m_value.d = d;
  }

  explicit JValue(jfloat f)
      : m_value() {
    m_value.f = f;
  }

  template <class T>
  explicit JValue(const JniLocalRef<T> &localRef)
    : m_value()
    , m_localRefPtr(new JniLocalRef<jobject>(localRef.template staticCast<jobject>())) {

    m_value.l = m_localRefPtr->get();
  }

  JValue(const JValue &other)
    : m_value(other.m_value)
    , m_localRefPtr(other.m_localRefPtr == nullptr ? nullptr : new JniLocalRef<jobject>(*other.m_localRefPtr)) {
  }

  JValue(JValue &&other)
      : m_value(other.m_value)
      , m_localRefPtr(other.m_localRefPtr) {

      other.m_localRefPtr = nullptr;
  }

  JValue& operator=(const JValue &other) {
    delete m_localRefPtr;

    m_value = other.m_value;
    m_localRefPtr = other.m_localRefPtr == nullptr ? nullptr : new JniLocalRef<jobject>(*other.m_localRefPtr);

    return *this;
  }

  JValue& operator=(JValue &&other) {
    if (other.m_localRefPtr == m_localRefPtr) {
      return *this;
    }

    m_localRefPtr = other.m_localRefPtr;
    m_value = other.m_value;

    other.m_localRefPtr = nullptr;
    return *this;
  }

  ~JValue() {
    delete m_localRefPtr;
  }

  const jvalue &get() const { return m_value; }

  bool isNull() const { return m_value.l == nullptr; }

  jboolean getBool() const { return m_value.z; }
  jint getInt() const { return m_value.i; }
  jlong getLong() const { return m_value.j; }
  jdouble getDouble() const { return m_value.d; }
  jfloat getFloat() const { return m_value.f; }

  const JniLocalRef<jobject> &getLocalRef() const {
    if (m_localRefPtr == nullptr) {
      static const auto nullLocalRef = JniLocalRef<jobject>();
      return nullLocalRef;
    }

    return *m_localRefPtr;
  }

  void detachLocalRef() const {
    if (m_localRefPtr) {
      m_localRefPtr->detach();
    }
  }

  void releaseLocalRef() const {
    if (m_localRefPtr) {
      m_localRefPtr->release();
      delete m_localRefPtr;
      const_cast<JValue *>(this)->m_localRefPtr = nullptr;
    }
  }

  // Note: returned array needs to be explictly deleted!!!
  static jvalue *createArray(const std::vector<JValue> &values) {
    size_t count = values.size();
    auto *array = new jvalue[count];
    for (int i = 0; i < count; ++i) {
      array[i] = values[i].get();
    }
    return array;
  }

  static void releaseAll(const std::vector<JValue> &values) {
    for (const JValue &value: values) {
      value.releaseLocalRef();
    }
  }

private:
  jvalue m_value;
  JniLocalRef<jobject> *m_localRefPtr = nullptr;
};

#endif
