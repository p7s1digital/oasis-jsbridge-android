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
#ifndef _JSBRIDGE_JSEXCEPTION_H
#define _JSBRIDGE_JSEXCEPTION_H

#include <exception>
#include <jni.h>
#include <string>

#if defined(DUKTAPE)
# include "duktape/duktape.h"
#elif defined(QUICKJS)
# include "quickjs/quickjs.h"
#endif

class JsBridgeContext;

class JsException : public std::exception {
public:
#if defined(DUKTAPE)
  // Pop the current JS exception to create a new C++ exception
  JsException(const JsBridgeContext *, duk_idx_t);

  JsException(JsException &&);
  JsException(const JsException &) = delete;
  JsException &operator=(const JsException &) = delete;

  void pushError() const;
#elif defined(QUICKJS)
  JsException(const JsBridgeContext *, JSValue);

  JSValueConst getValue() const { return m_value; }
#endif

  ~JsException() override;

  const char *what() const throw() override { return m_what.c_str(); }

private:
  const JsBridgeContext *m_jsBridgeContext;
#if defined(DUKTAPE)
  std::string m_errorPropName;
#elif defined(QUICKJS)
  JSValue m_value;
#endif
  std::string m_what;
};

#endif
