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
#include "Array.h"

#include "JsBridgeContext.h"
#include "jni-helpers/JValue.h"
#include <string>

namespace JavaTypes {

Array::Array(const JsBridgeContext *jsBridgeContext, const JniGlobalRef<jclass>& classRef, const JavaType& componentType)
 : JavaType(jsBridgeContext, classRef)
 , m_componentType(componentType) {
}

class Array::AdditionalArrayData: public JavaType::AdditionalData {
public:
  AdditionalArrayData() = default;

  const JavaType *genericArgumentType = nullptr;
  JniGlobalRef<jsBridgeParameter> genericArgumentParameter;
};

JavaType::AdditionalData *Array::createAdditionalPopData(const JniRef<jsBridgeParameter> &parameter) const {
  if (m_componentType.isPrimitive()) {
    return nullptr;
  }

  auto data = new AdditionalArrayData();

  const JniRef<jclass> &parameterClass = m_jniContext->getJsBridgeParameterClass();

  jmethodID getGenericParameter = m_jniContext->getMethodID(parameterClass, "getGenericParameter", "()Lde/prosiebensat1digital/oasisjsbridge/Parameter;");
  JniLocalRef<jsBridgeParameter> genericParameter = m_jniContext->callObjectMethod<jsBridgeParameter>(parameter, getGenericParameter);

  if (genericParameter.isNull()) {
    return nullptr;
  }

  jmethodID getGenericJavaClass = m_jniContext->getMethodID(parameterClass, "getJava", "()Ljava/lang/Class;");
  JniLocalRef<jclass> genericJavaClass = m_jniContext->callObjectMethod<jclass>(genericParameter, getGenericJavaClass);

  data->genericArgumentType = m_jsBridgeContext->getJavaTypes().get(m_jsBridgeContext, genericJavaClass);
  data->genericArgumentParameter = JniGlobalRef<jsBridgeParameter>(genericParameter);
  return data;
}

JavaType::AdditionalData *Array::createAdditionalPushData(const JniRef<jsBridgeParameter> &parameter) const {
  return createAdditionalPopData(parameter);
}

#if defined(DUKTAPE)

JValue Array::pop(bool inScript, const AdditionalData *additionalData) const {
  if (duk_is_null_or_undefined(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }

  if (!duk_is_array(m_ctx, -1)) {
    const auto message = std::string("Cannot convert ") + duk_safe_to_string(m_ctx, -1) + " to array";
    if (inScript) {
      duk_error(m_ctx, DUK_RET_TYPE_ERROR, message.c_str());
    }
    duk_pop(m_ctx);
    if (!inScript) {
      throw std::invalid_argument(message);
    }
  }

  auto additionalArrayData = dynamic_cast<const AdditionalArrayData *>(additionalData);

  const JavaType *componentType = additionalArrayData != nullptr ? additionalArrayData->genericArgumentType : &m_componentType;

  AdditionalData *additionalComponentData = nullptr;
  if (additionalArrayData != nullptr) {
    additionalComponentData = componentType->createAdditionalPopData(additionalArrayData->genericArgumentParameter);
  }
  return componentType->popArray(1, false, inScript, additionalComponentData);
}

duk_ret_t Array::push(const JValue &value, bool inScript, const AdditionalData *additionalData) const {
  JniLocalRef<jarray> jArray(value.getLocalRef().staticCast<jarray>());

  if (jArray.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  auto additionalArrayData = dynamic_cast<const AdditionalArrayData *>(additionalData);

  const JavaType *componentType = additionalArrayData != nullptr ? additionalArrayData->genericArgumentType : &m_componentType;

  AdditionalData *additionalComponentData = nullptr;
  if (additionalArrayData != nullptr) {
    additionalComponentData = componentType->createAdditionalPopData(additionalArrayData->genericArgumentParameter);
  }
  return componentType->pushArray(jArray, false, inScript, additionalComponentData);
}

#elif defined(QUICKJS)

JValue Array::toJava(JSValueConst v, bool inScript, const AdditionalData *additionalData) const {
  if (JS_IsNull(v) || JS_IsUndefined(v)) {
    return JValue();
  }

  assert(JS_IsArray(m_ctx, v));  // TODO: exception handling

  auto additionalArrayData = dynamic_cast<const AdditionalArrayData *>(additionalData);

  const JavaType *componentType = additionalArrayData != nullptr ? additionalArrayData->genericArgumentType : &m_componentType;

  AdditionalData *additionalComponentData = nullptr;
  if (additionalArrayData != nullptr) {
    additionalComponentData = componentType->createAdditionalPopData(additionalArrayData->genericArgumentParameter);
  }

  return componentType->toJavaArray(v, inScript, additionalComponentData);
}

JSValue Array::fromJava(const JValue &value, bool inScript, const AdditionalData *additionalData) const {
  JniLocalRef<jarray> jArray(value.getLocalRef().staticCast<jarray>());

  if (jArray.isNull()) {
    return JS_NULL;
  }

  auto additionalArrayData = dynamic_cast<const AdditionalArrayData *>(additionalData);

  const JavaType *componentType = additionalArrayData != nullptr ? additionalArrayData->genericArgumentType : &m_componentType;

  AdditionalData *additionalComponentData = nullptr;
  if (additionalArrayData != nullptr) {
    additionalComponentData = componentType->createAdditionalPopData(additionalArrayData->genericArgumentParameter);
  }
  return componentType->fromJavaArray(jArray, inScript, additionalComponentData);
}

#endif

}  // namespace JavaTypes

