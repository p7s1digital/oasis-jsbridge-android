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
#include "de_prosiebensat1digital_oasisjsbridge_JsBridge.h"
#include "JsBridgeContext.h"
#include "log.h"
#include "java-types/Deferred.h"
#include "jni-helpers/JniLocalRefStats.h"
#include "jni-helpers/JniLocalRef.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include "jni-helpers/JStringLocalRef.h"
#include <new>

#define SET_UP_JNI_CONTEXT(env, lctx) auto internalScopedContext = InternalScopedContext(env, lctx)
#define GET_DUKTAPE_CONTEXT() internalScopedContext.getJsBridgeContext()
#define DELETE_JNI_CONTEXT() internalScopedContext.deleteJniContext()

//namespace {
  // Handle context within its scope. This should be instanciated in each JNI entry
  // function to make sure that the whole JNI context is properly set up
  class InternalScopedContext {
  public:
    explicit InternalScopedContext(JNIEnv *env, jlong lctx) {
      //alog("BEGINNING OF JNI SCOPE");
      m_jsBridgeContext = reinterpret_cast<JsBridgeContext *>(lctx);

      m_jniContext = m_jsBridgeContext->jniContext();
      assert(m_jniContext != nullptr);
      m_jniContext->m_jniEnv = env;

#ifndef NDEBUG
      // In debug mode: check that we are in the correct thread
      // (everything else will lead to unexpected behavior...)
      m_jsBridgeContext->jniContext()->callJsBridgeVoidMethod("checkJsThread", "()V");
#endif
    }

    ~InternalScopedContext() {
      if (m_jniContext == nullptr) {
        return;
      }

      const JniLocalRefStats *localRefStats = m_jniContext->getLocalRefStats();
      //alog("END OF JNI SCOPE, LOCAL REF COUNT: %d, MAX: %d", localRefStats->currentCount(), localRefStats->maxCount());
    }

    JsBridgeContext *getJsBridgeContext() const { return m_jsBridgeContext; }

    void deleteJniContext() {
      delete m_jniContext;
      m_jniContext = nullptr;
    }

  private:
    JsBridgeContext *m_jsBridgeContext = nullptr;
    JniContext *m_jniContext = nullptr;
  };
//}

