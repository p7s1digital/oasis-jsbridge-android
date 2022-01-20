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
#ifndef _JSBRIDGE_JAVATYPES_AIDLINTERFACE_H
#define _JSBRIDGE_JAVATYPES_AIDLINTERFACE_H

#include "JavaType.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include <memory>

namespace JavaTypes {

class AidlInterface : public JavaType {

public:
  AidlInterface(const JsBridgeContext *, const JniRef<jsBridgeParameter> &);

#if defined(DUKTAPE)
  JValue pop() const override;
  JValue popArray(uint32_t count, bool expanded) const override;
  duk_ret_t push(const JValue &) const override;
  duk_ret_t pushArray(const JniLocalRef<jarray> &values, bool expand) const override;
#elif defined(QUICKJS)
  JValue toJava(JSValueConst) const override;
  JSValue fromJava(const JValue &) const override;
#endif

private:
  JObjectArrayLocalRef getJniJavaMethods() const;

  JniGlobalRef<jsBridgeParameter> m_parameter;
  mutable JniGlobalRef<jsBridgeParameter> m_lazyJniAidlStub;
  mutable JniGlobalRef<jobjectArray> m_lazyJniJavaMethods;
};

}  // namespace JavaTypes

#endif

