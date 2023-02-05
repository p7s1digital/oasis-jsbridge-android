/*
 * Copyright (C) 2019 ProSiebenSat1.Digital GmbH.
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
#include "JavaTypeProvider.h"

#include "JniCache.h"
#include "JsBridgeContext.h"
#include "java-types/Array.h"
#include "java-types/BoxedPrimitive.h"
#include "java-types/Boolean.h"
#include "java-types/Byte.h"
#include "java-types/Deferred.h"
#include "java-types/Double.h"
#include "java-types/Float.h"
#include "java-types/FunctionX.h"
#include "java-types/Integer.h"
#include "java-types/JsToNativeProxy.h"
#include "java-types/JsValue.h"
#include "java-types/JsonObjectWrapper.h"
#include "java-types/List.h"
#include "java-types/Long.h"
#include "java-types/NativeObjectWrapper.h"
#include "java-types/Object.h"
#include "java-types/String.h"
#include "java-types/Void.h"
#include "jni-helpers/JniContext.h"
#include "log.h"

using namespace JavaTypes;

namespace {
  template <class T>
  JavaType *createPrimitive(const JsBridgeContext *jsBridgeContext, bool boxed) {
    auto primitiveType = new T(jsBridgeContext);
    if (!boxed) {
      return primitiveType;
    }
    return new BoxedPrimitive(jsBridgeContext, std::unique_ptr<T>(primitiveType));
  }

  template <class T>
  Array *createPrimitiveArray(const JsBridgeContext *jsBridgeContext) {
    auto primitiveType = new T(jsBridgeContext);
    return new Array(jsBridgeContext, std::unique_ptr<T>(primitiveType));
  }
}

JavaTypeProvider::JavaTypeProvider(const JsBridgeContext *jsBridgeContext)
 : m_jsBridgeContext(jsBridgeContext)
 , m_objectType() {
}

const JavaType *JavaTypeProvider::newType(const JniRef<jsBridgeParameter> &parameter, bool boxed) const {
  JavaTypeId id = parameter.isNull() ? JavaTypeId::Object : getJavaTypeId(parameter);

  switch (id) {
    case JavaTypeId::Void:
      return new Void(m_jsBridgeContext, id, false /*boxed*/);
    case JavaTypeId::Unit:
      return new Void(m_jsBridgeContext, id, boxed);
    case JavaTypeId::Boolean:
      return createPrimitive<Boolean>(m_jsBridgeContext, boxed);
    case JavaTypeId::Byte:
      return createPrimitive<Byte>(m_jsBridgeContext, boxed);
    case JavaTypeId::Int:
      return createPrimitive<Integer>(m_jsBridgeContext, boxed);
    case JavaTypeId::Long:
      return createPrimitive<Long>(m_jsBridgeContext, boxed);
    case JavaTypeId::Float:
      return createPrimitive<Float>(m_jsBridgeContext, boxed);
    case JavaTypeId::Double:
      return createPrimitive<Double>(m_jsBridgeContext, boxed);

    case JavaTypeId::BoxedVoid:
      return new Void(m_jsBridgeContext, id, false /*boxed*/);  // Java "Void" object behaves like the unboxed version
    case JavaTypeId::BoxedBoolean:
      return createPrimitive<Boolean>(m_jsBridgeContext, true);
    case JavaTypeId::BoxedByte:
      return createPrimitive<Byte>(m_jsBridgeContext, true);
    case JavaTypeId::BoxedInt:
      return createPrimitive<Integer>(m_jsBridgeContext, true);
    case JavaTypeId::BoxedLong:
      return createPrimitive<Long>(m_jsBridgeContext, true);
    case JavaTypeId::BoxedFloat:
      return createPrimitive<Float>(m_jsBridgeContext, true);
    case JavaTypeId::BoxedDouble:
      return createPrimitive<Double>(m_jsBridgeContext, true);

    case JavaTypeId::String:
      return new String(m_jsBridgeContext, false);
    case JavaTypeId::DebugString:
      return new String(m_jsBridgeContext, true);
    case JavaTypeId::Object:
      return new Object(m_jsBridgeContext);

    case JavaTypeId::ObjectArray: {
      auto genericParameterType = getGenericParameterType(parameter);
      return new Array(m_jsBridgeContext, std::move(genericParameterType));
    }
    case JavaTypeId::List: {
      auto genericParameterType = getGenericParameterType(parameter);
      return new List(m_jsBridgeContext, std::move(genericParameterType));
    }
    case JavaTypeId::BooleanArray:
      return createPrimitiveArray<Boolean>(m_jsBridgeContext);
    case JavaTypeId::ByteArray:
      return createPrimitiveArray<Byte>(m_jsBridgeContext);
    case JavaTypeId::IntArray:
      return createPrimitiveArray<Integer>(m_jsBridgeContext);
    case JavaTypeId::LongArray:
      return createPrimitiveArray<Long>(m_jsBridgeContext);
    case JavaTypeId::FloatArray:
      return createPrimitiveArray<Float>(m_jsBridgeContext);
    case JavaTypeId::DoubleArray:
      return createPrimitiveArray<Double>(m_jsBridgeContext);

    case JavaTypeId::FunctionX:
      return new FunctionX(m_jsBridgeContext, parameter);
    case JavaTypeId::JsValue:
      return new JsValue(m_jsBridgeContext, isParameterNullable(parameter));
    case JavaTypeId::JsonObjectWrapper:
      return new JsonObjectWrapper(m_jsBridgeContext, isParameterNullable(parameter));
    case JavaTypeId::Deferred:
      return new Deferred(m_jsBridgeContext, getGenericParameterType(parameter));
    case JavaTypeId::NativeObjectWrapper:
      return new NativeObjectWrapper(m_jsBridgeContext);
    case JavaTypeId::JsToNativeProxy:
      return new JsToNativeProxy(m_jsBridgeContext);

    case JavaTypeId::Unknown:
      return nullptr;
  }
}

