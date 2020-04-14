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

#include "JsBridgeContext.h"
#include "exceptions/JniException.h"
#include "jni-helpers/JArrayLocalRef.h"
#include "log.h"

#ifdef DUKTAPE
# include "JsBridgeContext.h"
# include "StackChecker.h"
#endif

namespace JavaTypes {

Long::Long(const JsBridgeContext *jsBridgeContext)
  : Primitive(jsBridgeContext, JavaTypeId::Long, JavaTypeId::BoxedLong) {
}

#if defined(DUKTAPE)

JValue Long::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (!duk_is_number(m_ctx, -1)) {
    const auto message = std::string("Cannot convert return value ") + duk_safe_to_string(m_ctx, -1) + " to long";
    duk_pop(m_ctx);
    throw std::invalid_argument(message);
  }
  if (duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }
  auto l = (jlong) duk_require_number(m_ctx, -1);  // no real Duktape for int64 so needing a cast from double
  duk_pop(m_ctx);
  return JValue(l);
}

JValue Long::popArray(uint32_t count, bool expanded) const {
  if (!expanded) {
    count = static_cast<uint32_t>(duk_get_length(m_ctx, -1));
    if (!duk_is_array(m_ctx, -1)) {
      const auto message = std::string("Cannot convert JS value ") + duk_safe_to_string(m_ctx, -1) + " to Array<Boolean>";
      duk_pop(m_ctx);  // pop the array
      throw std::invalid_argument(message);
    }
  }

  JArrayLocalRef<jlong> longArray(m_jniContext, count);
  jlong *elements = longArray.isNull() ? nullptr : longArray.getMutableElements();
  if (elements == nullptr) {
    duk_pop_n(m_ctx, expanded ? count : 1);  // pop the expanded elements or the array
    throw JniException(m_jniContext);
  }

  for (int i = count - 1; i >= 0; --i) {
    if (!expanded) {
      duk_get_prop_index(m_ctx, -1, static_cast<duk_uarridx_t>(i));
    }
    JValue value = pop();
    elements[i] = value.getLong();
  }

  if (!expanded) {
    duk_pop(m_ctx);  // pop the array
  }

  return JValue(longArray);
}

duk_ret_t Long::push(const JValue &value) const {
  duk_push_number(m_ctx, static_cast<duk_double_t>(value.getLong()));  // no real Duktape for int64 so needing a cast to double
  return 1;
}

duk_ret_t Long::pushArray(const JniLocalRef<jarray> &values, bool expand) const {
  JArrayLocalRef<jlong> longArray(values);
  const auto count = longArray.getLength();

  const jlong *elements = longArray.getElements();
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
  inline jlong getLong(JSContext *ctx, JSValue v) {
    if (JS_IsBigInt(NULL, v)) {
      int64_t i64;
      JS_ToInt64(ctx, &i64, v);
      return i64;
    }

    if (JS_IsNumber(v)) {
      return jlong(JS_VALUE_GET_FLOAT64(v));
    }

    alog_warn("Cannot get long from JS: returning 0");  // TODO: proper exception handling
    return jlong();
  }
}

JValue Long::toJava(JSValueConst v) const {
  if (!JS_IsNumber(v)) {
    throw std::invalid_argument("Cannot convert return value to long");
  }

  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  return JValue(getLong(m_ctx, v));
}

JValue Long::toJavaArray(JSValueConst v) const {
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

  JArrayLocalRef<jlong> longArray(m_jniContext, count);
  if (longArray.isNull()) {
    throw JniException(m_jniContext);
  }

  jlong *elements = longArray.getMutableElements();
  if (elements == nullptr) {
    throw JniException(m_jniContext);
  }

  for (uint32_t i = 0; i < count; ++i) {
    JSValue ev = JS_GetPropertyUint32(m_ctx, v, i);
    elements[i] = getLong(m_ctx, ev);
  }

  longArray.releaseArrayElements();  // copy back elements to Java
  return JValue(longArray);
}

JSValue Long::fromJava(const JValue &value) const {
  return JS_NewInt64(m_ctx, value.getLong());
}

JSValue Long::fromJavaArray(const JniLocalRef<jarray> &values) const {
  JArrayLocalRef<jlong> longArray(values);
  const auto count = longArray.getLength();

  JSValue jsArray = JS_NewArray(m_ctx);

  const jlong *elements = longArray.getElements();
  if (elements == nullptr) {
    JS_FreeValue(m_ctx, jsArray);
    throw JniException(m_jniContext);
  }

  for (jsize i = 0; i < count; ++i) {
    JSValue elementValue = JS_NewInt64(m_ctx, elements[i]);
    JS_SetPropertyUint32(m_ctx, jsArray, static_cast<uint32_t>(i), elementValue);
  }

  return jsArray;
}

#endif

JValue Long::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                        const std::vector<JValue> &args) const {
  jlong l = m_jniContext->callLongMethodA(javaThis, methodId, args);

  // Explicitly release all values now because they won't be used afterwards
  JValue::releaseAll(args);

  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

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

