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
#ifndef _JSBRIDGE_JAVATYPE_H
#define _JSBRIDGE_JAVATYPE_H

#include "jni-helpers/JniGlobalRef.h"
#include <jni-helpers/JniTypes.h>
#include <jni-helpers/JValue.h>
#include <jni.h>
#include <map>
#include <vector>

#if defined(DUKTAPE)
# include "duktape/duktape.h"
#elif defined(QUICKJS)
extern "C" {
# include "quickjs/quickjs.h"
}
#endif

class JsBridgeContext;
class DuktapeContext;
class JniContext;
class JValue;

// Represents an instance of a Java class.  Handles getting/settings values of the represented type
// to/from Duktape/QuickJS with appropriate conversions and boxing/unboxing.
class JavaType {
public:
  class AdditionalData {
  public:
    virtual ~AdditionalData() = default;

    AdditionalData(const AdditionalData &) = delete;
    AdditionalData& operator=(const AdditionalData &) = delete;

  protected:
    AdditionalData() = default;
  };

  virtual ~JavaType() = default;

#if defined(DUKTAPE)
  virtual JValue pop(bool inScript, const AdditionalData *additionalData) const = 0;
  virtual JValue popArray(uint32_t count, bool expanded, bool inScript, const AdditionalData *additionalData) const;

  virtual duk_ret_t push(const JValue &value, bool inScript, const AdditionalData *additionalData) const = 0;
  virtual duk_ret_t pushArray(const JniLocalRef<jarray> &values, bool expand, bool inScript, const AdditionalData *additionalData) const;
#elif defined(QUICKJS)
  virtual JValue toJava(JSValueConst, bool inScript, const AdditionalData *additionalData) const = 0;
  virtual JValue toJavaArray(JSValueConst, bool inScript, const AdditionalData *additionalData) const;

  virtual JSValue fromJava(const JValue &value, bool inScript, const AdditionalData *additionalData) const = 0;
  virtual JSValue fromJavaArray(const JniLocalRef<jarray> &values, bool inScript, const AdditionalData *additionalData) const;
#endif

  virtual JValue callMethod(jmethodID, const JniRef<jobject> &javaThis, const std::vector<JValue> &args) const;

  virtual bool isPrimitive() const { return false; }
  virtual bool isInteger() const { return false; }
  virtual bool isDeferred() const { return false; }

  const JniRef<jclass> &getClass() const { return m_classRef; }
  virtual JniLocalRef<jclass> getArrayClass() const;

  virtual AdditionalData *createAdditionalPushData(const JniRef<jsBridgeParameter> &) const { return nullptr; }
  virtual AdditionalData *createAdditionalPopData(const JniRef<jsBridgeParameter> &) const { return nullptr; }

  const JsBridgeContext *getJsBridgeContext() const { return m_jsBridgeContext; }
  const JniContext *getJniContext() const { return m_jniContext; }

protected:
  explicit JavaType(const JsBridgeContext *, JniGlobalRef<jclass> classRef);

  const JsBridgeContext * const m_jsBridgeContext;
#if defined(DUKTAPE)
  duk_context * const m_ctx;
#elif defined(QUICKJS)
  JSContext * const m_ctx;
#endif
  const JniContext * const m_jniContext;

private:
  JniGlobalRef<jclass> m_classRef;
};

#endif
