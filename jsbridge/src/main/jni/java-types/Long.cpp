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
#include "Long.h"

#include "../JsBridgeContext.h"
#include "jni-helpers/JArrayLocalRef.h"

#ifdef DUKTAPE
# include "JsBridgeContext.h"
# include "StackChecker.h"
# include "StackUnwinder.h"
#endif

namespace JavaTypes {

Long::Long(const JsBridgeContext *jsBridgeContext, const JniGlobalRef<jclass>& classRef, const JniGlobalRef<jclass>& boxedClassRef)
  : Primitive(jsBridgeContext, classRef, boxedClassRef) {
}

#if defined(DUKTAPE)

JValue Long::pop(bool inScript, const AdditionalData *) const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (!inScript && !duk_is_number(m_ctx, -1)) {
    const auto message = std::string("Cannot convert return value ") + duk_safe_to_string(m_ctx, -1) + " to long";
    duk_pop(m_ctx);
    throw std::invalid_argument(message);
  }
  if (duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }
  auto l = (jlong) duk_require_number(m_ctx, -1);
  duk_pop(m_ctx);
  return JValue(l);
}

JValue Long::popArray(uint32_t count, bool expanded, bool inScript, const AdditionalData *additionalData) const {
  // If we're not expanded, pop the array off the stack no matter what.
  const StackUnwinder _(m_ctx, expanded ? 0 : 1);

  count = expanded ? count : static_cast<uint32_t>(duk_get_length(m_ctx, -1));
  JArrayLocalRef<jlong> longArray(m_jniContext, count);
  for (int i = count - 1; i >= 0; --i) {
    if (!expanded) {
      duk_get_prop_index(m_ctx, -1, i);
    }
    JValue value = pop(inScript, additionalData);
    longArray.setElement(i, value.getLong());
  }
  return JValue(longArray);
}

duk_ret_t Long::push(const JValue &value, bool inScript, const AdditionalData *) const {
  duk_push_number(m_ctx, (duk_double_t) value.getLong());
  return 1;
}

duk_ret_t Long::pushArray(const JniLocalRef<jarray> &values, bool expand, bool inScript, const AdditionalData *) const {
  JArrayLocalRef<jlong> longArray(values);
  const auto count = longArray.getLength();

  if (!expand) {
    duk_push_array(m_ctx);
  }

  for (jsize i = 0; i < count; ++i) {
    jlong element = longArray.getElement(i);
    duk_push_number(m_ctx, (duk_double_t) element);
    if (!expand) {
      duk_put_prop_index(m_ctx, -2, static_cast<duk_uarridx_t>(i));
    }
  }
  return expand ? count : 1;
}

#elif defined(QUICKJS)

JValue Long::toJava(JSValueConst v, bool inScript, const AdditionalData *) const {
  if (!inScript && !JS_IsNumber(v)) {
    const auto message = "Cannot convert return value to long";
    throw std::invalid_argument(message);
  }
  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  jlong l;
  if (JS_IsInteger(v)) {
    l = JS_VALUE_GET_INT(v);
  } else {
    l = long(JS_VALUE_GET_FLOAT64(v));
  }
  return JValue(l);
}

JValue Long::toJavaArray(JSValueConst v, bool inScript, const AdditionalData *additionalData) const {
  // TODO
  return JValue();
}

JSValue Long::fromJava(const JValue &value, bool inScript, const AdditionalData *) const {
  return JS_NewInt64(m_ctx, value.getLong());
}

JSValue Long::fromJavaArray(const JniLocalRef<jarray> &values, bool inScript, const AdditionalData *) const {
  // TODO
  return JS_UNDEFINED;
}

#endif

JValue Long::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                  const std::vector<JValue> &args) const {
  jlong l = m_jniContext->callLongMethodA(javaThis, methodId, args);
  m_jsBridgeContext->checkRethrowJsError();

  // Explicitly release all values now because they won't be used afterwards
  JValue::releaseAll(args);

  return JValue(l);
}

JniLocalRef<jclass> Long::getArrayClass() const {
  return m_jniContext->getObjectClass(JArrayLocalRef<jlong>(m_jniContext, 0));
}

}  // namespace JavaTypes

