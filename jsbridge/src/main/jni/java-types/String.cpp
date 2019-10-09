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
#include "String.h"
#include "JsBridgeContext.h"

#if defined(DUKTAPE)
# include "StackChecker.h"
#elif defined(QUICKJS)
# include "QuickJsUtils.h"
#endif

namespace JavaTypes {

String::String(const JsBridgeContext *jsBridgeContext)
 : JavaType(jsBridgeContext, JavaTypeId::String) {
}

#if defined(DUKTAPE)

JValue String::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  // Check if the caller passed in a null string.
  if (duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }

  JStringLocalRef stringLocalRef(m_jniContext, duk_safe_to_string(m_ctx, -1));
  duk_pop(m_ctx);
  return JValue(stringLocalRef);
}

duk_ret_t String::push(const JValue &value) const {
  CHECK_STACK_OFFSET(m_ctx, 1);

  if (value.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  JStringLocalRef jString(value.getLocalRef().staticCast<jstring>());
  if (jString.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  duk_push_string(m_ctx, jString.toUtf8Chars());
  return 1;
}

#elif defined(QUICKJS)

JValue String::toJava(JSValueConst v) const {
  // Check if the caller passed in a null string.
  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  JStringLocalRef stringLocalRef = m_jsBridgeContext->getUtils()->toJString(v);
  return JValue(stringLocalRef);
}

JSValue String::fromJava(const JValue &value) const {
  if (value.isNull()) {
    return JS_NULL;
  }

  JStringLocalRef jString(value.getLocalRef().staticCast<jstring>());
  if (jString.isNull()) {
    return JS_NULL;
  }

  return JS_NewString(m_ctx, jString.toUtf8Chars());
}

#endif

}  // namespace JavaTypes

