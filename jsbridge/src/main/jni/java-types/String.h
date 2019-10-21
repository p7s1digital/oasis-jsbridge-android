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
#ifndef _JSBRIDGE_JAVATYPES_STRING_H
#define _JSBRIDGE_JAVATYPES_STRING_H

#include "JavaType.h"

namespace JavaTypes {

class String : public JavaType {

public:
  explicit String(const JsBridgeContext *);

#if defined(DUKTAPE)
  JValue pop() const override;
  duk_ret_t push(const JValue &) const override;
#elif defined(QUICKJS)
  JValue toJava(JSValueConst) const override;
  JSValue fromJava(const JValue &) const override;
#endif
};

}  // namespace JavaTypes

#endif

