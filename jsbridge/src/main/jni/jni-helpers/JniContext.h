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
#ifndef _JSBRIDGE_JNICONTEXT_H
#define _JSBRIDGE_JNICONTEXT_H

#include "JniGlobalRef.h"
#include "JStringLocalRef.h"
#include "JValue.h"
#include "JniTypes.h"
#include "JniValueConverter.h"
#include "JniLocalRef.h"
#include <jni.h>
#include <string>

// JNI functions wrapper with JniLocalRef/JniGlobalRef
class JniContext {

public:
  JniContext(JNIEnv *env, jobject jsBridgeJavaObject);
  ~JniContext();

  JNIEnv *getJNIEnv() const { return m_jniEnv; }

  const JniRef<jclass> &getObjectClass() const { return m_objectClass; }
  const JniRef<jobject> &getJsBridgeObject() const { return m_jsBridgeJavaObject; }
  const JniRef<jclass> &getJsBridgeMethodClass() const { return m_jsBridgeMethodClass; }
  const JniRef<jclass> &getJsBridgeParameterClass() const { return m_jsBridgeParameterClass; }

  jmethodID getMethodID(const JniRef<jclass> &, const char *name, const char *sig) const;
  jmethodID getStaticMethodID(const JniRef<jclass> &, const char *name, const char *sig) const;
  jfieldID getStaticFieldID(const JniRef<jclass> &, const char *name, const char *sig) const;

  JniLocalRef<jclass> findClass(const char *name) const;

  template <class T>
  JniLocalRef<jclass> getObjectClass(const JniRef<T> &t) const {
    assert(m_jniEnv);

    jclass c = m_jniEnv->GetObjectClass((jobject) t.get());
    return JniLocalRef<jclass>(this, c);
  }

  JniLocalRef<jclass> getStaticObjectField(const JniRef<jclass> &clazz, jfieldID fieldId) const {
    assert(m_jniEnv);

    return JniLocalRef<jclass>(this, m_jniEnv->GetStaticObjectField(clazz.get(), fieldId));
  }

  template <class T>
  jmethodID fromReflectedMethod(const JniRef<T> &t) const {
    assert(m_jniEnv);
    return m_jniEnv->FromReflectedMethod((jobject) t.get());
  }

  template <class T, typename ...InputArgs>
  JniLocalRef<T> newObject(const JniRef<jclass> &clazz, jmethodID methodID, InputArgs &&...args) const {
    assert(m_jniEnv);

    jobject o = m_jniEnv->NewObject(clazz.get(), methodID, JniValueConverter::toJniValues(std::forward<InputArgs>(args))...);
    return JniLocalRef<T>(this, o);
  }

  void throw_(const JniRef<jthrowable> &throwable);
  void throwNew(const JniRef<jclass> &clazz, const char *s);

  jboolean exceptionCheck() const;
  jthrowable exceptionOccurred() const;
  void exceptionClear();

  template <class ObjT, typename ...InputArgs>
  void callVoidMethod(const JniRef<ObjT> &t, jmethodID methodId, InputArgs &&...args) const {
    assert(m_jniEnv);

    m_jniEnv->CallVoidMethod(t.get(), methodId, JniValueConverter::toJniValues(std::forward<InputArgs>(args))...);
  }

  template <class ObjT>
  void callVoidMethodA(const JniRef<ObjT> &t, jmethodID methodId, const std::vector<JValue> &args) const {
    assert(m_jniEnv);

    jvalue *rawArgs = JValue::createArray(args);
    m_jniEnv->CallVoidMethodA(t.get(), methodId, rawArgs);
    delete[] rawArgs;
  }

  template <class ObjT, typename ...InputArgs>
  jboolean callBooleanMethod(const JniRef<ObjT> &t, jmethodID methodId, InputArgs &&...args) const {
    assert(m_jniEnv);

    return m_jniEnv->CallBooleanMethod(t.get(), methodId, JniValueConverter::toJniValues(std::forward<InputArgs>(args))...);
  }

  template <class ObjT>
  jboolean callBooleanMethodA(const JniRef<ObjT> &t, jmethodID methodId, const std::vector<JValue> &args) const {
    assert(m_jniEnv);

    jvalue *rawArgs = JValue::createArray(args);
    jboolean ret = m_jniEnv->CallBooleanMethodA(t.get(), methodId, rawArgs);
    delete[] rawArgs;
    return ret;
  }

  template <class ObjT, typename ...InputArgs>
  jint callIntMethod(const JniRef<ObjT> &t, jmethodID methodId, InputArgs &&...args) const {
    assert(m_jniEnv);

    return m_jniEnv->CallIntMethod(t.get(), methodId, JniValueConverter::toJniValues(std::forward<InputArgs>(args))...);
  }

  template <class ObjT>
  jint callIntMethodA(const JniRef<ObjT> &t, jmethodID methodId, const std::vector<JValue> &args) const {
      assert(m_jniEnv);

      jvalue *rawArgs = JValue::createArray(args);
      jint ret = m_jniEnv->CallIntMethodA(t.get(), methodId, rawArgs);
      delete[] rawArgs;
      return ret;
  }

  template <class ObjT, typename ...InputArgs>
  jlong callLongMethod(const JniRef<ObjT> &t, jmethodID methodId, InputArgs &&...args) const {
    assert(m_jniEnv);

    return m_jniEnv->CallLongMethod(t.get(), methodId, JniValueConverter::toJniValues(std::forward<InputArgs>(args))...);
  }

