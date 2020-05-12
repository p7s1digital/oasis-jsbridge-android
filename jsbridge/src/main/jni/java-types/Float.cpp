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
#include "Float.h"

#include "JsBridgeContext.h"
#include "log.h"
#include "exceptions/JniException.h"
#include "jni-helpers/JArrayLocalRef.h"

#ifdef DUKTAPE
# include "JsBridgeContext.h"
# include "StackChecker.h"
#endif

namespace JavaTypes {

Float::Float(const JsBridgeContext *jsBridgeContext)
 : Primitive(jsBridgeContext, JavaTypeId::Float, JavaTypeId::BoxedFloat) {
}

#if defined(DUKTAPE)

JValue Float::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (!duk_is_number(m_ctx, -1)) {
    const auto message = std::string("Cannot convert return value ") + duk_safe_to_string(m_ctx, -1) + " to float";
    duk_pop(m_ctx);
    throw std::invalid_argument(message);
  }

  auto f = static_cast<float>(duk_require_number(m_ctx, -1));
  duk_pop(m_ctx);
  return JValue(f);
}

JValue Float::popArray(uint32_t count, bool expanded) const {
  if (!expanded) {
    count = static_cast<uint32_t>(duk_get_length(m_ctx, -1));
    if (!duk_is_array(m_ctx, -1)) {
      const auto message = std::string("Cannot convert JS value ") + duk_safe_to_string(m_ctx, -1) + " to Array<Float>";
      duk_pop(m_ctx);  // pop the array
      throw std::invalid_argument(message);
    }
  }

  JArrayLocalRef<jfloat> floatArray(m_jniContext, count);
  jfloat *elements = floatArray.isNull() ? nullptr : floatArray.getMutableElements();
  if (elements == nullptr) {
    duk_pop_n(m_ctx, expanded ? count : 1);  // pop the expanded elements or the array
    throw JniException(m_jniContext);
  }

  for (int i = count - 1; i >= 0; --i) {
    if (!expanded) {
      duk_get_prop_index(m_ctx, -1, static_cast<duk_uarridx_t>(i));
    }
    JValue value = pop();
    elements[i] = value.getFloat();
  }

  if (!expanded) {
    duk_pop(m_ctx);  // pop the array
  }

  return JValue(floatArray);
}

duk_ret_t Float::push(const JValue &value) const {
  duk_push_number(m_ctx, value.getFloat());
  return 1;
}

duk_ret_t Float::pushArray(const JniLocalRef<jarray> &values, bool expand) const {
  JArrayLocalRef<jfloat> floatArray(values);
  const auto count = floatArray.getLength();

  const jfloat *elements = floatArray.getElements();
  if (elements == nullptr) {
    throw JniException(m_jniContext);
  }

  CHECK_STACK_OFFSET(m_ctx, expand ? count : 1);

  if (!expand) {
    duk_push_array(m_ctx);
  }

  for (jsize i = 0; i < count; ++i) {
    duk_push_number(m_ctx, elements[i]);
    if (!expand) {
      duk_put_prop_index(m_ctx, -2, static_cast<duk_uarridx_t>(i));
    }
  }

  return expand ? count : 1;
}

#elif defined(QUICKJS)

namespace {
  inline jfloat getFloat(JSValue v) {
    int tag = JS_VALUE_GET_TAG(v);
    if (tag == JS_TAG_INT) {
      return JS_VALUE_GET_INT(v);
    }

    if (JS_TAG_IS_FLOAT64(tag)) {
      return static_cast<jfloat>(JS_VALUE_GET_FLOAT64(v));
    }

    alog_warn("Cannot get float from JS: returning 0");  // TODO: proper exception handling
    return jfloat();
  }
}

JValue Float::toJava(JSValueConst v) const {
  if (!JS_IsNumber(v)) {
    throw std::invalid_argument("Cannot convert return value to float");
  }

  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  return JValue(getFloat(v));
}

JValue Float::toJavaArray(JSValueConst v) const {
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

  JArrayLocalRef<jfloat> floatArray(m_jniContext, count);
  if (floatArray.isNull()) {
    throw JniException(m_jniContext);
  }

  jfloat *elements = floatArray.getMutableElements();
  if (elements == nullptr) {
    throw JniException(m_jniContext);
  }

  for (uint32_t i = 0; i < count; ++i) {
    JSValue ev = JS_GetPropertyUint32(m_ctx, v, i);
    elements[i] = getFloat(ev);
  }

  floatArray.releaseArrayElements();  // copy back elements to Java
  return JValue(floatArray);
}

JSValue Float::fromJava(const JValue &value) const {
  return JS_NewFloat64(m_ctx, value.getFloat());
}

JSValue Float::fromJavaArray(const JniLocalRef<jarray> &values) const {
  JArrayLocalRef<jfloat> floatArray(values);
  const auto count = floatArray.getLength();

  JSValue jsArray = JS_NewArray(m_ctx);

  const jfloat *elements = floatArray.getElements();
  if (elements == nullptr) {
    JS_FreeValue(m_ctx, jsArray);
    throw JniException(m_jniContext);
  }

  for (jsize i = 0; i < count; ++i) {
    JSValue elementValue = JS_NewFloat64(m_ctx, elements[i]);
    JS_SetPropertyUint32(m_ctx, jsArray, static_cast<uint32_t>(i), elementValue);
  }

  return jsArray;
}

#endif

JValue Float::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                         const std::vector<JValue> &args) const {

  jfloat f = m_jniContext->callFloatMethodA(javaThis, methodId, args);

  // Explicitly release all values now because they won't be used afterwards
  JValue::releaseAll(args);

  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  return JValue(f);
}

JValue Float::box(const JValue &floatValue) const {
  // From float to Float
  jmethodID boxId = m_jniContext->getStaticMethodID(getBoxedJavaClass(), "valueOf", "(F)Ljava/lang/Float;");
  return JValue(m_jniContext->callStaticObjectMethod(getBoxedJavaClass(), boxId, floatValue.getFloat()));
}

JValue Float::unbox(const JValue &boxedValue) const {
  // From Float to float
  static thread_local jmethodID unboxId = m_jniContext->getMethodID(getBoxedJavaClass(), "floatValue", "()F");
  return JValue(m_jniContext->callFloatMethod(boxedValue.getLocalRef(), unboxId));
}

}  // namespace JavaTypes

