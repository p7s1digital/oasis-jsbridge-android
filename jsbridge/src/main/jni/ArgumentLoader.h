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
#ifndef _JSBRIDGE_ARGUMENTLOADER_H
#define _JSBRIDGE_ARGUMENTLOADER_H

#include "JavaType.h"
#include "jni-helpers/JniGlobalRef.h"
#include "jni-helpers/JniTypes.h"
#include "jni-helpers/JniLocalRef.h"
#include <jni.h>
#include <vector>

#if defined(DUKTAPE)
# include "duktape/duktape.h"
#elif defined(QUICKJS)
# include "quickjs/quickjs.h"
#endif

class JavaTypeMap;
class JValue;

class ArgumentLoader {
public:
  ArgumentLoader(const JavaType *javaType, const JniRef<jsBridgeParameter> &parameter, bool inScript);
  ~ArgumentLoader();

  ArgumentLoader(const ArgumentLoader &) = delete;
  ArgumentLoader& operator=(const ArgumentLoader &) = delete;
  ArgumentLoader& operator=(ArgumentLoader &&) = delete;
  ArgumentLoader(ArgumentLoader &&) noexcept;

#if defined(DUKTAPE)
  JValue pop() const;
  JValue popArray(uint32_t count, bool expanded) const;
  JValue popDeferred(const JavaTypeMap *javaType) const;

  duk_ret_t push(const JValue &value) const;
  duk_ret_t pushArray(const JniLocalRef<jarray> &values, bool expand) const;
#elif defined(QUICKJS)
  JValue toJava(JSValueConst) const;
  JValue toJavaArray(JSValueConst) const;
  JValue toJavaDeferred(JSValueConst, const JavaTypeMap *javaType) const;

  JSValue fromJava(const JValue &value) const;
  JSValue fromJavaArray(const JniLocalRef<jarray> &values) const;
#endif

  JValue callMethod(jmethodID, const JniRef<jobject> &javaThis, const std::vector<JValue> &args) const;
  JValue callLambda(const JniRef<jsBridgeMethod> &method, const JniRef<jobject> &javaThis, const std::vector<JValue> &args) const;

  const JavaType *getJavaType() const { return m_javaType; }

private:
  const JavaType *m_javaType;
  JniGlobalRef<jsBridgeParameter> m_parameter;
  const bool m_inScript;
  mutable JavaType::AdditionalData *m_additionalPopData = nullptr;
  mutable JavaType::AdditionalData *m_additionalPushData = nullptr;
  mutable bool m_hasAdditionalPopData = false;
  mutable bool m_hasAdditionalPushData = false;
};

#endif
