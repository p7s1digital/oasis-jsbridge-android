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

#include "JsBridgeContext.h"
#include "log.h"
#include "exceptions/JniException.h"
#include "jni-helpers/JArrayLocalRef.h"

#ifdef DUKTAPE
# include "JsBridgeContext.h"
# include "StackChecker.h"
#endif

namespace JavaTypes {

Boolean::Boolean(const JsBridgeContext *jsBridgeContext)
 : Primitive(jsBridgeContext, JavaTypeId::Boolean, JavaTypeId::BoxedBoolean) {
}

#if defined(DUKTAPE)

JValue Boolean::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (!duk_is_boolean(m_ctx, -1)) {
    const auto message = std::string("Cannot convert return value ") + duk_safe_to_string(m_ctx, -1) + " to boolean";
    duk_pop(m_ctx);
    throw std::invalid_argument(message);
  }

  bool b = (bool) duk_require_boolean(m_ctx, -1);
  duk_pop(m_ctx);
  return JValue(b);
}

JValue Boolean::popArray(uint32_t count, bool expanded) const {
  if (!expanded) {
    count = static_cast<uint32_t>(duk_get_length(m_ctx, -1));
    if (!duk_is_array(m_ctx, -1)) {
      const auto message = std::string("Cannot convert JS value ") + duk_safe_to_string(m_ctx, -1) + " to Array<Boolean>";
      duk_pop(m_ctx);  // pop the array
      throw std::invalid_argument(message);
    }
  }

  JArrayLocalRef<jboolean> boolArray(m_jniContext, count);
  jboolean *elements = boolArray.isNull() ? nullptr : boolArray.getMutableElements();
  if (elements == nullptr) {
    duk_pop_n(m_ctx, expanded ? count : 1);  // pop the expanded elements or the array
    throw JniException(m_jniContext);
  }

  for (int i = count - 1; i >= 0; --i) {
    if (!expanded) {
      duk_get_prop_index(m_ctx, -1, static_cast<duk_uarridx_t>(i));
    }
    JValue value = pop();
    elements[i] = value.getBool();
  }

  if (!expanded) {
    duk_pop(m_ctx);  // pop the array
  }

  return JValue(boolArray);
}

duk_ret_t Boolean::push(const JValue &value) const {
  duk_push_boolean(m_ctx, (duk_bool_t) value.getBool());
  return 1;
}

duk_ret_t Boolean::pushArray(const JniLocalRef<jarray>& values, bool expand) const {
  JArrayLocalRef<jboolean> boolArray(values);
  const auto count = boolArray.getLength();

  const jboolean *elements = boolArray.getElements();
  if (elements == nullptr) {
    throw JniException(m_jniContext);
  }

  CHECK_STACK_OFFSET(m_ctx, expand ? count : 1);

  if (!expand) {
    duk_push_array(m_ctx);
  }

  for (jsize i = 0; i < count; ++i) {
    duk_push_boolean(m_ctx, duk_bool_t(elements[i] == JNI_TRUE));
    if (!expand) {
      duk_put_prop_index(m_ctx, -2, static_cast<duk_uarridx_t>(i));
    }
  }

  return expand ? count : 1;
}

#elif defined(QUICKJS)

JValue Boolean::toJava(JSValueConst v) const {
  if (!JS_IsBool(v)) {
    throw std::invalid_argument("Cannot convert return value to boolean");
  }

  return JValue(static_cast<jboolean>(JS_VALUE_GET_BOOL(v)));
}

JValue Boolean::toJavaArray(JSValueConst v) const {
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

  JArrayLocalRef<jboolean> boolArray(m_jniContext, count);
  if (boolArray.isNull()) {
    throw JniException(m_jniContext);
  }

  jboolean *elements = boolArray.getMutableElements();
  if (elements == nullptr) {
    throw JniException(m_jniContext);
  }

  for (uint32_t i = 0; i < count; ++i) {
    JSValue ev = JS_GetPropertyUint32(m_ctx, v, i);
    if (!JS_IsBool(ev)) {
      alog_warn("Cannot get int from JS: returning 0");  // TODO: proper exception handling
    }

    elements[i] = static_cast<jboolean>(JS_VALUE_GET_BOOL(ev));
  }

  boolArray.releaseArrayElements();  // copy back elements to Java
  return JValue(boolArray);
}

JSValue Boolean::fromJava(const JValue &value) const {
  return JS_NewBool(m_ctx, value.getBool());
}

JSValue Boolean::fromJavaArray(const JniLocalRef<jarray>& values) const {
  JArrayLocalRef<jboolean> boolArray(values);
  const auto count = boolArray.getLength();

  JSValue jsArray = JS_NewArray(m_ctx);

  const jboolean *elements = boolArray.getElements();
  if (elements == nullptr) {
    JS_FreeValue(m_ctx, jsArray);
    throw JniException(m_jniContext);
  }

  for (jsize i = 0; i < count; ++i) {
    JSValue elementValue = JS_NewBool(m_ctx, elements[i]);
    JS_SetPropertyUint32(m_ctx, jsArray, static_cast<uint32_t>(i), elementValue);
  }

  return jsArray;
}

#endif

JValue Boolean::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                           const std::vector<JValue> &args) const {

  jboolean retVal = m_jniContext->callBooleanMethodA(javaThis, methodId, args);

  // Explicitly release all values now because they won't be used afterwards
  JValue::releaseAll(args);

  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  return JValue((bool) retVal);
}

JValue Boolean::box(const JValue &booleanValue) const {
  // From boolean to Boolean
  static thread_local jmethodID boxId = m_jniContext->getStaticMethodID(getBoxedJavaClass(), "valueOf", "(Z)Ljava/lang/Boolean;");
  auto boxedBoolean = m_jniContext->callStaticObjectMethod(getBoxedJavaClass(), boxId, booleanValue.getBool());
  return JValue(std::move(boxedBoolean));
}

JValue Boolean::unbox(const JValue &boxedValue) const {
  // From Boolean to boolean
  static thread_local jmethodID unboxId = m_jniContext->getMethodID(getBoxedJavaClass(), "booleanValue", "()Z");
  return JValue(m_jniContext->callBooleanMethod(boxedValue.getLocalRef(), unboxId));
}

}  // namespace JavaTypes

