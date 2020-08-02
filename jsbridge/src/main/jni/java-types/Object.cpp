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

#include "BoxedPrimitive.h"
#include "Boolean.h"
#include "Double.h"
#include "Float.h"
#include "Integer.h"
#include "JsBridgeContext.h"
#include "JsonObjectWrapper.h"
#include "Long.h"
#include "String.h"
#include "jni-helpers/JniContext.h"

namespace JavaTypes {

Object::Object(const JsBridgeContext *jsBridgeContext)
 : JavaType(jsBridgeContext, JavaTypeId::Object) {
}

#if defined(DUKTAPE)

#include "StackChecker.h"
#include "JsonObjectWrapper.h"

JValue Object::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  duk_ret_t dukType = duk_get_type(m_ctx, -1);

  switch (dukType) {
    case DUK_TYPE_NULL:
    case DUK_TYPE_UNDEFINED:
      duk_pop(m_ctx);
      return JValue();

    case DUK_TYPE_BOOLEAN: {
      auto booleanType = std::make_unique<Boolean>(m_jsBridgeContext);
      auto boxedBoleanType = std::make_unique<BoxedPrimitive>(m_jsBridgeContext, std::move(booleanType));
      return boxedBoleanType->pop();
    }

    case DUK_TYPE_NUMBER: {
      auto doubleType = std::make_unique<Double>(m_jsBridgeContext);
      auto boxedDoubleType = std::make_unique<BoxedPrimitive>(m_jsBridgeContext, std::move(doubleType));
      return boxedDoubleType->pop();
    }

    case DUK_TYPE_STRING: {
      auto stringType = std::make_unique<String>(m_jsBridgeContext, false);
      return stringType->pop();
    }

    case DUK_TYPE_OBJECT: {
      auto jsonObjectWrapperType = std::make_unique<JsonObjectWrapper>(m_jsBridgeContext, false /*isNullable*/);
      return jsonObjectWrapperType->pop();
    }

    default: {
      const auto message = std::string("Cannot marshal return value ") + duk_safe_to_string(m_ctx, -1) + " to Java";
      duk_pop(m_ctx);
      throw std::invalid_argument(message);
    }
  }
}

duk_ret_t Object::push(const JValue &value) const {
  CHECK_STACK_OFFSET(m_ctx, 1);

  const JniLocalRef<jobject> &jBasicObject = value.getLocalRef();

  if (jBasicObject.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  JavaType *javaType = newJavaType(jBasicObject);

  if (javaType == nullptr) {
    throw std::invalid_argument("Cannot push Object: unsupported Java type");
  }

  duk_ret_t ret = javaType->push(value);
  delete javaType;
  return ret;
}

#elif defined(QUICKJS)

#include "QuickJsUtils.h"

JValue Object::toJava(JSValueConst v) const {
  if (JS_IsUndefined(v) || JS_IsNull(v)) {
    return JValue();
  }

  if (JS_IsBool(v)) {
    auto booleanType = std::make_unique<Boolean>(m_jsBridgeContext);
    auto boxedBoleanType = std::make_unique<BoxedPrimitive>(m_jsBridgeContext, std::move(booleanType));
    return boxedBoleanType->toJava(v);
  }

  if (JS_IsNumber(v)) {
    auto doubleType = std::make_unique<Double>(m_jsBridgeContext);
    auto boxedDoubleType = std::make_unique<BoxedPrimitive>(m_jsBridgeContext, std::move(doubleType));
    return boxedDoubleType->toJava(v);
  }

  if (JS_IsString(v)) {
    auto stringType = std::make_unique<String>(m_jsBridgeContext, false);
    return stringType->toJava(v);
  }

  if (JS_IsObject(v)) {
    auto jsonObjectWrapperType = std::make_unique<JsonObjectWrapper>(m_jsBridgeContext, false /*isNullable*/);
    return jsonObjectWrapperType->toJava(v);
  }

  throw std::invalid_argument("Cannot marshal return value to Java");
}

JSValue Object::fromJava(const JValue &value) const {
  const JniLocalRef<jobject> &jBasicObject = value.getLocalRef();

  if (jBasicObject.isNull()) {
    return JS_NULL;
  }

  auto javaType = std::unique_ptr<JavaType>(newJavaType(jBasicObject));

  if (javaType.get() == nullptr) {
    throw std::invalid_argument("Cannot transfer Java Object to JS: unsupported Java type");
  }

  return javaType->fromJava(value);
}

#endif

JavaType *Object::newJavaType(const JniLocalRef<jobject> &jobject) const {
  JniLocalRef<jclass> objectJavaClass = m_jniContext->getObjectClass(jobject);
  jmethodID getName = m_jniContext->getMethodID(objectJavaClass, "getName", "()Ljava/lang/String;");
  JStringLocalRef javaNameRef = m_jniContext->callStringMethod(objectJavaClass, getName);

  JavaTypeId id = getJavaTypeIdByJavaName(javaNameRef.getUtf16View());
  javaNameRef.release();

  switch (id) {
    case JavaTypeId::Boolean:
    case JavaTypeId::BoxedBoolean:
      return new BoxedPrimitive(m_jsBridgeContext, std::make_unique<Boolean>(m_jsBridgeContext));
    case JavaTypeId::Int:
    case JavaTypeId::BoxedInt:
      return new BoxedPrimitive(m_jsBridgeContext, std::make_unique<Integer>(m_jsBridgeContext));
    case JavaTypeId::Long:
    case JavaTypeId::BoxedLong:
      return new BoxedPrimitive(m_jsBridgeContext, std::make_unique<Long>(m_jsBridgeContext));
    case JavaTypeId::Float:
    case JavaTypeId::BoxedFloat:
      return new BoxedPrimitive(m_jsBridgeContext, std::make_unique<Float>(m_jsBridgeContext));
    case JavaTypeId::Double:
    case JavaTypeId::BoxedDouble:
      return new BoxedPrimitive(m_jsBridgeContext, std::make_unique<Double>(m_jsBridgeContext));
    case JavaTypeId::String:
      return new String(m_jsBridgeContext, false);
    case JavaTypeId::DebugString:
      return new String(m_jsBridgeContext, true);
    case JavaTypeId::JsonObjectWrapper:
      return new JsonObjectWrapper(m_jsBridgeContext, false /*isNullable*/);
    default:
      return nullptr;
  }
}

}  // namespace JavaTypes

