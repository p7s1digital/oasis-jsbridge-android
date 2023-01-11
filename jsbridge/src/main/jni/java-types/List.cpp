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
#include "List.h"

#include "AutoReleasedJSValue.h"
#include "JniCache.h"
#include "JsBridgeContext.h"
#include "Primitive.h"
#include "exceptions/JniException.h"
#include "jni-helpers/JValue.h"
#include <string>

namespace {
  JavaTypeId getArrayId(const JavaType *componentType) {
    auto primitive = dynamic_cast<const JavaTypes::Primitive *>(componentType);
    if (primitive == nullptr) {
      return JavaTypeId::ObjectArray;
    }
    return primitive->arrayId();
  }
}

namespace JavaTypes {

List::List(const JsBridgeContext *jsBridgeContext, std::unique_ptr<const JavaType> &&componentType)
 : JavaType(jsBridgeContext, getArrayId(componentType.get()))
 , m_componentType(std::move(componentType)) {
}

#if defined(DUKTAPE)

#include "StackChecker.h"

JValue List::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }

  if (!duk_is_array(m_ctx, -1)) {
    const auto message = std::string("Cannot convert ") + duk_safe_to_string(m_ctx, -1) + " to list";
    duk_pop(m_ctx);
    throw std::invalid_argument(message);
  }

  uint32_t count = duk_get_length(m_ctx, -1);

  JniLocalRef<jobject> javaList = m_jsBridgeContext->getJniCache()->newList();

  for (int i = 0; i < count; ++i) {
    duk_get_prop_index(m_ctx, -1, static_cast<duk_uarridx_t>(i));

    JValue elementValue;

    try {
      elementValue = m_componentType->pop();
    } catch (const std::exception &) {
      duk_pop(m_ctx);  // pop array
      throw;
    }
    const JniLocalRef<jobject> &jElement = elementValue.getLocalRef();
    m_jsBridgeContext->getJniCache()->addToList(javaList, jElement);

    if (m_jniContext->exceptionCheck()) {
      duk_pop(m_ctx);  // pop array
      throw JniException(m_jniContext);
    }
  }

  duk_pop(m_ctx);  // pop array
  return JValue(javaList);
}

duk_ret_t List::push(const JValue &value) const {
  CHECK_STACK_OFFSET(m_ctx, 1);

  const JniLocalRef<jobject> &jList = value.getLocalRef();

  if (jList.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  duk_push_array(m_ctx);

  const int count = m_jsBridgeContext->getJniCache()->getListLength(jList);
  for (int i = 0; i < count; ++i) {
    JniLocalRef<jobject> jElement = m_jsBridgeContext->getJniCache()->getListElement(jList, i);

    try {
      m_componentType->push(JValue(jElement));
      duk_put_prop_index(m_ctx, -2, static_cast<duk_uarridx_t>(i));
    } catch (const std::exception &) {
      duk_pop(m_ctx);  // pop array
      throw;
    }
  }

  return 1;
}

#elif defined(QUICKJS)

JValue List::toJava(JSValueConst v) const {
  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  if (!JS_IsArray(m_ctx, v)) {
    throw std::invalid_argument("Cannot convert value to array");
  }

  JSValue lengthValue = JS_GetPropertyStr(m_ctx, v, "length");
  assert(JS_IsNumber(lengthValue));
  uint32_t count = JS_VALUE_GET_INT(lengthValue);
  JS_FreeValue(m_ctx, lengthValue);

  JniLocalRef<jobject> javaList = m_jsBridgeContext->getJniCache()->newList();

  for (int i = 0; i < count; ++i) {
    JSValue elementJsValue = JS_GetPropertyUint32(m_ctx, v, i);
    JS_AUTORELEASE_VALUE(m_ctx, elementJsValue);  // also released in case of exception!
    JValue elementValue = m_componentType->toJava(elementJsValue);

    const JniLocalRef<jobject> &jElement = elementValue.getLocalRef();
    m_jsBridgeContext->getJniCache()->addToList(javaList, jElement);

    if (m_jniContext->exceptionCheck()) {
      throw JniException(m_jniContext);
    }
  }

  return JValue(javaList);
}

JSValue List::fromJava(const JValue &value) const {
  const JniLocalRef<jobject> &jList = value.getLocalRef();

  if (jList.isNull()) {
    return JS_NULL;
  }

  JSValue jsArray = JS_NewArray(m_ctx);

  const int count = m_jsBridgeContext->getJniCache()->getListLength(jList);
  for (int i = 0; i < count; ++i) {
    JniLocalRef<jobject> jElement = m_jsBridgeContext->getJniCache()->getListElement(jList, i);

    try {
      JSValue jsElement = m_componentType->fromJava(JValue(jElement));
      JS_SetPropertyUint32(m_ctx, jsArray, static_cast<uint32_t>(i), jsElement);
    } catch (const std::exception &) {
      JS_FreeValue(m_ctx, jsArray);
      throw;
    }
  }

  return jsArray;
}

#endif

}  // namespace JavaTypes

