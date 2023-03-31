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
#ifndef _JSBRIDGE_JAVATYPES_OBJECT_H
#define _JSBRIDGE_JAVATYPES_OBJECT_H

#include "JavaType.h"

namespace JavaTypes {

class Object : public JavaType {

public:
  Object(const JsBridgeContext *, std::optional<JniGlobalRef<jstring>> optJavaName);

#if defined(DUKTAPE)
  JValue pop() const override;
  duk_ret_t push(const JValue &value) const override;
#elif defined(QUICKJS)
  JValue toJava(JSValueConst v) const override;
  JSValue fromJava(const JValue &value) const override;
#endif

private:
  friend class Array;

  const std::optional<JniGlobalRef<jstring>> m_optJavaName;

  JStringLocalRef getJavaName(const JniLocalRef<jobject> &object) const;
  JavaType *newJavaType(const JniLocalRef<jobject> &jobject) const;
  JavaType *newJavaTypeFromJavaName(std::u16string_view javaName) const;
  JavaType *newJavaTypeFromJavaTypeId(JavaTypeId id, std::unique_ptr<const JavaType> &&componentJavaType) const;
};

}  // namespace JavaTypes

#endif
