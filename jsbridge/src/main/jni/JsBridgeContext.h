/*
 * Copyright (C) 2019 ProSiebenSat1.Digital GmbH.
 *
 * Originally based on Duktape Android:
 * Copyright (C) 2015 Square, Inc.
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
#ifndef _JSBRIDGE_JSBRIDGECONTEXT_H
#define _JSBRIDGE_JSBRIDGECONTEXT_H

#include "JavaTypeMap.h"
#include "jni-helpers/JniLocalRef.h"
#include "jni-helpers/JniContext.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include <jni.h>
#include <string>

#if defined(DUKTAPE)
# include "duktape/duktape.h"
#else
# include "quickjs/quickjs.h"
#endif

class DuktapeUtils;
class JavaType;
class JObjectArrayLocalRef;
class QuickJsUtils;

// JS context, delegating operations to the JS engine.
class JsBridgeContext {

public:
  JsBridgeContext();
  JsBridgeContext(const JsBridgeContext &) = delete;
  JsBridgeContext & operator=(const JsBridgeContext &) = delete;

  ~JsBridgeContext();

  // Must be called immediately after the constructor
  void init(JniContext *jniContext);

  void initDebugger();
  void cancelDebug();

  JValue evaluateString(const std::string &strSourceCode, const JniLocalRef<jsBridgeParameter> &returnParameter,
                                bool awaitJsPromise) const;
  void evaluateFileContent(const std::string &strSourceCode, const std::string &strFileName) const;

  void registerJavaObject(const std::string &strName, const JniLocalRef<jobject> &object,
                                  const JObjectArrayLocalRef &methods);
  void registerJavaLambda(const std::string &strName, const JniLocalRef<jobject> &object,
                                  const JniLocalRef<jsBridgeMethod> &method);
  void registerJsObject(const std::string &strName, const JObjectArrayLocalRef &methods);
  void registerJsLambda(const std::string &strName, const JniLocalRef<jsBridgeMethod> &method);
  JValue callJsMethod(const std::string &objectName, const JniLocalRef<jobject> &javaMethod,
                              const JObjectArrayLocalRef &args);
  JValue callJsLambda(const std::string &strFunctionName, const JObjectArrayLocalRef &args,
                              bool awaitJsPromise);

  void assignJsValue(const std::string &strGlobalName, const std::string &strCode);
  void newJsFunction(const std::string &strGlobalName, const JObjectArrayLocalRef &args, const std::string &strCode);

  void completeJsPromise(const std::string &strId, bool isFulfilled, const JniLocalRef<jobject> &value,
                                 const JniLocalRef<jclass> &valueType);
  void processPromiseQueue();

  void queueIllegalArgumentException(const std::string &message) const;
  void queueJsException(const std::string &message) const;
  //void queueNullPointerException(const std::string &message) const;
  void checkRethrowJsError() const;
  void queueJavaExceptionForJsError() const;

  JniContext *jniContext() const { return m_currentJniContext; }
  const JavaTypeMap &getJavaTypes() const { return m_javaTypes; }
  JniLocalRef<jthrowable> getJavaExceptionForJsError() const;

#if defined(DUKTAPE)
  static JsBridgeContext *getInstance(duk_context *);

  DuktapeUtils *getUtils() const { return m_utils; }
  duk_context *getCContext() const { return m_context; };
#elif defined(QUICKJS)
  static JsBridgeContext *getInstance(JSContext *);

  QuickJsUtils *getUtils() const { return m_utils; }
  JSContext *getCContext() const { return m_ctx; };
#endif

private:
  // Updated on each Java -> Native call (and reset to nullptr afterwards)
  JniContext *m_currentJniContext = nullptr;

  mutable JavaTypeMap m_javaTypes;
  const JavaType *m_objectType = nullptr;

#if defined(DUKTAPE)
  duk_idx_t pushJavaObject(const char *instanceName, const JniLocalRef<jobject> &object, const JObjectArrayLocalRef &methods) const;

  duk_context *m_context = nullptr;
  DuktapeUtils *m_utils = nullptr;
#elif defined(QUICKJS)
  JSValue createJavaObject(const char *instanceName, const JniLocalRef<jobject> &object, const JObjectArrayLocalRef &methods) const;

  JSRuntime *m_runtime = nullptr;
  JSContext *m_ctx = nullptr;
  QuickJsUtils *m_utils = nullptr;
#endif
};

#endif
