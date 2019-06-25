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
#include "Boolean.h"

#include "../JsBridgeContext.h"
#include "jni-helpers/JArrayLocalRef.h"

#ifdef DUKTAPE
# include "JsBridgeContext.h"
# include "StackUnwinder.h"
#endif

namespace JavaTypes {

Boolean::Boolean(const JsBridgeContext *jsBridgeContext, const JniGlobalRef<jclass>& classRef, const JniGlobalRef<jclass>& boxedClassRef)
 : Primitive(jsBridgeContext, classRef, boxedClassRef) {
}

#if defined(DUKTAPE)

JValue Boolean::pop(bool inScript, const AdditionalData *) const {
  if (!inScript && !duk_is_boolean(m_ctx, -1)) {
    const auto message = std::string("Cannot convert return value ") + duk_safe_to_string(m_ctx, -1) + " to boolean";
    duk_pop(m_ctx);
    throw std::invalid_argument(message);
  }
  if (duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }
  bool b = (bool) duk_require_boolean(m_ctx, -1);
  duk_pop(m_ctx);
  return JValue(b);
}

JValue Boolean::popArray(uint32_t count, bool expanded, bool inScript, const AdditionalData *additionalData) const {
  // If we're not expanded, pop the array off the stack no matter what.
  const StackUnwinder _(m_ctx, expanded ? 0 : 1);

  count = expanded ? count : static_cast<uint32_t>(duk_get_length(m_ctx, -1));
  JArrayLocalRef<jboolean> boolArray(m_jniContext, count);
  m_jsBridgeContext->checkRethrowJsError();

  for (int i = count - 1; i >= 0; --i) {
    if (!expanded) {
      duk_get_prop_index(m_ctx, -1, i);
    }
    JValue value = pop(inScript, additionalData);
    boolArray.setElement(i, value.getBool());
  }
  return JValue(boolArray);
}

duk_ret_t Boolean::push(const JValue &value, bool inScript, const AdditionalData *) const {
  duk_push_boolean(m_ctx, (duk_bool_t) value.getBool());
  return 1;
}

duk_ret_t Boolean::pushArray(const JniLocalRef<jarray>& values, bool expand, bool inScript, const AdditionalData *) const {
  JArrayLocalRef<jboolean> boolArray(values);
  const auto count = boolArray.getLength();

  if (!expand) {
    duk_push_array(m_ctx);
  }
  for (jsize i = 0; i < count; ++i) {
    jboolean element = boolArray.getElement(i);
    duk_push_boolean(m_ctx, duk_bool_t(element == JNI_TRUE));
    if (!expand) {
      duk_put_prop_index(m_ctx, -2, static_cast<duk_uarridx_t>(i));
    }
  }
  return expand ? count : 1;
}

#elif defined(QUICKJS)

JValue Boolean::toJava(JSValueConst v, bool inScript, const AdditionalData *) const {
  if (!inScript && !JS_IsBool(v)) {
    const auto message = "Cannot convert return value to boolean";
    throw std::invalid_argument(message);
  }
  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  bool b = (bool) JS_VALUE_GET_BOOL(v);
  return JValue(b);
}

JValue Boolean::toJavaArray(JSValueConst v, bool inScript, const AdditionalData *additionalData) const {
  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  JSValue lengthValue = JS_GetPropertyStr(m_ctx, v, "length");
  assert(JS_IsNumber(lengthValue));
  uint32_t count = JS_VALUE_GET_INT(lengthValue);
  JS_FreeValue(m_ctx, lengthValue);

  JArrayLocalRef<jboolean> boolArray(m_jniContext, count);

  assert(JS_IsArray(m_ctx, v));
  for (uint32_t i = 0; i < count; ++i) {
    JValue elementValue = toJava(JS_GetPropertyUint32(m_ctx, v, i), inScript, additionalData);
    boolArray.setElement(i, elementValue.getBool());
    m_jsBridgeContext->checkRethrowJsError();
  }

  return JValue(boolArray);
}

JSValue Boolean::fromJava(const JValue &value, bool inScript, const AdditionalData *) const {
  return JS_NewBool(m_ctx, value.getBool());
}

JSValue Boolean::fromJavaArray(const JniLocalRef<jarray>& values, bool inScript, const AdditionalData *additionalData) const {
  JArrayLocalRef<jboolean> boolArray(values);
  const auto count = boolArray.getLength();

  JSValue jsArray = JS_NewArray(m_ctx);

  for (jsize i = 0; i < count; ++i) {
    jboolean b = boolArray.getElement(i);
    try {
      JSValue elementValue = fromJava(JValue(b), inScript, additionalData);
      JS_SetPropertyUint32(m_ctx, jsArray, i, elementValue);
    } catch (std::invalid_argument &e) {
      JS_FreeValue(m_ctx, jsArray);
      throw e;
    }
  }

  return jsArray;
}

#endif

JValue Boolean::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                           const std::vector<JValue> &args) const {

  jboolean retVal = m_jniContext->callBooleanMethodA(javaThis, methodId, args);
  m_jsBridgeContext->checkRethrowJsError();

  // Explicitly release all values now because they won't be used afterwards
  JValue::releaseAll(args);

  return JValue((bool) retVal);
}

JniLocalRef<jclass> Boolean::getArrayClass() const {
  return m_jniContext->getObjectClass(JArrayLocalRef<jboolean>(m_jniContext, 0));
}

}  // namespace JavaTypes

