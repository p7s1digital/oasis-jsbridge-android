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
#include "Array.h"

#include "JsBridgeContext.h"
#include "Primitive.h"
#include "jni-helpers/JValue.h"
#include <string>

namespace {
  JavaTypeId getArrayId(const JavaType *componentType) {
    auto primitive = dynamic_cast<const JavaTypes::Primitive *>(componentType);
    if (primitive == nullptr) {
      return JavaTypeId::ObjectArray;
    }
    return primitive->arrayId();
  }
}

namespace JavaTypes {

Array::Array(const JsBridgeContext *jsBridgeContext, std::unique_ptr<const JavaType> &&componentType)
 : JavaType(jsBridgeContext, getArrayId(componentType.get()))
 , m_componentType(std::move(componentType)) {
}

#if defined(DUKTAPE)

#include "StackChecker.h"

JValue Array::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }

  if (!duk_is_array(m_ctx, -1)) {
    const auto message = std::string("Cannot convert ") + duk_safe_to_string(m_ctx, -1) + " to array";
    duk_pop(m_ctx);
    throw std::invalid_argument(message);
  }

  return m_componentType->popArray(1, false);
}

duk_ret_t Array::push(const JValue &value) const {
  CHECK_STACK_OFFSET(m_ctx, 1);

  JniLocalRef<jarray> jArray(value.getLocalRef().staticCast<jarray>());

  if (jArray.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  return m_componentType->pushArray(jArray, false);
}

#elif defined(QUICKJS)

JValue Array::toJava(JSValueConst v) const {
  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  if (!JS_IsArray(m_ctx, v)) {
    throw std::invalid_argument("Cannot convert value to array");
  }

  return m_componentType->toJavaArray(v);
}

JSValue Array::fromJava(const JValue &value) const {
  JniLocalRef<jarray> jArray(value.getLocalRef().staticCast<jarray>());

  if (jArray.isNull()) {
    return JS_NULL;
  }

  return m_componentType->fromJavaArray(jArray);
}

#endif

}  // namespace JavaTypes

