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
#include <log.h>

#include <utility>
#include "Object.h"

#include "Array.h"
#include "BoxedPrimitive.h"
#include "Boolean.h"
#include "Double.h"
#include "Float.h"
#include "Integer.h"
#include "JavaObject.h"
#include "JavaObjectWrapper.h"
#include "JsonObjectWrapper.h"
#include "JsBridgeContext.h"
#include "Long.h"
#include "String.h"
#include "JniCache.h"

#include "exceptions/JniException.h"
#include "jni-helpers/JniContext.h"

namespace JavaTypes {

Object::Object(const JsBridgeContext *jsBridgeContext, std::optional<JniGlobalRef<jstring>> optJavaName)
 : JavaType(jsBridgeContext, JavaTypeId::Object)
 , m_optJavaName(std::move(optJavaName)) {
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
      if (duk_is_array(m_ctx, -1)) {
        auto componentType = std::make_unique<Object>(m_jsBridgeContext, std::nullopt);
        auto arrayType = std::make_unique<Array>(m_jsBridgeContext, std::move(componentType));
        return arrayType->pop();
      }

      if (JavaObject::hasJavaThis(m_jsBridgeContext, -1)) {
        const auto javaObject = JavaObject::getJavaThis(m_jsBridgeContext, -1);
        duk_pop(m_ctx);
        return JValue(javaObject);
      }
    }

    default: {
      const auto message = std::string("Cannot marshal return value ") + duk_safe_to_string(m_ctx, -1) + " to Java";
      duk_pop(m_ctx);
      throw std::invalid_argument(message);
    }
  }
}

duk_ret_t Object::push(const JValue &value) const {
  alog("BW - 0");
  alog("BW - 0.0");
  CHECK_STACK_OFFSET(m_ctx, 1);
  alog("BW - 0.0.0");

  const JniLocalRef<jobject> &jBasicObject = value.getLocalRef();

  alog("BW - 0.0.0.0");
  if (jBasicObject.isNull()) {
    alog("BW - 0.1");
    duk_push_null(m_ctx);
    return 1;
  }

  alog("BW - 0.1.1");
  JavaType *javaType = newJavaType(jBasicObject);
  alog("BW - 0.2");

  alog("BW - 1");
  if (javaType == nullptr) {
    alog("BW - 2");
    duk_push_null(m_ctx);
    throw std::invalid_argument("Cannot push Object: unsupported Java type");
  }

  alog("BW - 3");
  duk_ret_t ret = javaType->push(value);
  alog("BW - 4 - ret = %d", ret);
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

  if (JS_IsArray(m_jsBridgeContext->getQuickJsContext(), v)) {
    auto componentType = std::make_unique<Object>(m_jsBridgeContext, std::nullopt);
    auto arrayType = std::make_unique<Array>(m_jsBridgeContext, std::move(componentType));
    return arrayType->toJava(v);
  }

  if (JS_IsObject(v)) {
    if (JavaObject::hasJavaThis(m_jsBridgeContext, v)) {
      const auto javaObject = JavaObject::getJavaThis(m_jsBridgeContext, v);
      return JValue(javaObject);
    }
  }

  throw std::invalid_argument("Cannot marshal return value to Java");
}

JSValue Object::fromJava(const JValue &value) const {
  const JniLocalRef<jobject> &jBasicObject = value.getLocalRef();

  if (jBasicObject.isNull()) {
    return JS_NULL;
  }

  auto javaNameRef = getJavaName(jBasicObject);
  auto javaNameView = javaNameRef.getUtf16View();
  const auto id = getJavaTypeIdByJavaName(javaNameView);

  auto javaType = std::unique_ptr<JavaType>(newJavaType(jBasicObject));
  if (javaType == nullptr) {
    throw std::invalid_argument("Cannot transfer Java Object to JS: unsupported Java type");
  }

  return javaType->fromJava(value);
}

#endif

JStringLocalRef Object::getJavaName(const JniLocalRef<jobject> &object) const {
  if (m_optJavaName.has_value()) {
    return JStringLocalRef(m_optJavaName.value());
  }

  // Get the class of the passed Java object
  jmethodID getClass = m_jniContext->getMethodID(m_jsBridgeContext->getJniCache()->getObjectClass(), "getClass", "()Ljava/lang/Class;");
  JniLocalRef<jclass> javaClassRef = m_jniContext->callObjectMethod<jclass>(object, getClass);
  JniLocalRef<jclass> javaClassClass = m_jsBridgeContext->getJniCache()->getJavaClassClass();

  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  // Call java.lang.Class::getName()
  jmethodID getName = m_jniContext->getMethodID(javaClassClass, "getName", "()Ljava/lang/String;");

  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  JStringLocalRef javaNameRef = m_jniContext->callStringMethod(javaClassRef, getName);
  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  return javaNameRef;
}

JavaType *Object::newJavaType(const JniLocalRef<jobject> &object) const {
  auto javaNameRef = getJavaName(object);
  auto javaNameView = javaNameRef.getUtf16View();
  JavaTypeId id = getJavaTypeIdByJavaName(javaNameView);

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
    case JavaTypeId::JavaObjectWrapper:
      return new JavaObjectWrapper(m_jsBridgeContext);
    case JavaTypeId::JsonObjectWrapper:
      return new JsonObjectWrapper(m_jsBridgeContext, false /*isNullable*/);
    case JavaTypeId::Object: {
      if (m_jniContext->isInstanceOf(object, getJniCache()->getNumberClass())) {
        // Java number -> JS double
        auto doubleType = std::make_unique<Double>(m_jsBridgeContext);
        return new BoxedPrimitive(m_jsBridgeContext, std::move(doubleType));
      }
      else if (m_jniContext->isInstanceOf(object, getJniCache()->getStringClass())) {
        // Java String -> JS String
        return new String(m_jsBridgeContext, false);
      } else {
        // Java Any -> JS wrapped JavaObject
        auto javaObjectWrapper = m_jsBridgeContext->getJniCache()->getOrCreateJavaObjectWrapper(object);
        return new JavaObjectWrapper(m_jsBridgeContext);
      }
    }
    case JavaTypeId::ObjectArray: {
      // ObjectArray -> Array of Object

      // Get the class of the passed Java object
      jmethodID getClass = m_jniContext->getMethodID(m_jsBridgeContext->getJniCache()->getObjectClass(), "getClass", "()Ljava/lang/Class;");
      JniLocalRef<jclass> javaClassRef = m_jniContext->callObjectMethod<jclass>(object, getClass);
      return new Array(m_jsBridgeContext, javaClassRef);
    }
    case JavaTypeId::Unknown:
    default:
      alog_warn("Class %s (type id = %d) cannot be transferred from Java to JS", javaNameRef.toUtf8Chars(), id);
      return nullptr;
  }
}

}  // namespace JavaTypes

