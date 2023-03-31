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
#ifndef _JSBRIDGE_JNICACHE_H
#define _JSBRIDGE_JNICACHE_H

#define JSBRIDGE_PKG_PATH "de/prosiebensat1digital/oasisjsbridge"

#include "JavaTypeId.h"
#include "JniInterfaces.h"
#include "JniTypes.h"
#include "jni-helpers/JniGlobalRef.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include "jni-helpers/JStringLocalRef.h"
#include <jni.h>
#include <unordered_map>

class JsBridgeContext;
class JniCache;
class JniContext;

// Cache for frequently-accessed JNI elements, provide access to JniInterface's
class JniCache {

public:
  JniCache(const JsBridgeContext *, const JniLocalRef<jobject> &jsBridgeJavaObject);

  const JniGlobalRef<jclass> &getJavaClass(JavaTypeId) const;
  const JniRef<jclass> &getObjectClass() const { return m_objectClass; }
  const JniRef<jclass> &getNumberClass() const { return m_numberClass; }
  const JniRef<jclass> &getStringClass() const { return m_stringClass; }
  const JniRef<jclass> &getListClass() const { return m_listClass; }
  const JniRef<jclass> &getJavaClassClass() const { return m_javaClassClass; }
  const JniRef<jclass> &getJsBridgeClass() const { return m_jsBridgeClass; }
  const JniRef<jclass> &getJsBridgeMethodClass() const { return m_jsBridgeMethodClass; }
  const JniRef<jclass> &getJsBridgeParameterClass() const { return m_jsBridgeParameterClass; }

  // Access to JniInterface's
  const JsBridgeInterface &getJsBridgeInterface() const { return m_jsBridgeInterface; }
  MethodInterface getMethodInterface(const JniRef<jsBridgeMethod> &method) const { return MethodInterface(this, method); }
  ParameterInterface getParameterInterface(const JniRef<jsBridgeParameter> &parameter) const { return ParameterInterface(this, parameter); }

  // Exceptions
  const JniRef<jclass> &getIllegalArgumentExceptionClass() const { return m_illegalArgumentExceptionClass; }
  const JniRef<jclass> &getRuntimeExceptionClass() const { return m_runtimeExceptionClass; }
  JniLocalRef<jthrowable> newJsException(
      const JStringLocalRef &jsonValue, const JStringLocalRef &detailedMessage,
      const JStringLocalRef &jsStackTrace, const JniRef<jthrowable> &cause) const;

  // JavaReflectedMethod (java.lang.reflect.Method)
  JStringLocalRef getJavaReflectedMethodName(const JniLocalRef<jobject> &javaMethod) const;

  // DebugString (de.prosiebensat1digital.oasisjsbridge.DebugString)
  JniLocalRef<jobject> newDebugString(const char *s) const;
  JniLocalRef<jobject> newDebugString(const JStringLocalRef &s) const;
  JStringLocalRef getDebugStringString(const JniRef<jobject> &debugString) const;

  // JsValue (de.prosiebensat1digital.oasisjsbridge.JsValue)
  JniLocalRef<jobject> newJsValue(const JStringLocalRef &name) const;
  JStringLocalRef getJsValueName(const JniRef<jobject> &jsValue) const;

  // JsonObjectWrapper (de.prosiebensat1digital.oasisjsbridge.JsonObjectWrapper)
  JniLocalRef<jobject> newJsonObjectWrapper(const JStringLocalRef &jsonString) const;
  JStringLocalRef getJsonObjectWrapperString(const JniRef<jobject> &jsonObjectWrapper) const;

  // JavaObjectWrapper (de.prosiebensat1digital.oasisjsbridge.JavaObjectWrapper)
  JniLocalRef<jobject> getOrCreateJavaObjectWrapper(const JniRef<jobject> &javaObject) const;
  JniLocalRef<jobject> javaObjectWrapperFromJavaObject(const JniRef<jobject> &javaObject) const;
  JniLocalRef<jobject> getJavaObjectWrapperJavaObject(const JniRef<jobject> &javaObjectWrapper) const;

  // JsToJavaProxy (de.prosiebensat1digital.oasisjsbridge.JsToJavaProxy)
  JniLocalRef<jobject> newJsToJavaProxy(const JniRef<jobject> &javaObject, const JStringLocalRef &name) const;

  // List (java.util.List)
  JniLocalRef<jobject> newList() const;
  void addToList(const JniLocalRef<jobject> &list, const JniLocalRef<jobject> &element) const;
  int getListLength(const JniLocalRef<jobject> &list) const;
  JniLocalRef<jobject> getListElement(const JniLocalRef<jobject> &list, int i) const;

  // Parameter (de.prosiebensat1digital.oasisjsbridge.Parameter)
  JniLocalRef<jsBridgeParameter> newParameter(const JniLocalRef<jclass> &javaClass) const;

  const JniContext *getJniContext() const { return m_jniContext; }

private:
  const JsBridgeContext *m_jsBridgeContext;
  const JniContext *m_jniContext;

  mutable std::unordered_map<JavaTypeId, JniGlobalRef<jclass>> m_javaClasses;

  JniGlobalRef<jclass> m_objectClass;
  JniGlobalRef<jclass> m_numberClass;
  JniGlobalRef<jclass> m_stringClass;
  JniGlobalRef<jclass> m_listClass;
  JniGlobalRef<jclass> m_javaClassClass;
  JniGlobalRef<jclass> m_arrayListClass;
  JniGlobalRef<jclass> m_jsBridgeClass;
  JniGlobalRef<jclass> m_jsExceptionClass;
  JniGlobalRef<jclass> m_illegalArgumentExceptionClass;
  JniGlobalRef<jclass> m_runtimeExceptionClass;

  JniGlobalRef<jclass> m_jsBridgeMethodClass;
  JniGlobalRef<jclass> m_jsBridgeParameterClass;

  JniGlobalRef<jclass> m_jsBridgeDebugStringClass;
  JniGlobalRef<jclass> m_jsBridgeJsValueClass;
  JniGlobalRef<jclass> m_jsonObjectWrapperClass;
  JniGlobalRef<jclass> m_javaObjectWrapperClass;
  JniGlobalRef<jclass> m_jsToJavaProxyClass;

  const JsBridgeInterface m_jsBridgeInterface;
};

#endif
