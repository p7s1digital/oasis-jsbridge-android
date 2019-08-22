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
#ifndef _JSBRIDGE_JAVATYPES_FUNCTIONX_H
#define _JSBRIDGE_JAVATYPES_FUNCTIONX_H

#include "JavaType.h"
#include "JavaMethod.h"
#include <memory>

namespace JavaTypes {

class FunctionX : public JavaType {

public:
  FunctionX(const JsBridgeContext *, const JniRef<jsBridgeParameter> &);

#if defined(DUKTAPE)
  JValue pop(bool inScript) const override;
  JValue popArray(uint32_t count, bool expanded, bool inScript) const override;
  duk_ret_t push(const JValue &, bool inScript) const override;
  duk_ret_t pushArray(const JniLocalRef<jarray> &values, bool expand, bool inScript) const override;
#elif defined(QUICKJS)
  JValue toJava(JSValueConst, bool inScript) const override;
  JValue toJavaArray(JSValueConst, bool inScript) const override;
  JSValue fromJava(const JValue &, bool inScript) const override;
  JSValue fromJavaArray(const JniLocalRef<jarray> &values, bool inScript) const override;
#endif

private:
  const JniRef<jsBridgeMethod> &getJniJavaMethod() const;
  const std::shared_ptr<JavaMethod> &getCppJavaMethod() const;

  JniGlobalRef<jsBridgeParameter> m_parameter;
  mutable JniGlobalRef<jsBridgeMethod> m_lazyJniJavaMethod;
  mutable std::shared_ptr<JavaMethod> m_lazyCppJavaMethod;
};

}  // namespace JavaTypes

#endif

