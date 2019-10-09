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
#include "Double.h"
#include "JsBridgeContext.h"
#include "log.h"
#include "exceptions/JniException.h"
#include "jni-helpers/JArrayLocalRef.h"

#ifdef DUKTAPE
# include "JsBridgeContext.h"
# include "StackChecker.h"
#endif

namespace JavaTypes {

Double::Double(const JsBridgeContext *jsBridgeContext)
 : Primitive(jsBridgeContext, JavaTypeId::Double, JavaTypeId::BoxedDouble) {
}

#if defined(DUKTAPE)

JValue Double::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (!duk_is_number(m_ctx, -1)) {
    const auto message = std::string("Cannot convert JS value ") + duk_safe_to_string(m_ctx, -1) + " to double";
    duk_pop(m_ctx);
    throw std::invalid_argument(message);
  }

  double d = duk_require_number(m_ctx, -1);
  duk_pop(m_ctx);
  return JValue(d);
}

JValue Double::popArray(uint32_t count, bool expanded) const {
  if (!expanded) {
    count = static_cast<uint32_t>(duk_get_length(m_ctx, -1));
    if (!duk_is_array(m_ctx, -1)) {
      const auto message = std::string("Cannot convert JS value ") + duk_safe_to_string(m_ctx, -1) + " to Array<Double>";
      duk_pop(m_ctx);  // pop the array
      throw std::invalid_argument(message);
    }
  }

  JArrayLocalRef<jdouble> doubleArray(m_jniContext, count);
  jdouble *elements = doubleArray.isNull() ? nullptr : doubleArray.getMutableElements();
  if (elements == nullptr) {
    duk_pop_n(m_ctx, expanded ? count : 1);  // pop the expanded elements or the array
    throw JniException(m_jniContext);
  }

  for (int i = count - 1; i >= 0; --i) {
    if (!expanded) {
      duk_get_prop_index(m_ctx, -1, static_cast<duk_uarridx_t>(i));
    }
    JValue value = pop();
    elements[i] = value.getDouble();
  }

  if (!expanded) {
    duk_pop(m_ctx);  // pop the array
  }

  return JValue(doubleArray);
}

duk_ret_t Double::push(const JValue &value) const {
  duk_push_number(m_ctx, value.getDouble());
  return 1;
}

duk_ret_t Double::pushArray(const JniLocalRef<jarray> &values, bool expand) const {
  JArrayLocalRef<jdouble> doubleArray(values);
  const auto count = doubleArray.getLength();

  const jdouble *elements = doubleArray.getElements();
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
  inline jdouble getDouble(JSValue v) {
    if (JS_IsInteger(v)) {
      return JS_VALUE_GET_INT(v);
    }

    if (JS_IsNumber(v)) {
      return static_cast<jdouble>(JS_VALUE_GET_FLOAT64(v));
    }

    alog_warn("Cannot get double from JS: returning 0");  // TODO: proper exception handling
    return jdouble();
  }
}

JValue Double::toJava(JSValueConst v) const {
  if (!JS_IsNumber(v)) {
    throw std::invalid_argument("Cannot convert return value to double");
  }

  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  return JValue(getDouble(v));
}

JValue Double::toJavaArray(JSValueConst v) const {
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

  JArrayLocalRef<jdouble> doubleArray(m_jniContext, count);
  if (doubleArray.isNull()) {
    throw JniException(m_jniContext);
  }

  jdouble *elements = doubleArray.getMutableElements();
  if (elements == nullptr) {
    throw JniException(m_jniContext);
  }

  for (uint32_t i = 0; i < count; ++i) {
    JSValue ev = JS_GetPropertyUint32(m_ctx, v, i);
    elements[i] = getDouble(ev);
  }

  doubleArray.releaseArrayElements();  // copy back elements to Java
  return JValue(doubleArray);
}

JSValue Double::fromJava(const JValue &value) const {
  return JS_NewFloat64(m_ctx, value.getDouble());
}

JSValue Double::fromJavaArray(const JniLocalRef<jarray> &values) const {
  JArrayLocalRef<jdouble> doubleArray(values);
  const auto count = doubleArray.getLength();

  JSValue jsArray = JS_NewArray(m_ctx);

  const jdouble *elements = doubleArray.getElements();
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

JValue Double::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                          const std::vector<JValue> &args) const {

  jdouble d = m_jniContext->callDoubleMethodA(javaThis, methodId, args);

  // Explicitly release all values now because they won't be used afterwards
  JValue::releaseAll(args);

  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  return JValue(d);
}

JValue Double::box(const JValue &doubleValue) const {
  // From double to Double
  static thread_local jmethodID boxId = m_jniContext->getStaticMethodID(getBoxedJavaClass(), "valueOf", "(D)Ljava/lang/Double;");
  return JValue(m_jniContext->callStaticObjectMethod(getBoxedJavaClass(), boxId, doubleValue.getDouble()));
}

JValue Double::unbox(const JValue &boxedValue) const {
  // From Double to double
  static thread_local jmethodID unboxId = m_jniContext->getMethodID(getBoxedJavaClass(), "doubleValue", "()D");
  return JValue(m_jniContext->callDoubleMethod(boxedValue.getLocalRef(), unboxId));
}

}  // namespace JavaTypes