  template <class ObjT>
  jlong callLongMethodA(const JniRef<ObjT> &t, jmethodID methodId, const std::vector<JValue> &args) const {
    assert(m_jniEnv);

    jvalue *rawArgs = JValue::createArray(args);
    jlong ret = m_jniEnv->CallLongMethodA(t.get(), methodId, rawArgs);
    delete[] rawArgs;
    return ret;
  }

  template <class ObjT, typename ...InputArgs>
  jdouble callDoubleMethod(const JniRef<ObjT> &t, jmethodID methodId, InputArgs &&...args) const {
    assert(m_jniEnv);

    return m_jniEnv->CallDoubleMethod(t.get(), methodId, JniValueConverter::toJniValues(std::forward<InputArgs>(args))...);
  }

  template <class ObjT>
  jdouble callDoubleMethodA(const JniRef<ObjT> &t, jmethodID methodId, const std::vector<JValue> &args) const {
    assert(m_jniEnv);

    jvalue *rawArgs = JValue::createArray(args);
    jdouble ret = m_jniEnv->CallDoubleMethodA(t.get(), methodId, rawArgs);
    delete[] rawArgs;
    return ret;
  }

  template <class ObjT, typename ...InputArgs>
  jfloat callFloatMethod(const JniRef<ObjT> &t, jmethodID methodId, InputArgs &&...args) const {
    assert(m_jniEnv);

    return m_jniEnv->CallFloatMethod(t.get(), methodId, JniValueConverter::toJniValues(std::forward<InputArgs>(args))...);
  }

  template <class ObjT>
  jfloat callFloatMethodA(const JniRef<ObjT> &t, jmethodID methodId, const std::vector<JValue> &args) const {
    assert(m_jniEnv);

    jvalue *rawArgs = JValue::createArray(args);
    jfloat ret = m_jniEnv->CallFloatMethodA(t.get(), methodId, rawArgs);
    delete[] rawArgs;
    return ret;
  }

  template <class RetT = jobject, class ObjT, typename ...InputArgs>
  JniLocalRef<RetT> callObjectMethod(const JniRef<ObjT> &t, jmethodID methodId, InputArgs &&...args) const {
    assert(m_jniEnv);

    RetT jniRetVal = (RetT) m_jniEnv->CallObjectMethod(t.get(), methodId, JniValueConverter::toJniValues(std::forward<InputArgs>(args))...);
    return JniLocalRef<RetT>(this, jniRetVal);
  }

  template <class RetT = jobject, class ObjT>
  JniLocalRef<RetT> callObjectMethodA(const JniRef<ObjT> &t, jmethodID methodId, const std::vector<JValue> &args) const {
    assert(m_jniEnv);

    jvalue *rawArgs = JValue::createArray(args);
    jobject o = m_jniEnv->CallObjectMethodA(t.get(), methodId, rawArgs);
    delete[] rawArgs;
    return JniLocalRef<RetT>(this, o);
  }

  template <typename ...InputArgs>
  void callStaticVoidMethod(const JniRef<jclass> &t, jmethodID methodId, InputArgs &&...args) const {
      assert(m_jniEnv);

      m_jniEnv->CallStaticVoidMethod(t.get(), methodId, JniValueConverter::toJniValues(std::forward<InputArgs>(args))...);
  }

  template <class RetT = jobject, typename ...InputArgs>
  JniLocalRef<RetT> callStaticObjectMethod(const JniRef<jclass> &t, jmethodID methodId, InputArgs &&...args) const {
    assert(m_jniEnv);

    jobject o = m_jniEnv->CallStaticObjectMethod(t.get(), methodId, JniValueConverter::toJniValues(std::forward<InputArgs>(args))...);
    return JniLocalRef<RetT>(this, o);
  }

  template <class RetT = jobject>
  JniLocalRef<RetT> callStaticObjectMethodA(const JniRef<jclass> &t, jmethodID methodId, const std::vector<JValue> &args) const {
    assert(m_jniEnv);

    jvalue *rawArgs = JValue::createArray(args);
    jobject o = m_jniEnv->CallStaticObjectMethodA(t.get(), methodId, rawArgs);
    delete[] rawArgs;
    return JniLocalRef<RetT>(this, o);
  }

  template <typename ...InputArgs>
  void callJsBridgeVoidMethod(const char *name, const char *sig, InputArgs &&...args) const {
    assert(m_jniEnv);

    jmethodID methodId = getMethodID(m_jsBridgeJavaClass, name, sig);
    return callVoidMethod(m_jsBridgeJavaObject, methodId, args...);
  }

  template <class RetT = jobject, typename ...InputArgs>
  JniLocalRef<RetT> callJsBridgeObjectMethod(const char *name, const char *sig, InputArgs &&...args) const {
    assert(m_jniEnv);

    jmethodID methodId = getMethodID(m_jsBridgeJavaClass, name, sig);
    return callObjectMethod<RetT>(m_jsBridgeJavaObject, methodId, args...);
  }

private:
  friend class InternalScopedContext;

  JNIEnv *m_jniEnv;

  // TODO: move it to JniCache!
  JniGlobalRef<jclass> m_jsBridgeJavaClass;
  JniGlobalRef<jobject> m_jsBridgeJavaObject;
  JniGlobalRef<jclass> m_jsBridgeMethodClass;
  JniGlobalRef<jclass> m_jsBridgeParameterClass;
  JniGlobalRef<jclass> m_objectClass;
};

#endif
