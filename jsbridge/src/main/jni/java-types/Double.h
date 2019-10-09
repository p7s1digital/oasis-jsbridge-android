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
#ifndef _JSBRIDGE_JAVATYPES_DOUBLE_H
#define _JSBRIDGE_JAVATYPES_DOUBLE_H

#include "Primitive.h"

namespace JavaTypes {

class Double : public Primitive {

public:
  Double(const JsBridgeContext *);

#if defined(DUKTAPE)
  JValue pop() const override;
  JValue popArray(uint32_t count, bool expanded) const override;

  duk_ret_t push(const JValue &) const override;
  duk_ret_t pushArray(const JniLocalRef<jarray> &values, bool expand) const override;
#elif defined(QUICKJS)
  JValue toJava(JSValueConst) const override;
  JValue toJavaArray(JSValueConst) const override;

  JSValue fromJava(const JValue &) const override;
  JSValue fromJavaArray(const JniLocalRef<jarray> &values) const override;
#endif

  JValue callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                    const std::vector<JValue> &args) const override;

  JavaTypeId arrayId() const override { return JavaTypeId::DoubleArray; }

private:
  JValue box(const JValue &) const override;
  JValue unbox(const JValue &) const override;
};

}  // namespace JavaTypes

#endif

