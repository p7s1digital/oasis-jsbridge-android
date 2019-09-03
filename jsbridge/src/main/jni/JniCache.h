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

#include "JavaTypeId.h"
#include "jni-helpers/JniGlobalRef.h"
#include "jni-helpers/JniTypes.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include "jni-helpers/JStringLocalRef.h"
#include <jni.h>
#include <unordered_map>

class JsBridgeContext;
class JniCache;
class JniContext;

template <class T>
class JavaInterface {
public:
  const JniRef<T> &object() const { return m_object; }

protected:
  JavaInterface(const JniCache *cache, const JniRef<jclass> &javaClass, const JniRef<T> &object)
    : p(cache) , m_class(javaClass) , m_object(object) {}

  const JniCache * const p;
  const JniRef<jclass> &m_class;
  JniGlobalRef<T> m_object;
};

// JNI functions wrapper with JniLocalRef/JniGlobalRef and JniLocalRefStats.
class JniCache {

public:
  JniCache(const JsBridgeContext *, const JniLocalRef<jobject> &jsBridgeJavaObject);

  const JniGlobalRef<jclass> &getJavaClass(JavaTypeId) const;
  const JniRef<jclass> &getObjectClass() const { return m_objectClass; }

  const JniRef<jclass> &getIllegalArgumentExceptionClass() const { return m_illegalArgumentExceptionClass; }
  JniLocalRef<jthrowable> newJsException(
      const JStringLocalRef &jsonValue, const JStringLocalRef &detailedMessage,
      const JStringLocalRef &jsStackTrace, const JniRef<jthrowable> &cause) const;

  // JavaMethod
  JStringLocalRef getJavaMethodName(const JniLocalRef<jobject> &javaMethod) const;

  // JsBridge
  struct JsBridge : public JavaInterface<jobject> {
    JsBridge(const JniCache *, const JniRef<jobject> &);

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

  // Method
  struct MethodInterface : public JavaInterface<jsBridgeMethod> {
    MethodInterface(const JniCache *, const JniRef<jsBridgeMethod> &);

    JniLocalRef<jobject> getJavaMethod() const;
    JStringLocalRef getName() const;
    JniLocalRef<jobject> callNativeLambda(const JniRef<jobject> &, const JObjectArrayLocalRef &) const;
    JniLocalRef<jsBridgeParameter> getReturnParameter() const;
    JObjectArrayLocalRef getParameters() const;
    jboolean isVarArgs() const;
  };

  // Parameter
  struct ParameterInterface : public JavaInterface<jsBridgeParameter> {
    ParameterInterface(const JniCache *, const JniRef<jsBridgeParameter> &);

    JniLocalRef<jsBridgeMethod> getInvokeMethod() const;
    JniLocalRef<jobject> getJava() const;
    JStringLocalRef getJavaName() const;
    JniLocalRef<jsBridgeParameter> getGenericParameter() const;
    JStringLocalRef getName() const;
  };

  const JsBridge &jsBridgeInterface() const { return m_jsBridge; }
  MethodInterface methodInterface(const JniRef<jsBridgeMethod> &method) const { return MethodInterface(this, method); }
  ParameterInterface parameterInterface(const JniRef<jsBridgeParameter> &parameter) const { return ParameterInterface(this, parameter); }

  // JsValue
  JniLocalRef<jobject> newJsValue(const JStringLocalRef &name) const;
  JStringLocalRef getJsValueName(const JniRef<jobject> &jsValue) const;

  // JsonObjectWrapper
  JniLocalRef<jobject> newJsonObjectWrapper(const JStringLocalRef &jsonString) const;
  JStringLocalRef getJsonObjectWrapperString(const JniRef<jobject> &jsonObjectWrapper) const;

private:
  const JsBridgeContext *m_jsBridgeContext;
  const JniContext *m_jniContext;

  mutable std::unordered_map<JavaTypeId, JniGlobalRef<jclass>> m_javaClasses;

  JniGlobalRef<jclass> m_objectClass;
  JniGlobalRef<jclass> m_illegalArgumentExceptionClass;
  JniGlobalRef<jclass> m_jsExceptionClass;

  const JsBridge m_jsBridge;
  JniGlobalRef<jclass> m_jsBridgeClass;
  JniGlobalRef<jclass> m_jsBridgeMethodClass;
  JniGlobalRef<jclass> m_jsBridgeParameterClass;

  JniGlobalRef<jclass> m_jsBridgeJsValueClass;
  JniGlobalRef<jclass> m_jsonObjectWrapperClass;
};

#endif
