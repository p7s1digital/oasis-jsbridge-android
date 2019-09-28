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
#ifndef _JSBRIDGE_JAVASCRIPTOBJECT_H
#define _JSBRIDGE_JAVASCRIPTOBJECT_H

#include "jni-helpers/JniTypes.h"
#include "jni-helpers/JValue.h"
#include "jni-helpers/JniLocalRef.h"
#include <functional>
#include <string>
#include <unordered_map>

#if defined(DUKTAPE)
# include "duktape/duktape.h"
#elif defined(QUICKJS)
# include "quickjs/quickjs.h"
#endif

class JsBridgeContext;
class JavaScriptMethod;
class JObjectArrayLocalRef;

// A wrapper to a JS object and its methods.
//
// It contains the whole information (mostly: methods with parameter and Java types) needed to call
// the object methods from Java.
class JavaScriptObject {
public:
  typedef std::unordered_map<jmethodID, std::shared_ptr<JavaScriptMethod>> MethodMap;

#if defined(DUKTAPE)
  JavaScriptObject(const JsBridgeContext *, std::string strName, duk_idx_t jsObjectIndex, const JObjectArrayLocalRef &methods);

  JValue call(const JniLocalRef<jobject> &javaMethod, const JObjectArrayLocalRef &args) const;
#elif defined(QUICKJS)
  JavaScriptObject(const JsBridgeContext *, std::string strName, JSValueConst jsObjectValue, const JObjectArrayLocalRef &methods);

  JValue call(JSValueConst jsObjectValue, const JniLocalRef<jobject> &javaMethod, const JObjectArrayLocalRef &args) const;
#endif

  JavaScriptObject() = delete;
  JavaScriptObject(const JavaScriptObject &) = delete;
  JavaScriptObject& operator=(const JavaScriptObject &) = delete;

private:
  const std::string m_name;
  const JsBridgeContext *m_jsBridgeContext;
  MethodMap m_methods;

#if defined(DUKTAPE)
  void *m_jsHeapPtr = nullptr;
#endif
};

#endif
