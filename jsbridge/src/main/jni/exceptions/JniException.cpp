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
#include "JniException.h"
#include "jni-helpers/JniContext.h"
#include "log.h"

JniException::JniException(const JniContext *jniContext) {
  // First, clear the exception otherwise we cannot call any JNI method!
  jthrowable rawThrowable = jniContext->exceptionOccurred();
  assert(rawThrowable != nullptr);

  m_throwable = JniLocalRef<jthrowable>(jniContext, rawThrowable, JniLocalRefMode::AutoReleased);
  jniContext->exceptionClear();

  m_what = createMessage(jniContext, m_throwable);
}

// static
std::string JniException::createMessage(const JniContext *jniContext, const JniLocalRef<jthrowable> &throwable) {
  if (throwable.isNull()) {
    return "null";
  }

  JniLocalRef<jclass> exceptionClass = jniContext->getObjectClass(throwable);
  jmethodID getClass = jniContext->getMethodID(exceptionClass, "getClass", "()Ljava/lang/Class;");

  JniLocalRef<jobject> classObject = jniContext->callObjectMethod(exceptionClass, getClass);
  JniLocalRef<jclass> classObjectClass = jniContext->getObjectClass(classObject);

  jmethodID getName = jniContext->getMethodID(classObjectClass, "getName", "()Ljava/lang/String;");
  std::string exceptionName = jniContext->callStringMethod(exceptionClass, getName).toUtf8Chars();

  jmethodID getMessage = jniContext->getMethodID(exceptionClass, "getMessage", "()Ljava/lang/String;");
  std::string message = jniContext->callStringMethod(throwable, getMessage).toUtf8Chars();

  return exceptionName + ": " + message;
}

