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
#ifndef _JSBRIDGE_JAVATYPES_PRIMITIVE_H
#define _JSBRIDGE_JAVATYPES_PRIMITIVE_H

#include "JavaType.h"

namespace JavaTypes {

class Primitive : public JavaType {

public:
  virtual const char* getUnboxSignature() const = 0;
  virtual const char* getUnboxMethodName() const = 0;
  virtual const char* getBoxSignature() const = 0;
  virtual const char* getBoxMethodName() const { return "valueOf"; }

  bool isPrimitive() const override { return true; }
  const JniGlobalRef<jclass> &boxedClass() const { return m_boxedClassRef; }

protected:
  Primitive(const JsBridgeContext *jsBridgeContext, const JniGlobalRef<jclass>& classRef, const JniGlobalRef<jclass>& boxedClassRef)
   : JavaType(jsBridgeContext, classRef)
   , m_boxedClassRef(boxedClassRef) {
  }

private:
  JniGlobalRef<jclass> m_boxedClassRef;
};

}  // namespace JavaTypes

#endif

