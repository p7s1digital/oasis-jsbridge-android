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
#include "Integer.h"

#include "JsBridgeContext.h"
#include "log.h"
#include "exceptions/JniException.h"
#include "jni-helpers/JArrayLocalRef.h"

#if defined(DUKTAPE)
# include "JsBridgeContext.h"
# include "StackChecker.h"
#endif

namespace JavaTypes {

Integer::Integer(const JsBridgeContext *jsBridgeContext)
 : Primitive(jsBridgeContext, JavaTypeId::Int, JavaTypeId::BoxedInt) {
}

#if defined(DUKTAPE)

JValue Integer::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (!duk_is_number(m_ctx, -1)) {
    const auto message = std::string("Cannot convert return value ") + duk_safe_to_string(m_ctx, -1) + " to int";
    duk_pop(m_ctx);
    throw std::invalid_argument(message);
  }
  if (duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }
  int i = duk_require_int(m_ctx, -1);
  duk_pop(m_ctx);
  return JValue(i);
}

JValue Integer::popArray(uint32_t count, bool expanded) const {
  if (!expanded) {
    count = static_cast<uint32_t>(duk_get_length(m_ctx, -1));
    if (!duk_is_array(m_ctx, -1)) {
      const auto message = std::string("Cannot convert JS value ") + duk_safe_to_string(m_ctx, -1) + " to Array<Integer>";
      duk_pop(m_ctx);  // pop the array
      throw std::invalid_argument(message);
    }
  }

  JArrayLocalRef<jint> intArray(m_jniContext, count);
  jint *elements = intArray.isNull() ? nullptr : intArray.getMutableElements();
  if (elements == nullptr) {
    duk_pop_n(m_ctx, expanded ? count : 1);  // pop the expanded elements or the array
    throw JniException(m_jniContext);
  }

  for (int i = count - 1; i >= 0; --i) {
    if (!expanded) {
      duk_get_prop_index(m_ctx, -1, static_cast<duk_uarridx_t>(i));
    }
    JValue value = pop();
    elements[i] = value.getInt();
  }

  if (!expanded) {
    duk_pop(m_ctx);  // pop the array
  }

  return JValue(intArray);
}

duk_ret_t Integer::push(const JValue &value) const {
  duk_push_int(m_ctx, value.getInt());
  return 1;
}

duk_ret_t Integer::pushArray(const JniLocalRef<jarray> &values, bool expand) const {
  JArrayLocalRef<jint> intArray(values);
  const auto count = intArray.getLength();

  const jint *elements = intArray.getElements();
  if (elements == nullptr) {
    throw JniException(m_jniContext);
  }

  CHECK_STACK_OFFSET(m_ctx, expand ? count : 1);

  if (!expand) {
    duk_push_array(m_ctx);
  }

  for (jsize i = 0; i < count; ++i) {
    duk_push_int(m_ctx, elements[i]);
    if (!expand) {
      duk_put_prop_index(m_ctx, -2, static_cast<duk_uarridx_t>(i));
    }
  }

  return expand ? count : 1;
}

#elif defined(QUICKJS)

namespace {
  inline jint getInt(JSValue v) {
    if (JS_VALUE_GET_TAG(v) == JS_TAG_INT) {
      return JS_VALUE_GET_INT(v);
    }

    if (JS_IsNumber(v)) {
      return jint(JS_VALUE_GET_FLOAT64(v));
    }

    alog_warn("Cannot get int from JS: returning 0");  // TODO: proper exception handling
    return jint();
  }
}

JValue Integer::toJava(JSValueConst v) const {
  if (!JS_IsNumber(v)) {
    throw std::invalid_argument("Cannot convert return value to int");
  }

  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  return JValue(getInt(v));
}

JValue Integer::toJavaArray(JSValueConst v) const {
  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  if (!JS_IsArray(m_ctx, v)) {
    throw std::invalid_argument("Cannot convert JS value to Java array");
  }

  JSValue lengthValue = JS_GetPropertyStr(m_ctx, v, "length");
  assert(JS_IsNumber(lengthValue));
  uint32_t count = JS_VALUE_GET_INT(lengthValue);
  JS_FreeValue(m_ctx, lengthValue);

  JArrayLocalRef<jint> intArray(m_jniContext, count);
  if (intArray.isNull()) {
    throw JniException(m_jniContext);
  }

  jint *elements = intArray.getMutableElements();
  if (elements == nullptr) {
    throw JniException(m_jniContext);
  }

  for (uint32_t i = 0; i < count; ++i) {
    JSValue ev = JS_GetPropertyUint32(m_ctx, v, i);
    elements[i] = getInt(ev);
  }

  intArray.releaseArrayElements();  // copy back elements to Java
  return JValue(intArray);
}

JSValue Integer::fromJava(const JValue &value) const {
  return JS_NewInt32(m_ctx, value.getInt());
}

JSValue Integer::fromJavaArray(const JniLocalRef<jarray> &values) const {
  JArrayLocalRef<jint> intArray(values);
  const auto count = intArray.getLength();

  JSValue jsArray = JS_NewArray(m_ctx);

  const jint *elements = intArray.getElements();
  if (elements == nullptr) {
    JS_FreeValue(m_ctx, jsArray);
    throw JniException(m_jniContext);
  }

  for (jsize i = 0; i < count; ++i) {
    JSValue elementValue = JS_NewInt32(m_ctx, elements[i]);
    JS_SetPropertyUint32(m_ctx, jsArray, static_cast<uint32_t>(i), elementValue);
  }

  return jsArray;
}

#endif

JValue Integer::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                           const std::vector<JValue> &args) const {
  jint returnValue = m_jniContext->callIntMethodA(javaThis, methodId, args);

  // Explicitly release all values now because they won't be used afterwards
  JValue::releaseAll(args);

  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  return JValue(returnValue);
}

JValue Integer::box(const JValue &intValue) const {
  // From int to Integer
  static thread_local jmethodID boxId = m_jniContext->getStaticMethodID(getBoxedJavaClass(), "valueOf", "(I)Ljava/lang/Integer;");
  return JValue(m_jniContext->callStaticObjectMethod(getBoxedJavaClass(), boxId, intValue.getInt()));
}

JValue Integer::unbox(const JValue &boxedValue) const {
  // From Integer to int
  static thread_local jmethodID unboxId = m_jniContext->getMethodID(getBoxedJavaClass(), "intValue", "()I");
  return JValue(m_jniContext->callIntMethod(boxedValue.getLocalRef(), unboxId));
}

}  // namespace JavaTypes

