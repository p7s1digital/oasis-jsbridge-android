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
#ifndef _JSBRIDGE_JAVASCRIPTLAMBDA_H
#define _JSBRIDGE_JAVASCRIPTLAMBDA_H

#include "JavaScriptObjectBase.h"
#include "jni-helpers/JniRef.h"
#include "jni-helpers/JniTypes.h"
#include "jni-helpers/JValue.h"
#include <string>

#ifdef QUICKJS
# include "quickjs/quickjs.h"
#endif

class ArgumentLoader;
class JsBridgeContext;
class JavaScriptMethod;
class JObjectArrayLocalRef;

// A wrapper to a JS function (lambda).
//
// It contains the whole information needed (mostly: parameters and Java types) to call the
// function from Java.
class JavaScriptLambda : public JavaScriptObjectBase {
public:
#if defined(DUKTAPE)
  JavaScriptLambda(const JsBridgeContext *, const JniRef<jsBridgeMethod> &method, std::string strName, void *jsHeapPtr);
#elif defined(QUICKJS)
  JavaScriptLambda(const JsBridgeContext *, const JniRef<jsBridgeMethod> &method, std::string strName, JSValue);
#endif

  ~JavaScriptLambda() override;

  JavaScriptLambda(const JavaScriptLambda &) = delete;
  JavaScriptLambda& operator=(const JavaScriptLambda &) = delete;

  JValue call(const JsBridgeContext *, const JObjectArrayLocalRef &args, bool awaitJsPromise) const;

private:
  JavaScriptMethod *m_method;
#if defined(DUKTAPE)
  void *m_jsHeapPtr;
#elif defined(QUICKJS)
  JSContext *m_ctx;
  JSValue m_jsValue;
#endif
};

#endif