std::unique_ptr<const JavaType> JavaTypeProvider::makeUniqueType(const JniRef<jsBridgeParameter> &parameter, bool boxed) const {
  const JavaType *t = newType(parameter, boxed);
  return t ? std::unique_ptr<const JavaType>(t) : std::unique_ptr<const JavaType>();
}

const std::unique_ptr<const JavaType> &JavaTypeProvider::getObjectType() const {
  if (m_objectType == nullptr)   {
    m_objectType.reset(new Object(m_jsBridgeContext));
  }

  return m_objectType;
}

std::unique_ptr<const JavaType> JavaTypeProvider::getDeferredType(const JniRef<jsBridgeParameter> &parameter) const {
  return std::make_unique<Deferred>(m_jsBridgeContext, makeUniqueType(parameter, true /*boxed*/));
}


// Private methods
// ---

JavaTypeId JavaTypeProvider::getJavaTypeId(const JniRef<jsBridgeParameter> &parameter) const {
  const JniContext *jniContext = m_jsBridgeContext->getJniContext();
  assert(jniContext != nullptr);

  JStringLocalRef javaName = m_jsBridgeContext->getJniCache()->getParameterInterface(parameter).getJavaName();
  if (javaName.isNull()) {
    throw std::invalid_argument("Could not get Java name from Parameter!");
  }

  JavaTypeId id = getJavaTypeIdByJavaName(javaName.getUtf16View());
  if (id == JavaTypeId::Unknown) {
    throw std::invalid_argument(std::string("Unsupported Java type: ") + javaName.toStdString());
    //return JavaTypeId::Unknown;
  }

  return id;
}

bool JavaTypeProvider::isParameterNullable(const JniRef<jsBridgeParameter> &parameter) const {
  ParameterInterface parameterInterface = m_jsBridgeContext->getJniCache()->getParameterInterface(parameter);
  return parameterInterface.isNullable();
}

JniLocalRef<jsBridgeParameter> JavaTypeProvider::getGenericParameter(const JniRef<jsBridgeParameter> &parameter) const {
  return m_jsBridgeContext->getJniCache()->getParameterInterface(parameter).getGenericParameter();
}

std::unique_ptr<const JavaType> JavaTypeProvider::getGenericParameterType(const JniRef<jsBridgeParameter> &parameter) const {
  return makeUniqueType(getGenericParameter(parameter), true /*boxed*/);
}

