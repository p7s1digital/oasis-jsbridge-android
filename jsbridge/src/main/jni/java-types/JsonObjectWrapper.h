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
#ifndef _JSBRIDGE_JAVATYPES_JSONOBJECTWRAPPER_H
#define _JSBRIDGE_JAVATYPES_JSONOBJECTWRAPPER_H

#include "JavaType.h"

namespace JavaTypes {

class JsonObjectWrapper : public JavaType {

public:
  JsonObjectWrapper(const JsBridgeContext *, bool isNullable);

#if defined(DUKTAPE)
    JValue pop() const override;
    duk_ret_t push(const JValue &value) const override;
#elif defined(QUICKJS)
    JValue toJava(JSValueConst) const override;
    JSValue fromJava(const JValue &value) const override;
#endif

private:
  bool m_isNullable;
};

}  // namespace JavaTypes

#endif
