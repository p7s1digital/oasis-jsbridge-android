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
#include <utils.h>
#include "Double.h"

#include "../JsBridgeContext.h"
#include "jni-helpers/JArrayLocalRef.h"

#ifdef DUKTAPE
# include "JsBridgeContext.h"
# include "StackChecker.h"
# include "StackUnwinder.h"
#endif

namespace JavaTypes {

Double::Double(const JsBridgeContext *jsBridgeContext, const JniGlobalRef<jclass>& classRef, const JniGlobalRef<jclass>& boxedClassRef)
 : Primitive(jsBridgeContext, classRef, boxedClassRef) {
}

#if defined(DUKTAPE)

JValue Double::pop(bool inScript, const AdditionalData *) const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (!inScript && !duk_is_number(m_ctx, -1)) {
    const auto message =
        std::string("Cannot convert return value ") + duk_safe_to_string(m_ctx, -1) + " to double";
    duk_pop(m_ctx);
    throw std::invalid_argument(message);
  }

  double d = duk_require_number(m_ctx, -1);
  duk_pop(m_ctx);
  return JValue(d);
}

JValue Double::popArray(uint32_t count, bool expanded, bool inScript, const AdditionalData *additionalData) const {
  // If we're not expanded, pop the array off the stack no matter what.
  const StackUnwinder _(m_ctx, expanded ? 0 : 1);

  count = expanded ? count : static_cast<uint32_t>(duk_get_length(m_ctx, -1));
  JArrayLocalRef<jdouble> doubleArray(m_jniContext, count);
  m_jsBridgeContext->checkRethrowJsError();

  for (int i = count - 1; i >= 0; --i) {
    if (!expanded) {
      duk_get_prop_index(m_ctx, -1, i);
    }
    JValue value = pop(inScript, additionalData);
    doubleArray.setElement(i, value.getDouble());
  }

  return JValue(doubleArray);
}

duk_ret_t Double::push(const JValue &value, bool inScript, const AdditionalData *) const {
  duk_push_number(m_ctx, value.getDouble());
  return 1;
}

duk_ret_t Double::pushArray(const JniLocalRef<jarray> &values, bool expand, bool inScript, const AdditionalData *) const {
  JArrayLocalRef<jdouble> doubleArray(values);
  const auto count = doubleArray.getLength();

  if (!expand) {
    duk_push_array(m_ctx);
  }

  for (jsize i = 0; i < count; ++i) {
    jdouble element = doubleArray.getElement(i);
    duk_push_number(m_ctx, element);
    if (!expand) {
      duk_put_prop_index(m_ctx, -2, static_cast<duk_uarridx_t>(i));
    }
  }

  return expand ? count : 1;
}

#elif defined(QUICKJS)

JValue Double::toJava(JSValueConst v, bool inScript, const AdditionalData *) const {
  if (!inScript && !JS_IsNumber(v)) {
    const auto message = "Cannot convert return value to double";
    throw std::invalid_argument(message);
  }

  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  double d;
  if (JS_IsInteger(v)) {
    d = JS_VALUE_GET_INT(v);
  } else {
    d = JS_VALUE_GET_FLOAT64(v);
  }
  return JValue(d);
}

JValue Double::toJavaArray(JSValueConst v, bool inScript, const AdditionalData *additionalData) const {
  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  JSValue lengthValue = JS_GetPropertyStr(m_ctx, v, "length");
  assert(JS_IsNumber(lengthValue));
  uint32_t count = JS_VALUE_GET_INT(lengthValue);
  JS_FreeValue(m_ctx, lengthValue);

  JArrayLocalRef<jdouble> doubleArray(m_jniContext, count);

  assert(JS_IsArray(m_ctx, v));
  for (uint32_t i = 0; i < count; ++i) {
    JValue elementValue = toJava(JS_GetPropertyUint32(m_ctx, v, i), inScript, additionalData);
    doubleArray.setElement(i, elementValue.getDouble());
    m_jsBridgeContext->checkRethrowJsError();
  }

  return JValue(doubleArray);
}

JSValue Double::fromJava(const JValue &value, bool inScript, const AdditionalData *) const {
  return JS_NewFloat64(m_ctx, value.getDouble());
}

JSValue Double::fromJavaArray(const JniLocalRef<jarray> &values, bool inScript, const AdditionalData *additionalData) const {
  JArrayLocalRef<jdouble> doubleArray(values);
  const auto count = doubleArray.getLength();

  JSValue jsArray = JS_NewArray(m_ctx);

  for (jsize i = 0; i < count; ++i) {
   jdouble d = doubleArray.getElement(i);
    try {
      JSValue elementValue = fromJava(JValue(d), inScript, additionalData);
      JS_SetPropertyUint32(m_ctx, jsArray, i, elementValue);
    } catch (std::invalid_argument &e) {
      JS_FreeValue(m_ctx, jsArray);
      throw e;
    }
  }

  return jsArray;
}

#endif

JValue Double::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                          const std::vector<JValue> &args) const {

  jdouble d = m_jniContext->callDoubleMethodA(javaThis, methodId, args);
  m_jsBridgeContext->checkRethrowJsError();

  // Explicitly release all values now because they won't be used afterwards
  JValue::releaseAll(args);

  return JValue(d);
}

JniLocalRef<jclass> Double::getArrayClass() const {
  return m_jniContext->getObjectClass(JArrayLocalRef<jdouble>(m_jniContext, 0));
}

}  // namespace JavaTypes

