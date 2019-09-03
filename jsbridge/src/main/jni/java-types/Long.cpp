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

Long::Long(const JsBridgeContext *jsBridgeContext)
  : Primitive(jsBridgeContext, JavaTypeId::Long, JavaTypeId::BoxedLong) {
}

#if defined(DUKTAPE)

JValue Long::pop(bool inScript) const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (!duk_is_number(m_ctx, -1)) {
    const auto message = std::string("Cannot convert return value ") + duk_safe_to_string(m_ctx, -1) + " to long";
    duk_pop(m_ctx);
    CHECK_STACK_NOW();
    m_jsBridgeContext->throwTypeException(message, inScript);
  }
  if (duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }
  auto l = (jlong) duk_require_number(m_ctx, -1);
  duk_pop(m_ctx);
  return JValue(l);
}

JValue Long::popArray(uint32_t count, bool expanded, bool inScript) const {
  // If we're not expanded, pop the array off the stack no matter what.
  const StackUnwinder _(m_ctx, expanded ? 0 : 1);

  if (!expanded) {
    count = static_cast<uint32_t>(duk_get_length(m_ctx, -1));
    if (!duk_is_array(m_ctx, -1)) {
      const auto message = std::string("Cannot convert JS value ") + duk_safe_to_string(m_ctx, -1) + " to Array<Boolean>";
      m_jsBridgeContext->throwTypeException(message, inScript);
    }
  }

  JArrayLocalRef<jlong> longArray(m_jniContext, count);
  for (int i = count - 1; i >= 0; --i) {
    if (!expanded) {
      duk_get_prop_index(m_ctx, -1, i);
    }
    JValue value = pop(inScript);
    longArray.setElement(i, value.getLong());
  }
  return JValue(longArray);
}

duk_ret_t Long::push(const JValue &value, bool inScript) const {
  duk_push_number(m_ctx, (duk_double_t) value.getLong());
  return 1;
}

duk_ret_t Long::pushArray(const JniLocalRef<jarray> &values, bool expand, bool inScript) const {
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

JValue Long::toJava(JSValueConst v, bool inScript) const {
  if (!JS_IsNumber(v)) {
    const char *message = "Cannot convert return value to long";
    m_jsBridgeContext->throwTypeException(message, inScript);
    return JValue();
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

JValue Long::toJavaArray(JSValueConst v, bool inScript) const {
  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  if (!JS_IsArray(m_ctx, v)) {
    m_jsBridgeContext->throwTypeException("Cannot convert JS value to Java array", inScript);
    return JValue();
  }

  JSValue lengthValue = JS_GetPropertyStr(m_ctx, v, "length");
  assert(JS_IsNumber(lengthValue));
  uint32_t count = JS_VALUE_GET_INT(lengthValue);
  JS_FreeValue(m_ctx, lengthValue);

  JArrayLocalRef<jlong> longArray(m_jniContext, count);

  for (uint32_t i = 0; i < count; ++i) {
    JValue elementValue = toJava(JS_GetPropertyUint32(m_ctx, v, i), inScript);
    longArray.setElement(i, elementValue.getLong());
    m_jsBridgeContext->checkRethrowJsError();
  }

  return JValue(longArray);
}

JSValue Long::fromJava(const JValue &value, bool inScript) const {
  return JS_NewInt64(m_ctx, value.getLong());
}

JSValue Long::fromJavaArray(const JniLocalRef<jarray> &values, bool inScript) const {
  JArrayLocalRef<jlong> longArray(values);
  const auto count = longArray.getLength();

  JSValue jsArray = JS_NewArray(m_ctx);

  for (jsize i = 0; i < count; ++i) {
    jlong lValue = longArray.getElement(i);
    try {
      JSValue elementValue = fromJava(JValue(lValue), inScript);
      JS_SetPropertyUint32(m_ctx, jsArray, static_cast<uint32_t>(i), elementValue);
    } catch (std::invalid_argument &e) {
      JS_FreeValue(m_ctx, jsArray);
      throw e;
    }
  }

  return jsArray;
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

JValue Long::box(const JValue &longValue) const {
  // From long to Long
  static thread_local jmethodID boxId = m_jniContext->getStaticMethodID(getBoxedJavaClass(), "valueOf", "(J)Ljava/lang/Long;");
  return JValue(m_jniContext->callStaticObjectMethod(getBoxedJavaClass(), boxId, longValue.getLong()));
}

JValue Long::unbox(const JValue &boxedValue) const {
  // From Long to long
  static thread_local jmethodID unboxId = m_jniContext->getMethodID(getBoxedJavaClass(), "longValue", "()J");
  return JValue(m_jniContext->callLongMethod(boxedValue.getLocalRef(), unboxId));
}

}  // namespace JavaTypes

