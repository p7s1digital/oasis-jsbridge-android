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
#include "JniCache.h"
#include "JsBridgeContext.h"
#include "log.h"
#include "java-types/Deferred.h"
#include "jni-helpers/JniContext.h"
#include "jni-helpers/JniLocalRef.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include "jni-helpers/JStringLocalRef.h"
#include <new>

namespace {
  // This should be instanciated in each JNI entry function to make sure that the JNI context is
  // properly set up with the current JNIEnv
  JsBridgeContext *getJsBridgeContext(JNIEnv *env, jlong lctx) {
    assert(lctx != 0L);
    auto jsBridgeContext = reinterpret_cast<JsBridgeContext *>(lctx);

    // Set the current JNIEnv instance of the JniContext
    JniContext *jniContext = jsBridgeContext->getJniContext();
    assert(jniContext != nullptr);
    assert(jniContext->getJNIEnv() == env);
    jniContext->setCurrentJNIEnv(env);

#ifndef NDEBUG
    // In debug mode: check that we are in the correct thread
    // (everything else will lead to unexpected behavior...)
    jsBridgeContext->getJniCache()->getJsBridgeInterface().checkJsThread();
#endif

    return jsBridgeContext;
  }
}

extern "C" {

JNIEXPORT jlong JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCreateContext
    (JNIEnv *env, jobject object, jboolean doDebug) {

  alog("jniCreateContext()");

  auto jsBridgeContext = new JsBridgeContext();

  auto jniContext = new JniContext(env);

  try {
    jsBridgeContext->init(jniContext, JniLocalRef<jobject>(jniContext, object, true /*fromJniParam*/));

    if (doDebug) {
      jsBridgeContext->initDebugger();
    }
  } catch (std::bad_alloc &) {
    return 0L;
  }

  return reinterpret_cast<jlong>(jsBridgeContext);
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCancelDebug
    (JNIEnv *env, jobject, jlong lctx) {

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  jsBridgeContext->cancelDebug();
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniDeleteContext
    (JNIEnv *env, jobject, jlong lctx) {

  alog("jniDeleteContext()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

  delete jsBridgeContext;
  delete jniContext;
}

JNIEXPORT jobject JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniEvaluateString
    (JNIEnv *env, jobject, jlong lctx, jstring code, jobject returnParameter, jboolean awaitJsPromise) {

  //alog("jniEvaluateString()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

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

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

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

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

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

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

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

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

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

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

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

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

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

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

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

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

  std::string strGlobalName = JStringLocalRef(jniContext, globalName, true).str();
  std::string strJsCode = JStringLocalRef(jniContext, jsCode, true).str();

  jsBridgeContext->assignJsValue(strGlobalName, strJsCode);
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniNewJsFunction
    (JNIEnv *env, jobject, jlong lctx, jstring globalName, jobjectArray args, jstring jsCode) {

  //alog("jniNewJsFunction()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

  std::string strGlobalName = JStringLocalRef(jniContext, globalName, true).str();
  JObjectArrayLocalRef objectArgs(jniContext, args, true);
  JStringLocalRef strCode(jniContext, jsCode, true);

  jsBridgeContext->newJsFunction(strGlobalName, objectArgs, strCode.str());
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCompleteJsPromise
    (JNIEnv *env, jobject, jlong lctx, jstring id, jboolean isFulfilled, jobject value) {

  //alog("jniCompleteJsPromise()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

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

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  jsBridgeContext->processPromiseQueue();
}

}  // extern "C"
