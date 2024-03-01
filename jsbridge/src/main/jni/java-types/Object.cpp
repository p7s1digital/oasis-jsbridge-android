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
#include <exceptions/JniException.h>
#include "Object.h"

#include "AidlInterface.h"
#include "AidlParcelable.h"
#include "BoxedPrimitive.h"
#include "Boolean.h"
#include "Double.h"
#include "Float.h"
#include "Integer.h"
#include "JsBridgeContext.h"
#include "JsonObjectWrapper.h"
#include "Long.h"
#include "String.h"
#include "JniCache.h"
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

    case DUK_TYPE_OBJECT:
      if (m_insideAidl) {
        // Ideally, for AIDL would try to guess if it is a parcelable or an interface by checking if we have at least one
        // function but it does not help much anyway because the type itself has been erased and cannot be guessed
        duk_pop(m_ctx);
        throw std::invalid_argument("Unknown object class cannot be transferred from JS to Java because the type information is missing. Hint: use in AIDL an Array instead of List, e.g. List<MyParcelable> -> MyParcelable[]");
      } else {
        alog_warn("Unknown object class cannot be transferred from JS to Java because the type information is missing. Defaulting to JsonObjectWrapper...");
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
#include "AidlParcelable.h"

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
    if (m_insideAidl) {
      // Ideally, for AIDL would try to guess if it is a parcelable or an interface by checking if we have at least one
      // function but it does not help much anyway because the type itself has been erased and cannot be guessed
      throw std::invalid_argument("Unknown object class cannot be transferred from JS to Java because the type information is missing. Hint: use in AIDL an Array instead of List, e.g. List<MyParcelable> -> MyParcelable[]");
    } else {
      alog_warn("Unknown object class cannot be transferred from JS to Java because the type information is missing. Defaulting to JsonObjectWrapper...");
      auto jsonObjectWrapperType = std::make_unique<JsonObjectWrapper>(m_jsBridgeContext, false /*isNullable*/);
      return jsonObjectWrapperType->toJava(v);
    }
  }

  throw std::invalid_argument("Cannot marshal return value to Java");
}

JSValue Object::fromJava(const JValue &value) const {
  const JniLocalRef<jobject> &jBasicObject = value.getLocalRef();

  if (jBasicObject.isNull()) {
    return JS_NULL;
  }

  auto javaType = std::unique_ptr<JavaType>(newJavaType(jBasicObject));

  if (javaType == nullptr) {
    throw std::invalid_argument("Cannot transfer Java Object to JS: unsupported Java type");
  }

  return javaType->fromJava(value);
}

#endif

JavaType *Object::newJavaType(const JniLocalRef<jobject> &object) const {
  // Get the class of the passed Java object
  jmethodID getClass = m_jniContext->getMethodID(m_jsBridgeContext->getJniCache()->getObjectClass(), "getClass", "()Ljava/lang/Class;");
  JniLocalRef<jclass> javaClassRef = m_jniContext->callObjectMethod<jclass>(object, getClass);
  JniLocalRef<jclass> javaClassClass = m_jniContext->getObjectClass(javaClassRef);

  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }
  // Call java.lang.Class::getName()
  jmethodID getName = m_jniContext->getMethodID(javaClassClass, "getName", "()Ljava/lang/String;");

  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  JStringLocalRef javaNameRef = m_jniContext->callStringMethod(javaClassRef, getName);

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
    case JavaTypeId::Unknown:
      if (m_insideAidl) {
        // If we don't know the type, it could be an AIDL parcelable or interface element of a List and whose
        // type has been erased and is not available via reflection
        // => try to find it out and create the correct type

        JniLocalRef<jsBridgeParameter> jsBridgeParameter = m_jsBridgeContext->getJniCache()->newParameter(javaClassRef);
        ParameterInterface parameterInterface = m_jsBridgeContext->getJniCache()->getParameterInterface(jsBridgeParameter);

        if (m_jniContext->exceptionCheck()) {
          throw JniException(m_jniContext);
        }

        if (parameterInterface.isAidlParcelable()) {
          return new AidlParcelable(m_jsBridgeContext, jsBridgeParameter, false);
        }
        if (parameterInterface.isAidlInterface()) {
          return new AidlInterface(m_jsBridgeContext, jsBridgeParameter);
        }
      }

      return nullptr;
    default:
      return nullptr;
  }
  return nullptr;
}

}  // namespace JavaTypes

