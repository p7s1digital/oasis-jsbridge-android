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
#include "jni-helpers/JniGlobalRef.h"
#include "jni-helpers/JniTypes.h"
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
  const JniRef<jclass> &getJsBridgeClass() const { return m_jsBridgeClass; }
  const JniRef<jclass> &getJsBridgeMethodClass() const { return m_jsBridgeMethodClass; }
  const JniRef<jclass> &getJsBridgeParameterClass() const { return m_jsBridgeParameterClass; }

  // Access to JniInterface's
  const JsBridgeInterface &getJsBridgeInterface() const { return m_jsBridgeInterface; }
  MethodInterface getMethodInterface(const JniRef<jsBridgeMethod> &method) const { return MethodInterface(this, method); }
  ParameterInterface getParameterInterface(const JniRef<jsBridgeParameter> &parameter) const { return ParameterInterface(this, parameter); }

  // Exceptions
  const JniRef<jclass> &getIllegalArgumentExceptionClass() const { return m_illegalArgumentExceptionClass; }
  JniLocalRef<jthrowable> newJsException(
      const JStringLocalRef &jsonValue, const JStringLocalRef &detailedMessage,
      const JStringLocalRef &jsStackTrace, const JniRef<jthrowable> &cause) const;

  // JavaReflectedMethod (java.lang.reflect.Method)
  JStringLocalRef getJavaReflectedMethodName(const JniLocalRef<jobject> &javaMethod) const;

  // JsValue (de.prosiebensat1digital.oasisjsbridge.JsValue)
  JniLocalRef<jobject> newJsValue(const JStringLocalRef &name) const;
  JStringLocalRef getJsValueName(const JniRef<jobject> &jsValue) const;

  // JsonObjectWrapper (de.prosiebensat1digital.oasisjsbridge.JsonObjectWrapper)
  JniLocalRef<jobject> newJsonObjectWrapper(const JStringLocalRef &jsonString) const;
  JStringLocalRef getJsonObjectWrapperString(const JniRef<jobject> &jsonObjectWrapper) const;

  const JniContext *getJniContext() const { return m_jniContext; }

private:
  const JsBridgeContext *m_jsBridgeContext;
  const JniContext *m_jniContext;

  mutable std::unordered_map<JavaTypeId, JniGlobalRef<jclass>> m_javaClasses;

  JniGlobalRef<jclass> m_objectClass;
  JniGlobalRef<jclass> m_jsBridgeClass;
  JniGlobalRef<jclass> m_jsExceptionClass;
  JniGlobalRef<jclass> m_illegalArgumentExceptionClass;

  JniGlobalRef<jclass> m_jsBridgeMethodClass;
  JniGlobalRef<jclass> m_jsBridgeParameterClass;

  JniGlobalRef<jclass> m_jsBridgeJsValueClass;
  JniGlobalRef<jclass> m_jsonObjectWrapperClass;

  const JsBridgeInterface m_jsBridgeInterface;
};

#endif
