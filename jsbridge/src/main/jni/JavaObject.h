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
#ifndef _JSBRIDGE_JAVAOBJECT_H
#define _JSBRIDGE_JAVAOBJECT_H

#include "jni-helpers/JniLocalRef.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include <string>

#if defined(DUKTAPE)
# include "duktape/duktape.h"
#elif defined(QUICKJS)
# include "quickjs/quickjs.h"
#endif

class JsBridgeContext;

class JavaObject {
public:
  JavaObject() = delete;
  ~JavaObject() = delete;

#if defined(DUKTAPE)
  static duk_ret_t push(const JsBridgeContext *, const std::string &strName, const JniLocalRef<jobject> &object, const JObjectArrayLocalRef &methods);
  static duk_ret_t pushLambda(const JsBridgeContext *, const std::string &strName, const JniLocalRef<jobject> &object, const JniLocalRef<jsBridgeMethod> &method);
#elif defined(QUICKJS)
  static JSValue create(const JsBridgeContext *, const std::string &strName, const JniLocalRef<jobject> &object, const JObjectArrayLocalRef &methods);
  static JSValue createLambda(const JsBridgeContext *, const std::string &strName, const JniLocalRef<jobject> &object, const JniLocalRef<jsBridgeMethod> &method);
#endif
};

#endif
