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

namespace JavaTypes {

class Deferred : public JavaType {

public:
  static const char *PROMISE_COMPONENT_TYPE_PROP_NAME;

  Deferred(const JsBridgeContext *, std::unique_ptr<const JavaType> &&componentType);

#if defined(DUKTAPE)
  JValue pop(bool inScript) const override;
  duk_ret_t push(const JValue &, bool inScript) const override;
#elif defined(QUICKJS)
  JValue toJava(JSValueConst, bool inScript) const override;
  JSValue fromJava(const JValue &, bool inScript) const override;
#endif

  bool isDeferred() const override { return true; }

  static void completeJsPromise(const JsBridgeContext *, const std::string &strId, bool isFulfilled, const JniLocalRef<jobject> &value);

private:
  std::shared_ptr<const JavaType> m_componentType;
};

}  // namespace JavaTypes

#endif

