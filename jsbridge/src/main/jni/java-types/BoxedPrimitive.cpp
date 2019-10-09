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
#include "BoxedPrimitive.h"

#include "Primitive.h"

#if defined(DUKTAPE)
# include "JsBridgeContext.h"
# include "StackChecker.h"
#elif defined(QUICKJS)
# include "JsBridgeContext.h"
#endif

namespace JavaTypes {

BoxedPrimitive::BoxedPrimitive(const JsBridgeContext *jsBridgeContext, std::unique_ptr<Primitive> primitive)
 : JavaType(jsBridgeContext, primitive->boxedId())
 , m_primitive(std::move(primitive)) {
}

#if defined(DUKTAPE)

JValue BoxedPrimitive::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }

  JValue primitiveValue = m_primitive->pop();
  return m_primitive->box(primitiveValue);
}

duk_ret_t BoxedPrimitive::push(const JValue &value) const {
  CHECK_STACK_OFFSET(m_ctx, 1);

  const JniLocalRef<jobject> &jObject = value.getLocalRef();
  if (jObject.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  return m_primitive->push(m_primitive->unbox(value));
}

#elif defined(QUICKJS)

JValue BoxedPrimitive::toJava(JSValueConst v) const {
  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  JValue primitiveValue = m_primitive->toJava(v);
  return JValue(m_primitive->box(primitiveValue));
}

JSValue BoxedPrimitive::fromJava(const JValue &value) const {
  const JniLocalRef<jobject> &jObject = value.getLocalRef();
  if (jObject.isNull()) {
    return JS_NULL;
  }

  JValue unboxedValue = m_primitive->unbox(value);
  return m_primitive->fromJava(unboxedValue);
}

#endif

}  // namespace JavaTypes

