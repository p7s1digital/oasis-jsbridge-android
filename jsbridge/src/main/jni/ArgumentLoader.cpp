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
#include "ArgumentLoader.h"

#include "JsBridgeContext.h"
#include "JavaTypeMap.h"
#include "java-types/Deferred.h"
#include "jni-helpers/JniContext.h"
#include "jni-helpers/JniTypes.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include "jni-helpers/JValue.h"

ArgumentLoader::ArgumentLoader(const JavaType *javaType, const JniRef<jsBridgeParameter> &parameter, bool inScript)
 : m_javaType(javaType)
 , m_parameter(parameter)
 , m_inScript(inScript) {
}

ArgumentLoader::ArgumentLoader(ArgumentLoader &&other) noexcept
 : m_javaType(other.m_javaType)
 , m_parameter(std::move(other.m_parameter))
 , m_inScript(other.m_inScript)
 , m_additionalPopData(other.m_additionalPopData)
 , m_additionalPushData(other.m_additionalPopData)
 , m_hasAdditionalPopData(false)
 , m_hasAdditionalPushData(false) {

  // Avoid double deletion of additional pop/push data instance
  other.m_additionalPopData = nullptr;
  other.m_additionalPushData = nullptr;
}

ArgumentLoader::~ArgumentLoader() {
  delete m_additionalPopData;
  delete m_additionalPushData;
}

#if defined(DUKTAPE)

JValue ArgumentLoader::pop() const {
  if (!m_hasAdditionalPopData) {
    m_additionalPopData = m_javaType->createAdditionalPopData(m_parameter);
  }

  return m_javaType->pop(m_inScript, m_additionalPopData);
}

JValue ArgumentLoader::popArray(uint32_t count, bool expanded) const {
  if (!m_hasAdditionalPopData) {
    m_additionalPopData = m_javaType->createAdditionalPopData(m_parameter);
  }

  return m_javaType->popArray(count, expanded, m_inScript, m_additionalPopData);
}

JValue ArgumentLoader::popDeferred(const JavaTypeMap *javaTypes) const {
  const JavaTypes::Deferred *deferredType = javaTypes->getDeferredType(m_javaType->getJsBridgeContext());
  return deferredType->pop(false, m_javaType, m_parameter);
}

duk_ret_t ArgumentLoader::push(const JValue &value) const {
  if (!m_hasAdditionalPushData) {
    m_additionalPushData = m_javaType->createAdditionalPushData(m_parameter);
  }

  return m_javaType->push(value, m_inScript, m_additionalPushData);
}

duk_ret_t ArgumentLoader::pushArray(const JniLocalRef<jarray> &values, bool expand) const {
  if (!m_hasAdditionalPushData) {
    m_additionalPushData = m_javaType->createAdditionalPushData(m_parameter);
  }

  return m_javaType->pushArray(values, expand, m_inScript, m_additionalPopData);
}

#elif defined(QUICKJS)

JValue ArgumentLoader::toJava(JSValueConst v) const {
  if (!m_hasAdditionalPopData) {
    m_additionalPopData = m_javaType->createAdditionalPopData(m_parameter);
  }

  return m_javaType->toJava(v, m_inScript, m_additionalPopData);
}

JValue ArgumentLoader::toJavaArray(JSValueConst v) const {
  if (!m_hasAdditionalPopData) {
    m_additionalPopData = m_javaType->createAdditionalPopData(m_parameter);
  }

  return m_javaType->toJavaArray(v, m_inScript, m_additionalPopData);
}

JValue ArgumentLoader::toJavaDeferred(JSValueConst v, const JavaTypeMap *javaTypes) const {
  const JavaTypes::Deferred *deferredType = javaTypes->getDeferredType(m_javaType->getJsBridgeContext());
  return deferredType->toJava(v, false, m_javaType, m_parameter);
}

JSValue ArgumentLoader::fromJava(const JValue &value) const {
  if (!m_hasAdditionalPushData) {
    m_additionalPushData = m_javaType->createAdditionalPushData(m_parameter);
  }

  return m_javaType->fromJava(value, m_inScript, m_additionalPushData);
}

JSValue ArgumentLoader::fromJavaArray(const JniLocalRef<jarray> &values) const {
  if (!m_hasAdditionalPushData) {
    m_additionalPushData = m_javaType->createAdditionalPushData(m_parameter);
  }

  return m_javaType->fromJavaArray(values, m_inScript, m_additionalPopData);
}

#endif

JValue ArgumentLoader::callMethod(jmethodID methodID, const JniRef<jobject> &javaThis, const std::vector<JValue> &args) const {
  return m_javaType->callMethod(methodID, javaThis, args);
}

JValue ArgumentLoader::callLambda(const JniRef<jsBridgeMethod> &method, const JniRef<jobject> &javaThis, const std::vector<JValue> &args) const {
  const JniContext *jniContext = m_javaType->getJniContext();
  const JsBridgeContext *jsBridgeContext = m_javaType->getJsBridgeContext();

  JniLocalRef<jclass> objectClass = jniContext->findClass("java/lang/Object");
  JObjectArrayLocalRef argArray(jniContext, args.size(), objectClass);
  int i = 0;
  for (auto &arg : args) {
    argArray.setElement(i++, arg.isNull() ? JniLocalRef<jobject>() : arg.getLocalRef());
  }

  const JniRef<jclass> &methodClass = jniContext->getJsBridgeMethodClass();
  jmethodID callNativeLambda = jniContext->getMethodID(methodClass, "callNativeLambda", "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");
  JniLocalRef<jobject> ret = jniContext->callObjectMethod(method, callNativeLambda, javaThis, argArray);

  jsBridgeContext->checkRethrowJsError();
  return JValue(ret);
}
