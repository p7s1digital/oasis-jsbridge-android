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
#include "Short.h"

#include "JsBridgeContext.h"
#include "exceptions/JniException.h"
#include "jni-helpers/JArrayLocalRef.h"
#include "log.h"

#ifdef DUKTAPE
# include "JsBridgeContext.h"
# include "StackChecker.h"
#endif

namespace JavaTypes {

Short::Short(const JsBridgeContext *jsBridgeContext)
  : Primitive(jsBridgeContext, JavaTypeId::Short, JavaTypeId::BoxedShort) {
}

#if defined(DUKTAPE)

JValue Short::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (!duk_is_number(m_ctx, -1)) {
    const auto message = std::string("Cannot convert return value ") + duk_safe_to_string(m_ctx, -1) + " to short";
    duk_pop(m_ctx);
    throw std::invalid_argument(message);
  }
  if (duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }
  auto l = (jshort) duk_require_number(m_ctx, -1);  // no real Duktape for int64 so needing a cast from double
  duk_pop(m_ctx);
  return JValue(l);
}

JValue Short::popArray(uint32_t count, bool expanded) const {
  if (!expanded) {
    count = static_cast<uint32_t>(duk_get_length(m_ctx, -1));
    if (!duk_is_array(m_ctx, -1)) {
      const auto message = std::string("Cannot convert JS value ") + duk_safe_to_string(m_ctx, -1) + " to Array<Short>";
      duk_pop(m_ctx);  // pop the array
      throw std::invalid_argument(message);
    }
  }

  JArrayLocalRef<jshort> shortArray(m_jniContext, count);
  jshort *elements = shortArray.isNull() ? nullptr : shortArray.getMutableElements();
  if (elements == nullptr) {
    duk_pop_n(m_ctx, expanded ? count : 1);  // pop the expanded elements or the array
    throw JniException(m_jniContext);
  }

  for (int i = count - 1; i >= 0; --i) {
    if (!expanded) {
      duk_get_prop_index(m_ctx, -1, static_cast<duk_uarridx_t>(i));
    }
    try {
      JValue value = pop();
      elements[i] = value.getShort();
    } catch (const std::exception &e) {
      if (!expanded) {
        duk_pop(m_ctx);  // pop the array
      }
      throw;
    }
  }

  if (!expanded) {
    duk_pop(m_ctx);  // pop the array
  }

  return JValue(shortArray);
}

duk_ret_t Short::push(const JValue &value) const {
  duk_push_number(m_ctx, static_cast<duk_double_t>(value.getShort()));  // no real Duktape for int64 so needing a cast to double
  return 1;
}

duk_ret_t Short::pushArray(const JniLocalRef<jarray> &values, bool expand) const {
  JArrayLocalRef<jshort> shortArray(values);
  const auto count = shortArray.getLength();

  const jshort *elements = shortArray.getElements();
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
  inline jshort getShort(JSContext *ctx, JSValue v) {
    int tag = JS_VALUE_GET_TAG(v);
    if (tag == JS_TAG_INT) {
      int64_t i64;
      JS_ToInt64(ctx, &i64, v);
      return i64;
    }

    if (JS_TAG_IS_FLOAT64(tag)) {
      return jshort(JS_VALUE_GET_FLOAT64(v));
    }

    throw std::invalid_argument("Cannot convert JS value to Java short");
  }
}

JValue Short::toJava(JSValueConst v) const {
  if (!JS_IsNumber(v)) {
    throw std::invalid_argument("Cannot convert return value to short");
  }

  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  return JValue(getShort(m_ctx, v));
}

JValue Short::toJavaArray(JSValueConst v) const {
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

  JArrayLocalRef<jshort> shortArray(m_jniContext, count);
  if (shortArray.isNull()) {
    throw JniException(m_jniContext);
  }

  jshort *elements = shortArray.getMutableElements();
  if (elements == nullptr) {
    throw JniException(m_jniContext);
  }

  for (uint32_t i = 0; i < count; ++i) {
    JSValue ev = JS_GetPropertyUint32(m_ctx, v, i);
    elements[i] = getShort(m_ctx, ev);
  }

  shortArray.releaseArrayElements();  // copy back elements to Java
  return JValue(shortArray);
}

JSValue Short::fromJava(const JValue &value) const {
  return JS_NewInt64(m_ctx, value.getShort());
}

JSValue Short::fromJavaArray(const JniLocalRef<jarray> &values) const {
  JArrayLocalRef<jshort> shortArray(values);
  const auto count = shortArray.getLength();

  JSValue jsArray = JS_NewArray(m_ctx);

  const jshort *elements = shortArray.getElements();
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

JValue Short::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                        const std::vector<JValue> &args) const {
  jshort l = m_jniContext->callShortMethodA(javaThis, methodId, args);

  // Explicitly release all values now because they won't be used afterwards
  JValue::releaseAll(args);

  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  return JValue(l);
}

JValue Short::box(const JValue &shortValue) const {
  // From short to Short
  static thread_local jmethodID boxId = m_jniContext->getStaticMethodID(getBoxedJavaClass(), "valueOf", "(S)Ljava/lang/Short;");
  return JValue(m_jniContext->callStaticObjectMethod(getBoxedJavaClass(), boxId, shortValue.getShort()));
}

JValue Short::unbox(const JValue &boxedValue) const {
  // From Short to short
  static thread_local jmethodID unboxId = m_jniContext->getMethodID(getBoxedJavaClass(), "shortValue", "()S");
  return JValue(m_jniContext->callShortMethod(boxedValue.getLocalRef(), unboxId));
}

}  // namespace JavaTypes
