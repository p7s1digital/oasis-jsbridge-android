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
#ifndef _JSBRIDGE_EXCEPTIONHANDLER_H
#define _JSBRIDGE_EXCEPTIONHANDLER_H

#include "jni-helpers/JniLocalRef.h"
#include <string>

#if defined(QUICKJS)
# include "quickjs/quickjs.h"
#endif

class JniException;
class JsBridgeContext;
class JsException;

class ExceptionHandler {

public:
  explicit ExceptionHandler(const JsBridgeContext *);
  ExceptionHandler(const ExceptionHandler &) = delete;
  ExceptionHandler &operator=(const ExceptionHandler &) = delete;

  // JS error -> C++ JsException
  JsException getCurrentJsException() const;

  // C++ JsException -> Java exception
  JniLocalRef<jthrowable> getJavaException(const JsException &) const;

  // Java exception -> JS exception
#if defined(DUKTAPE)
  void pushJavaException(const JniLocalRef<jthrowable> &) const;
#elif defined(QUICKJS)
  JSValue javaExceptionToJsValue(const JniLocalRef<jthrowable> &) const;
#endif

  // Throw exception
  void jsThrow(const std::exception &) const;
  void jniThrow(const std::exception &) const;

private:
  const JsBridgeContext *m_jsBridgeContext;
};

#endif
