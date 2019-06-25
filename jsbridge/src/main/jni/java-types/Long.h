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
#ifndef _JSBRIDGE_JAVATYPES_LONG_H
#define _JSBRIDGE_JAVATYPES_LONG_H

#include "Primitive.h"

namespace JavaTypes {

class Long : public Primitive {

public:
  Long(const JsBridgeContext *, const JniGlobalRef<jclass>& classRef, const JniGlobalRef<jclass>& boxedClassRef);

#if defined(DUKTAPE)
  JValue pop(bool inScript, const AdditionalData *) const override;
  JValue popArray(uint32_t count, bool expanded, bool inScript, const AdditionalData *) const override;

  duk_ret_t push(const JValue &, bool inScript, const AdditionalData *) const override;
  duk_ret_t pushArray(const JniLocalRef<jarray> &values, bool expand, bool inScript, const AdditionalData *) const override;
#elif defined(QUICKJS)
  JValue toJava(JSValueConst, bool inScript, const AdditionalData *) const override;
  JValue toJavaArray(JSValueConst, bool inScript, const AdditionalData *) const override;

  JSValue fromJava(const JValue &, bool inScript, const AdditionalData *) const override;
  JSValue fromJavaArray(const JniLocalRef<jarray> &, bool inScript, const AdditionalData *) const override;
#endif

  JValue callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                    const std::vector<JValue> &args) const override;

  JniLocalRef<jclass> getArrayClass() const override;

  const char* getUnboxSignature() const override {
    return "()J";
  }

  const char* getUnboxMethodName() const override {
    return "longValue";
  }

  const char* getBoxSignature() const override {
    return "(J)Ljava/lang/Long;";
  }

  bool isInteger() const override {
    return true;  // TODO: check it...
  }
};

}  // namespace JavaTypes

#endif

