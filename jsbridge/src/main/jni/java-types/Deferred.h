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
#ifndef _JSBRIDGE_JAVATYPES_DEFERRED_H
#define _JSBRIDGE_JAVATYPES_DEFERRED_H

#include "JavaType.h"

class ArgumentLoader;

namespace JavaTypes {

class Deferred : public JavaType {

public:
  Deferred(const JsBridgeContext *, const JniGlobalRef <jclass> &classRef);

#if defined(DUKTAPE)
  JValue pop(bool inScript, const AdditionalData *) const override;
  JValue pop(bool inScript, const JavaType *genericArgumentType, const JniRef<jsBridgeParameter> &genericArgumentParameter) const;
  duk_ret_t push(const JValue &, bool inScript, const AdditionalData *) const override;
#elif defined(QUICKJS)
  JValue toJava(JSValueConst, bool inScript, const AdditionalData *) const override;
  JValue toJava(JSValueConst, bool inScript, const JavaType *genericArgumentType, const JniRef<jsBridgeParameter> &genericArgumentParameter) const;
  JSValue fromJava(const JValue &, bool inScript, const AdditionalData *) const override;
#endif

  bool isDeferred() const override { return true; }

  AdditionalData *createAdditionalPushData(const JniRef<jsBridgeParameter> &) const override;
  AdditionalData *createAdditionalPopData(const JniRef<jsBridgeParameter> &) const override;

private:
  class AdditionalPushData;
  class AdditionalPopData;

  const JavaType *m_objectType;
};

}  // namespace JavaTypes

#endif

