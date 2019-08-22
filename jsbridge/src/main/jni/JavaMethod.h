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
#ifndef _JSBRIDGE_JAVAMETHOD_H
#define _JSBRIDGE_JAVAMETHOD_H

#include "jni-helpers/JniTypes.h"
#include "jni-helpers/JValue.h"
#include "jni-helpers/JniLocalRef.h"
#include <jni.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#if defined(DUKTAPE)
# include "duktape/duktape.h"
#elif defined(QUICKJS)
# include "quickjs/quickjs.h"
#endif

class JavaType;
class JsBridgeContext;

class JavaMethod {
public:
  JavaMethod(const JsBridgeContext *, const JniLocalRef<jsBridgeMethod> &method, std::string methodName,
             bool isLambda);

#if defined(DUKTAPE)
  duk_ret_t invoke(const JsBridgeContext *, const JniRef<jobject> &javaThis) const;
#elif defined(QUICKJS)
  JSValue invoke(const JsBridgeContext *, const JniRef<jobject> &javaThis, int argc, JSValueConst *argv) const;
#endif

private:
  static JValue callLambda(const JsBridgeContext *, const JniRef<jsBridgeMethod> &, const JniRef<jobject> &javaThis, const std::vector<JValue> &args);

  std::string m_methodName;
  bool m_isLambda;
  std::vector<std::unique_ptr<const JavaType>> m_argumentTypes;
  bool m_isVarArgs;
  std::unique_ptr<const JavaType> m_returnValueType;

#if defined(DUKTAPE)
  std::function<duk_ret_t(const JniRef<jobject> &javaThis, const std::vector<JValue> &args)> m_methodBody;
#elif defined(QUICKJS)
  std::function<JSValue(const JniRef<jobject> &javaThis, const std::vector<JValue> &args)> m_methodBody;
#endif
};

#endif