extern "C" {

JNIEXPORT jlong JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCreateContext
    (JNIEnv *env, jobject object, jboolean doDebug) {

  alog("jniCreateContext()");

  auto jsBridgeContext = new JsBridgeContext();

  auto jniContext = new JniContext(env, object);

  try {
    jsBridgeContext->init(jniContext);

    if (doDebug) {
      jsBridgeContext->initDebugger();
    }
  } catch (std::bad_alloc &) {
    return 0L;
  }

  auto lctx = reinterpret_cast<jlong>(jsBridgeContext);

  SET_UP_JNI_CONTEXT(env, lctx);

  return lctx;
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCancelDebug
    (JNIEnv *env, jobject, jlong lctx) {

  SET_UP_JNI_CONTEXT(env, lctx);
  auto jsBridgeContext = GET_DUKTAPE_CONTEXT();
  if (jsBridgeContext == nullptr) {
    return;
  }

  jsBridgeContext->cancelDebug();
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniDeleteContext
    (JNIEnv *env, jobject, jlong lctx) {

  alog("jniDeleteContext()");

  SET_UP_JNI_CONTEXT(env, lctx);
  auto jsBridgeContext = GET_DUKTAPE_CONTEXT();
  if (jsBridgeContext == nullptr) {
    return;
  }

  delete jsBridgeContext;
  DELETE_JNI_CONTEXT();
}

JNIEXPORT jobject JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniEvaluateString
    (JNIEnv *env, jobject, jlong lctx, jstring code, jobject returnParameter, jboolean awaitJsPromise) {

  //alog("jniEvaluateString()");

  SET_UP_JNI_CONTEXT(env, lctx);
  auto jsBridgeContext = GET_DUKTAPE_CONTEXT();
  if (jsBridgeContext == nullptr) {
    return nullptr;
  }

  JniContext *jniContext = jsBridgeContext->jniContext();

  std::string strCode = JStringLocalRef(jniContext, code, true).str();

  JValue returnValue;
  try {
    returnValue = jsBridgeContext->evaluateString(strCode, JniLocalRef<jsBridgeParameter >(jniContext, returnParameter, true), awaitJsPromise);
  } catch (const std::invalid_argument &e) {
    jsBridgeContext->queueIllegalArgumentException(e.what());
    return nullptr;
  } catch (const std::runtime_error &e) {
    jsBridgeContext->queueJsException(e.what());
    return nullptr;
  }

  // Prevent auto-releasing the localref returned to Java
  returnValue.detachLocalRef();

  return returnValue.get().l;
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniEvaluateFileContent
    (JNIEnv *env, jobject, jlong lctx, jstring code, jstring filename) {

  //alog("jniEvaluateFileContent()");

  SET_UP_JNI_CONTEXT(env, lctx);
  auto jsBridgeContext = GET_DUKTAPE_CONTEXT();
  if (jsBridgeContext == nullptr) {
    return;
  }

  auto jniContext = jsBridgeContext->jniContext();
  assert(jniContext != nullptr);

  std::string strCode = JStringLocalRef(jniContext, code, true).str();
  std::string strFilename = JStringLocalRef(jniContext, filename, true).str();

  try {
    jsBridgeContext->evaluateFileContent(strCode, strFilename);
  } catch (const std::invalid_argument &e) {
    jsBridgeContext->queueIllegalArgumentException(e.what());
  } catch (const std::runtime_error &e) {
    jsBridgeContext->queueJsException(e.what());
  }
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniRegisterJavaObject
    (JNIEnv *env, jobject, jlong lctx, jstring name, jobject javaObject, jobjectArray javaMethods) {

  //alog("jniRegisterJavaObject()");

  SET_UP_JNI_CONTEXT(env, lctx);
  auto jsBridgeContext = GET_DUKTAPE_CONTEXT();
  if (jsBridgeContext == nullptr) {
    return;
  }

  auto jniContext = jsBridgeContext->jniContext();
  assert(jniContext != nullptr);

  std::string strName = JStringLocalRef(jniContext, name, true).str();

  try {
    jsBridgeContext->registerJavaObject(strName, JniLocalRef<jobject>(jniContext, javaObject, true),
                                       JObjectArrayLocalRef(jniContext, javaMethods, true));
  } catch (const std::invalid_argument &e) {
    jsBridgeContext->queueIllegalArgumentException(e.what());
  } catch (const std::runtime_error &e) {
    jsBridgeContext->queueJsException(e.what());
  }
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniRegisterJavaLambda
    (JNIEnv *env, jobject, jlong lctx, jstring name, jobject javaObject, jobject javaMethod) {

  //alog("jniRegisterJavaLambda()");

  SET_UP_JNI_CONTEXT(env, lctx);
  auto jsBridgeContext = GET_DUKTAPE_CONTEXT();
  if (jsBridgeContext == nullptr) {
    return;
  }

  auto jniContext = jsBridgeContext->jniContext();
  assert(jniContext != nullptr);

  std::string strName = JStringLocalRef(jniContext, name, true).str();

  try {
    jsBridgeContext->registerJavaLambda(strName, JniLocalRef<jobject>(jniContext, javaObject, true),
                                       JniLocalRef<jsBridgeMethod>(jniContext, javaMethod, true));
  } catch (const std::invalid_argument &e) {
    jsBridgeContext->queueIllegalArgumentException(e.what());
  } catch (const std::runtime_error &e) {
    jsBridgeContext->queueJsException(e.what());
  }
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniRegisterJsObject
    (JNIEnv *env, jobject, jlong lctx, jstring name, jobjectArray methods) {

  //alog("jniRegisterJsObject()");

  SET_UP_JNI_CONTEXT(env, lctx);
  auto jsBridgeContext = GET_DUKTAPE_CONTEXT();
  if (jsBridgeContext == nullptr) {
    return;
  }

  auto jniContext = jsBridgeContext->jniContext();
  assert(jniContext != nullptr);

    std::string strName = JStringLocalRef(jniContext, name, true).str();

    try {
      jsBridgeContext->registerJsObject(strName, JObjectArrayLocalRef(jniContext, methods, true));
    } catch (const std::invalid_argument &e) {
      jsBridgeContext->queueIllegalArgumentException(e.what());
    } catch (const std::runtime_error &e) {
      jsBridgeContext->queueJsException(e.what());
    }
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniRegisterJsLambda
    (JNIEnv *env, jobject, jlong lctx, jstring name, jobject method) {

  //alog("jniRegisterJsLambda()");

  SET_UP_JNI_CONTEXT(env, lctx);
  auto jsBridgeContext = GET_DUKTAPE_CONTEXT();
  if (jsBridgeContext == nullptr) {
    return;
  }

  auto jniContext = jsBridgeContext->jniContext();
  assert(jniContext != nullptr);

  std::string strName = JStringLocalRef(jniContext, name, true).str();

  try {
    jsBridgeContext->registerJsLambda(strName, JniLocalRef<jsBridgeMethod>(jniContext, method, true));
  } catch (const std::invalid_argument &e) {
    jsBridgeContext->queueIllegalArgumentException(e.what());
  } catch (const std::runtime_error &e) {
    jsBridgeContext->queueJsException(e.what());
  }
}

JNIEXPORT jobject JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCallJsMethod
    (JNIEnv *env, jobject, jlong lctx, jstring objectName, jobject javaMethod, jobjectArray args) {

  //alog("jniCallJsMethod()");

  SET_UP_JNI_CONTEXT(env, lctx);
  auto jsBridgeContext = GET_DUKTAPE_CONTEXT();
  if (jsBridgeContext == nullptr) {
    return nullptr;
  }

  auto jniContext = jsBridgeContext->jniContext();
  assert(jniContext != nullptr);

  std::string strObjectName = JStringLocalRef(jniContext, objectName, true).str();

  JValue value;

  try {
    value = jsBridgeContext->callJsMethod(strObjectName,
                                          JniLocalRef<jobject>(jniContext, javaMethod, true),
                                          JObjectArrayLocalRef(jniContext, args, true));
  } catch (const std::invalid_argument &e) {
      jsBridgeContext->queueIllegalArgumentException(e.what());
  } catch (const std::runtime_error &e) {
    jsBridgeContext->queueJsException(e.what());
  }

  // Prevent auto-releasing the localref returned to Java
  value.detachLocalRef();

  return value.get().l;
}

JNIEXPORT jobject JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCallJsLambda
    (JNIEnv *env, jobject, jlong lctx, jstring objectName, jobjectArray args, jboolean awaitJsPromise) {

  //alog("jniCallJsLambda()");

  SET_UP_JNI_CONTEXT(env, lctx);
  auto jsBridgeContext = GET_DUKTAPE_CONTEXT();
  if (jsBridgeContext == nullptr) {
    return nullptr;
  }

  auto jniContext = jsBridgeContext->jniContext();
  assert(jniContext != nullptr);

  std::string strObjectName = JStringLocalRef(jniContext, objectName, true).str();

  JValue value;

  try {
    value = jsBridgeContext->callJsLambda(strObjectName,
                                          JObjectArrayLocalRef(jniContext, args, true),
                                          awaitJsPromise);
  } catch (const std::invalid_argument &e) {
    jsBridgeContext->queueIllegalArgumentException(e.what());
  } catch (const std::runtime_error &e) {
    jsBridgeContext->queueJsException(e.what());
  }

  // Prevent auto-releasing the localref returned to Java
  value.detachLocalRef();

  return value.get().l;
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniAssignJsValue
    (JNIEnv *env, jobject, jlong lctx, jstring globalName, jstring jsCode) {

  //alog("jniInitJsValue()");

  SET_UP_JNI_CONTEXT(env, lctx);
  auto jsBridgeContext = GET_DUKTAPE_CONTEXT();
  if (jsBridgeContext == nullptr) {
    return;
  }

  auto jniContext = jsBridgeContext->jniContext();
  assert(jniContext != nullptr);

  std::string strGlobalName = JStringLocalRef(jniContext, globalName, true).str();
  std::string strJsCode = JStringLocalRef(jniContext, jsCode, true).str();

  jsBridgeContext->assignJsValue(strGlobalName, strJsCode);
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniNewJsFunction
    (JNIEnv *env, jobject, jlong lctx, jstring globalName, jobjectArray args, jstring jsCode) {

  //alog("jniNewJsFunction()");

  SET_UP_JNI_CONTEXT(env, lctx);
  auto jsBridgeContext = GET_DUKTAPE_CONTEXT();
  if (jsBridgeContext == nullptr) {
    return;
  }

  auto jniContext = jsBridgeContext->jniContext();
  assert(jniContext != nullptr);

  std::string strGlobalName = JStringLocalRef(jniContext, globalName, true).str();
  JObjectArrayLocalRef objectArgs(jniContext, args, true);
  JStringLocalRef strCode(jniContext, jsCode, true);

  jsBridgeContext->newJsFunction(strGlobalName, objectArgs, strCode.str());
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCompleteJsPromise
    (JNIEnv *env, jobject, jlong lctx, jstring id, jboolean isFulfilled, jobject value) {

  //alog("jniCompleteJsPromise()");

  SET_UP_JNI_CONTEXT(env, lctx);
  auto jsBridgeContext = GET_DUKTAPE_CONTEXT();
  if (jsBridgeContext == nullptr) {
    return;
  }

  auto jniContext = jsBridgeContext->jniContext();
  assert(jniContext != nullptr);

  std::string strId = JStringLocalRef(jniContext, id, true).str();
  JniLocalRef<jobject> valueRef(jniContext, value, true);

  try {
    JavaTypes::Deferred::completeJsPromise(jsBridgeContext, strId, isFulfilled, valueRef);
  } catch (const std::invalid_argument &e) {
    jsBridgeContext->queueIllegalArgumentException(e.what());
  } catch (const std::runtime_error &e) {
    jsBridgeContext->queueJsException(e.what());
  }
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniProcessPromiseQueue
  (JNIEnv *env, jobject, jlong lctx) {

  //alog("jniProcessPromiseQueue()");

  SET_UP_JNI_CONTEXT(env, lctx);
  auto jsBridgeContext = GET_DUKTAPE_CONTEXT();
  if (jsBridgeContext == nullptr) {
    return;
  }

  jsBridgeContext->processPromiseQueue();
}

}  // extern "C"
