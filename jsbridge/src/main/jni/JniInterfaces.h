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
#ifndef _JSBRIDGE_JNIINTERFACES_H
#define _JSBRIDGE_JNIINTERFACES_H

#include "JniTypes.h"
#include "jni-helpers/JniGlobalRef.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include "jni-helpers/JStringLocalRef.h"
#include <jni.h>

class JniCache;

// Base class for a JniInterface: access to methods of a Java instances
template <class T>
class JniInterface {
public:
  const JniRef<T> &object() const { return m_object; }

protected:
  JniInterface(const JniCache *cache, const JniRef<jclass> &javaClass, const JniRef<T> &object)
    : m_jniCache(cache) , m_class(javaClass) , m_object(object) {}

  const JniCache * const m_jniCache;
  JniGlobalRef<jclass> m_class;
  JniGlobalRef<T> m_object;
};

// de.prosiebensat1digital.oasisjsbridge.JsBridge
class JsBridgeInterface : public JniInterface<jobject> {
public:
  JsBridgeInterface(const JniCache *, const JniRef<jobject> &);

  void checkJsThread() const;
  void onDebuggerPending() const;
  void onDebuggerReady() const;
  JniLocalRef<jobject> createJsLambdaProxy(const JStringLocalRef &, const JniRef<jsBridgeMethod> &) const;
  void consoleLogHelper(const JStringLocalRef &logType, const JStringLocalRef &msg) const;
  void resolveDeferred(const JniRef<jobject> &javaDeferred, const JValue &) const;
  void rejectDeferred(const JniRef<jobject> &javaDeferred, const JValue &exception) const;
  JniLocalRef<jobject> createCompletableDeferred() const;
  void setUpJsPromise(const JStringLocalRef &, const JniRef<jobject> &deferred) const;
};

// de.prosiebensat1digital.oasisjsbridge.Method
class MethodInterface : public JniInterface<jsBridgeMethod> {
public:
  MethodInterface(const JniCache *, const JniRef<jsBridgeMethod> &);

  JniLocalRef<jobject> getJavaMethod() const;
  JStringLocalRef getName() const;
  JniLocalRef<jobject> callNativeLambda(const JniRef<jobject> &, const JObjectArrayLocalRef &) const;
  JniLocalRef<jsBridgeParameter> getReturnParameter() const;
  JObjectArrayLocalRef getParameters() const;
  jboolean isVarArgs() const;
};

// de.prosiebensat1digital.oasisjsbridge.Parameter
class ParameterInterface : public JniInterface<jsBridgeParameter> {
public:
  ParameterInterface(const JniCache *, const JniRef<jsBridgeParameter> &);

  JniLocalRef<jsBridgeMethod> getInvokeMethod() const;
  JniLocalRef<jobject> getJava() const;
  JStringLocalRef getJavaName() const;
  jboolean isNullable() const;
  JniLocalRef<jsBridgeParameter> getComponentType() const;
  JniLocalRef<jsBridgeParameter> getGenericParameter() const;
  JStringLocalRef getName() const;
};

#endif
