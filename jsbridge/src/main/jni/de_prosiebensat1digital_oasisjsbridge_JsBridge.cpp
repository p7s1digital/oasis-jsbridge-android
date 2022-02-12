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
#include "ExceptionHandler.h"
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

JNIEXPORT jlong JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCreateContext(JNIEnv *env, jobject object) {

  alog("jniCreateContext()");

  auto jsBridgeContext = new JsBridgeContext();
  auto jniContext = new JniContext(env, JniContext::EnvironmentSource::Manual);

  try {
    jsBridgeContext->init(jniContext, JniLocalRef<jobject>(jniContext, object, JniLocalRefMode::Borrowed));
  } catch (const std::bad_alloc &) {
    return 0L;
  }

  return reinterpret_cast<jlong>(jsBridgeContext);
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniStartDebugger
    (JNIEnv *env, jobject, jlong lctx, jint port) {

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  jsBridgeContext->startDebugger(port);
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

  JValue returnValue;
  try {
    returnValue = jsBridgeContext->evaluateString(JStringLocalRef(jniContext, code, JniLocalRefMode::Borrowed), JniLocalRef<jsBridgeParameter >(jniContext, returnParameter, JniLocalRefMode::Borrowed), awaitJsPromise);
  } catch (const std::exception &e) {
    jsBridgeContext->getExceptionHandler()->jniThrow(e);
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

  std::string strFilename = JStringLocalRef(jniContext, filename, JniLocalRefMode::Borrowed).toStdString();

  try {
    jsBridgeContext->evaluateFileContent(JStringLocalRef(jniContext, code, JniLocalRefMode::Borrowed), strFilename);
  } catch (const std::exception &e) {
    jsBridgeContext->getExceptionHandler()->jniThrow(e);
  }
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniRegisterJavaObject
    (JNIEnv *env, jobject, jlong lctx, jstring name, jobject javaObject, jobjectArray javaMethods) {

  //alog("jniRegisterJavaObject()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

  std::string strName = JStringLocalRef(jniContext, name, JniLocalRefMode::Borrowed).toUtf8Chars();

  try {
    jsBridgeContext->registerJavaObject(strName, JniLocalRef<jobject>(jniContext, javaObject, JniLocalRefMode::Borrowed),
                                       JObjectArrayLocalRef(jniContext, javaMethods, JniLocalRefMode::Borrowed));
  } catch (const std::exception &e) {
    jsBridgeContext->getExceptionHandler()->jniThrow(e);
  }
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniRegisterJavaLambda
    (JNIEnv *env, jobject, jlong lctx, jstring name, jobject javaObject, jobject javaMethod) {

  //alog("jniRegisterJavaLambda()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

  std::string strName = JStringLocalRef(jniContext, name, JniLocalRefMode::Borrowed).toStdString();

  try {
    jsBridgeContext->registerJavaLambda(strName, JniLocalRef<jobject>(jniContext, javaObject, JniLocalRefMode::Borrowed),
                                       JniLocalRef<jsBridgeMethod>(jniContext, javaMethod, JniLocalRefMode::Borrowed));
  } catch (const std::exception &e) {
    jsBridgeContext->getExceptionHandler()->jniThrow(e);
  }
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniRegisterJsObject
    (JNIEnv *env, jobject, jlong lctx, jstring name, jobjectArray methods, jboolean check) {

  //alog("jniRegisterJsObject()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

  std::string strName = JStringLocalRef(jniContext, name, JniLocalRefMode::Borrowed).toUtf8Chars();

  try {
    jsBridgeContext->registerJsObject(strName, JObjectArrayLocalRef(jniContext, methods, JniLocalRefMode::Borrowed), check);
  } catch (const std::exception &e) {
    jsBridgeContext->getExceptionHandler()->jniThrow(e);
  }
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniRegisterJsLambda
    (JNIEnv *env, jobject, jlong lctx, jstring name, jobject method) {

  //alog("jniRegisterJsLambda()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

  std::string strName = JStringLocalRef(jniContext, name, JniLocalRefMode::Borrowed).toStdString();

  try {
    jsBridgeContext->registerJsLambda(strName, JniLocalRef<jsBridgeMethod>(jniContext, method, JniLocalRefMode::Borrowed));
  } catch (const std::exception &e) {
    jsBridgeContext->getExceptionHandler()->jniThrow(e);
  }
}

JNIEXPORT jobject JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCallJsMethod
    (JNIEnv *env, jobject, jlong lctx, jstring objectName, jobject javaMethod, jobjectArray args, jboolean awaitJsPromise) {

  //alog("jniCallJsMethod()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

  std::string strObjectName = JStringLocalRef(jniContext, objectName, JniLocalRefMode::Borrowed).toUtf8Chars();

  JValue value;

  try {
    value = jsBridgeContext->callJsMethod(strObjectName,
                                          JniLocalRef<jobject>(jniContext, javaMethod, JniLocalRefMode::Borrowed),
                                          JObjectArrayLocalRef(jniContext, args, JniLocalRefMode::Borrowed),
                                          awaitJsPromise);
  } catch (const std::exception &e) {
    jsBridgeContext->getExceptionHandler()->jniThrow(e);
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

  std::string strObjectName = JStringLocalRef(jniContext, objectName, JniLocalRefMode::Borrowed).toStdString();

  JValue value;

  try {
    value = jsBridgeContext->callJsLambda(strObjectName,
                                          JObjectArrayLocalRef(jniContext, args, JniLocalRefMode::Borrowed),
                                          awaitJsPromise);
  } catch (const std::exception &e) {
    jsBridgeContext->getExceptionHandler()->jniThrow(e);
  }

// Prevent auto-releasing the localref returned to Java
  value.detachLocalRef();

  return value.get().l;
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniAssignJsValue
    (JNIEnv *env, jobject, jlong lctx, jstring globalName, jstring jsCode) {

  //alog("jniAssignJsValue()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

  std::string strGlobalName = JStringLocalRef(jniContext, globalName, JniLocalRefMode::Borrowed).toUtf8Chars();

  try {
    jsBridgeContext->assignJsValue(strGlobalName, JStringLocalRef(jniContext, jsCode, JniLocalRefMode::Borrowed));
  } catch (const std::exception &e) {
    jsBridgeContext->getExceptionHandler()->jniThrow(e);
  }
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniDeleteJsValue
    (JNIEnv *env, jobject, jlong lctx, jstring globalName) {

  //alog("jniDeleteJsValue()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

  std::string strGlobalName = JStringLocalRef(jniContext, globalName, JniLocalRefMode::Borrowed).toStdString();

  try {
    jsBridgeContext->deleteJsValue(strGlobalName);
  } catch (const std::exception &e) {
    jsBridgeContext->getExceptionHandler()->jniThrow(e);
  }
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCopyJsValue
    (JNIEnv *env, jobject, jlong lctx, jstring globalNameTo, jstring globalNameFrom) {

  //alog("jniCopyJsValue()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

  std::string strGlobalNameTo = JStringLocalRef(jniContext, globalNameTo, JniLocalRefMode::Borrowed).toStdString();
  std::string strGlobalNameFrom = JStringLocalRef(jniContext, globalNameFrom, JniLocalRefMode::Borrowed).toStdString();

  try {
    jsBridgeContext->copyJsValue(strGlobalNameTo, strGlobalNameFrom);
  } catch (const std::exception &e) {
    jsBridgeContext->getExceptionHandler()->jniThrow(e);
  }
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniNewJsFunction
    (JNIEnv *env, jobject, jlong lctx, jstring globalName, jobjectArray args, jstring jsCode) {

  //alog("jniNewJsFunction()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

  std::string strGlobalName = JStringLocalRef(jniContext, globalName, JniLocalRefMode::Borrowed).toUtf8Chars();
  JObjectArrayLocalRef objectArgs(jniContext, args, JniLocalRefMode::Borrowed);
  JStringLocalRef strCode(jniContext, jsCode, JniLocalRefMode::Borrowed);

  try {
    jsBridgeContext->newJsFunction(strGlobalName, objectArgs, strCode);
  } catch (const std::exception &e) {
    jsBridgeContext->getExceptionHandler()->jniThrow(e);
  }
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniConvertJavaValueToJs
    (JNIEnv *env, jobject, jlong lctx, jstring globalName, jobject javaValue, jobject parameter) {

  //alog("jniConvertJavaValueToJs()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

  std::string strGlobalName = JStringLocalRef(jniContext, globalName, JniLocalRefMode::Borrowed).toStdString();
  JniLocalRef<jobject> javaValueRef(jniContext, javaValue, JniLocalRefMode::Borrowed);
  JniLocalRef<jsBridgeParameter> parameterRef(jniContext, parameter, JniLocalRefMode::Borrowed);

  try {
    jsBridgeContext->convertJavaValueToJs(strGlobalName, javaValueRef, parameterRef);
  } catch (const std::exception &e) {
    jsBridgeContext->getExceptionHandler()->jniThrow(e);
  }
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCompleteJsPromise
    (JNIEnv *env, jobject, jlong lctx, jstring id, jboolean isFulfilled, jobject value) {

  //alog("jniCompleteJsPromise()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  auto jniContext = jsBridgeContext->getJniContext();

  std::string strId = JStringLocalRef(jniContext, id, JniLocalRefMode::Borrowed).toUtf8Chars();
  JniLocalRef<jobject> valueRef(jniContext, value, JniLocalRefMode::Borrowed);

  try {
    JavaTypes::Deferred::completeJsPromise(jsBridgeContext, strId, isFulfilled, valueRef);
  } catch (const std::exception &e) {
    jsBridgeContext->getExceptionHandler()->jniThrow(e);
  }
}

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniProcessPromiseQueue
  (JNIEnv *env, jobject, jlong lctx) {

  //alog("jniProcessPromiseQueue()");

  auto jsBridgeContext = getJsBridgeContext(env, lctx);
  jsBridgeContext->processPromiseQueue();
}

}  // extern "C"
