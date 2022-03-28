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
#include <jni.h>

#ifndef _DE_PROSIEBENSAT1DIGITAL_OASISJSBRIDGE_jSBRIDGE_H
#define _DE_PROSIEBENSAT1DIGITAL_OASISJSBRIDGE_jSBRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jlong JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCreateContext
  (JNIEnv *, jobject);

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniStartDebug
    (JNIEnv *, jobject, jlong, jint);

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCancelDebug
    (JNIEnv *, jobject, jlong);

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniDeleteContext
  (JNIEnv *, jobject, jlong);

JNIEXPORT jobject JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniEvaluateString
  (JNIEnv *, jobject, jlong, jstring, jobject, jboolean);

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniEvaluateFileContent
  (JNIEnv *, jobject, jlong, jstring, jstring, jboolean asModule);

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniRegisterJavaObject
    (JNIEnv *, jobject, jlong, jstring, jobject, jobjectArray);

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniRegisterJavaLambda
    (JNIEnv *, jobject, jlong, jstring, jobject, jobject);

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniRegisterJsObject
    (JNIEnv *, jobject, jlong, jstring, jobjectArray, jboolean);

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniRegisterJsLambda
    (JNIEnv *, jobject, jlong, jstring, jobject);

JNIEXPORT jobject JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCallJsMethod
    (JNIEnv *, jobject, jlong, jstring, jobject, jobjectArray, jboolean awaitJsPromise);

JNIEXPORT jobject JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCallJsLambda
    (JNIEnv *, jobject, jlong, jstring, jobjectArray, jboolean);

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniAssignJsValue
(JNIEnv *, jobject, jlong, jstring, jstring);

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniDeleteJsValue
    (JNIEnv *, jobject, jlong, jstring);

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCopyJsValue
    (JNIEnv *, jobject, jlong, jstring, jstring);

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniNewJsFunction
(JNIEnv *, jobject, jlong, jstring, jobjectArray, jstring);

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniConvertJavaValueToJs
    (JNIEnv *, jobject, jlong, jstring, jobject, jobject);

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniCompleteJsPromise
    (JNIEnv *, jobject, jlong, jstring, jboolean, jobject);

JNIEXPORT void JNICALL Java_de_prosiebensat1digital_oasisjsbridge_JsBridge_jniProcessPromiseQueue
    (JNIEnv *, jobject, jlong);

#ifdef __cplusplus
}
#endif

#endif
