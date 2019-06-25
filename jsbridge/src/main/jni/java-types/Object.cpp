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
#include "Object.h"

#include "JsBridgeContext.h"
#include "jni-helpers/JniContext.h"
#include "../JsBridgeContext.h"

#if defined(DUKTAPE)
# include "JsBridgeContext.h"
#elif defined(QUICKJS)
# include "JsBridgeContext.h"
# include "QuickJsUtils.h"
#endif

namespace JavaTypes {

Object::Object(const JsBridgeContext *jsBridgeContext, const JniGlobalRef <jclass> &classRef,
               const JavaType &boxedBoolean, const JavaType &boxedDouble, const JavaType &jsonObjectWrapperType)
  : JavaType(jsBridgeContext, classRef)
  , m_boxedBoolean(boxedBoolean)
  , m_boxedDouble(boxedDouble)
  , m_jsonObjectWrapperType(jsonObjectWrapperType) {
}

#if defined(DUKTAPE)

JValue Object::pop(bool inScript, const AdditionalData *) const {
  switch (duk_get_type(m_ctx, -1)) {
    case DUK_TYPE_NULL:
    case DUK_TYPE_UNDEFINED:
      duk_pop(m_ctx);
    return JValue();

    case DUK_TYPE_BOOLEAN:
      return m_boxedBoolean.pop(inScript, nullptr);

    case DUK_TYPE_NUMBER:
      return m_boxedDouble.pop(inScript, nullptr);

    case DUK_TYPE_STRING: {
      const char *str = duk_get_string(m_ctx, -1);
      JStringLocalRef localRef(m_jniContext, str);
      duk_pop(m_ctx);
      return JValue(localRef);
    }

    case DUK_TYPE_OBJECT:
      return m_jsonObjectWrapperType.pop(inScript, nullptr);

    default: {
      const auto message = std::string("Cannot marshal return value ") + duk_safe_to_string(m_ctx, -1) + " to Java";
      if (inScript) {
        duk_error(m_ctx, DUK_RET_TYPE_ERROR, message.c_str());
      }
      duk_pop(m_ctx);
      if (!inScript) {
        throw std::invalid_argument(message);
      }
      return JValue();
    }
  }
}

duk_ret_t Object::push(const JValue &value, bool inScript, const AdditionalData *) const {
  const JniLocalRef<jobject> &jObject = value.getLocalRef();

  if (jObject.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  JniLocalRef<jclass> javaClass = m_jniContext->getObjectClass(jObject);
  return m_jsBridgeContext->getJavaTypes().get(m_jsBridgeContext, javaClass)->push(value, inScript, nullptr /*TODO*/);
}

#elif defined(QUICKJS)

JValue Object::toJava(JSValueConst v, bool inScript, const AdditionalData *) const {
  if (JS_IsUndefined(v) || JS_IsNull(v)) {
    return JValue();
  }

  if (JS_IsBool(v)) {
    return m_boxedBoolean.toJava(v, inScript, nullptr);
  }

  if (JS_IsNumber(v)) {
    return m_boxedDouble.toJava(v, inScript, nullptr);
  }

  if (JS_IsString(v)) {
    JStringLocalRef localRef = m_jsBridgeContext->getUtils()->toJString(v);
    return JValue(localRef);
  }

  if (JS_IsObject(v)) {
    return m_jsonObjectWrapperType.toJava(v, inScript, nullptr);
  }

  const auto message = "Cannot marshal return value to Java";
  if (inScript) {
    JS_ThrowTypeError(m_ctx, message);
  }
  if (!inScript) {
    throw std::invalid_argument(message);
  }

  return JValue();
}

JSValue Object::fromJava(const JValue &value, bool inScript, const AdditionalData *) const {
  const JniLocalRef<jobject> &jObject = value.getLocalRef();

  if (jObject.isNull()) {
    return JS_NULL;
  }

  JniLocalRef<jclass> javaClass = m_jniContext->getObjectClass(jObject);
  return m_jsBridgeContext->getJavaTypes().get(m_jsBridgeContext, javaClass)->fromJava(value, inScript, nullptr);
}

#endif

}  // namespace JavaTypes

