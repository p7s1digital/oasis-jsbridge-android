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

// Small wrapper around JNI value
// It has the (small) memory cost of having an additional LocalRef but makes the management of JNI
// references safe
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

  explicit JValue(jbyte b)
    : m_value() {
    m_value.b = b;
  }

  explicit JValue(jlong l)
    : m_value() {
    m_value.j = l;
  }

  explicit JValue(jshort s)
   : m_value() {
    m_value.s = s;
  }

  explicit JValue(jdouble d)
    : m_value() {
    m_value.d = d;
  }

  explicit JValue(jfloat f)
    : m_value() {
    m_value.f = f;
  }

  explicit JValue(JniLocalRef<jobject> &&localRef)
    : m_value() {

    m_localRef.swap(localRef);
    m_value.l = m_localRef.get();
  }

  template <class T>
  explicit JValue(const JniLocalRef<T> &localRef)
    : m_value()
    , m_localRef(localRef.template staticCast<jobject>()) {

    m_value.l = m_localRef.get();
  }

  JValue(const JValue &other)
    : m_value(other.m_value)
    , m_localRef(other.m_localRef) {

    assert(m_localRef.isNull() || m_value.l == m_localRef.get());
  }

  JValue(JValue &&other)
      : m_value(other.m_value) {

    m_localRef.swap(other.m_localRef);
  }

  JValue& operator=(const JValue &other) {
    m_value = other.m_value;
    m_localRef = other.m_localRef;

    assert(m_localRef.isNull() || m_value.l == m_localRef.get());

    return *this;
  }

  JValue& operator=(JValue &&other) {
    m_localRef.swap(other.m_localRef);
    std::swap(m_value, other.m_value);

    return *this;
  }

  const jvalue &get() const { return m_value; }

  bool isNull() const { return m_value.l == nullptr; }

  jboolean getBool() const { return m_value.z; }
  jbyte getByte() const { return m_value.b; }
  jint getInt() const { return m_value.i; }
  jlong getLong() const { return m_value.j; }
  jshort getShort() const { return m_value.s; }
  jdouble getDouble() const { return m_value.d; }
  jfloat getFloat() const { return m_value.f; }

  const JniLocalRef<jobject> &getLocalRef() const { return m_localRef; }

  void detachLocalRef() {
    m_localRef.detach();
  }

  void releaseLocalRef() const {
    m_localRef.release();
    m_localRef.reset();
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
  mutable JniLocalRef<jobject> m_localRef;
};

#endif
