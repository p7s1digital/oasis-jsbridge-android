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
#include "JavaScriptObject.h"
#include "JavaScriptMethod.h"
#include "JavaType.h"
#include "JniCache.h"
#include "JsBridgeContext.h"
#include "jni-helpers/JniContext.h"
#include "jni-helpers/JniLocalRef.h"
#include "jni-helpers/JniLocalFrame.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include "log.h"
#include <stdexcept>
#include <string>
#include <vector>

#if defined(DUKTAPE)

#include "StackChecker.h"

JavaScriptObject::JavaScriptObject(const JsBridgeContext *jsBridgeContext, std::string strName, duk_idx_t jsObjectIndex, const JObjectArrayLocalRef &methods)
 : m_name(std::move(strName))
 , m_jsBridgeContext(jsBridgeContext) {

  duk_context *ctx = jsBridgeContext->getCContext();
  const JniContext *jniContext = jsBridgeContext->jniContext();
  const JniCache *jniCache = jsBridgeContext->getJniCache();

  CHECK_STACK(ctx);

  m_jsHeapPtr = duk_get_heapptr(ctx, jsObjectIndex);
  duk_push_heapptr(ctx, m_jsHeapPtr);

  if (!duk_is_object(ctx, -1) || duk_is_null(ctx, -1)) {
    duk_pop(ctx);
    throw std::runtime_error("JavaScript object " + m_name + "cannot be accessed");
  }

  if (duk_has_prop_string(ctx, -1, "then")) {
    alog_warn("Registering a JS object from a promise... You probably need to call JsValue.await(), first!");
  }

  // Make sure that the object has all of the methods we want and add them
  const jsize numMethods = methods.getLength();
  for (jsize i = 0; i < numMethods; ++i) {
    const JniLocalRef<jsBridgeMethod> method = methods.getElement<jsBridgeMethod>(i);
    JniCache::MethodInterface methodInterface = jniCache->methodInterface(method);

    // Sanity check that as of right now, the object we're proxying has a function with this name.
    std::string strMethodName = methodInterface.getName().c_str();
    if (!duk_get_prop_string(ctx, -1, strMethodName.c_str())) {
      duk_pop_2(ctx);
      throw std::runtime_error("JS global " + m_name + " has no method called " + strMethodName);
    } else if (!duk_is_callable(ctx, -1)) {
      duk_pop_2(ctx);
      throw std::runtime_error("JS property " + m_name + "." + strMethodName + " not callable");
    }

    try {
      JniLocalRef<jobject> javaMethod = methodInterface.getJavaMethod();
      jmethodID methodId = jniContext->fromReflectedMethod(javaMethod);

      // Build a call wrapper that handles marshalling the arguments and return value.
      auto jsMethod = new JavaScriptMethod(jsBridgeContext, method, strMethodName, false);

      m_methods.emplace(methodId, std::shared_ptr<JavaScriptMethod>(jsMethod));
    } catch (const std::invalid_argument &e) {
      duk_pop_2(ctx);
      throw std::invalid_argument("In proxied method \"" + m_name + "." + strMethodName + "\": " + e.what());
    }

    // Pop the method property.
    duk_pop(ctx);
  }

  duk_pop(ctx);  // JS object
}

JValue JavaScriptObject::call(const JniLocalRef<jobject> &javaMethod, const JObjectArrayLocalRef &args) const {

  if (m_jsHeapPtr == nullptr) {
    m_jsBridgeContext->queueJsException(
        "JavaScript object " + m_name + " has been garbage collected");
    return JValue();
  }

  const JniContext *jniContext = m_jsBridgeContext->jniContext();
  const JniCache *jniCache = m_jsBridgeContext->getJniCache();

  jmethodID methodId = jniContext->fromReflectedMethod(javaMethod);

  auto getMethodName = [&]() {
    return jniCache->getJavaMethodName(javaMethod).c_str();
  };

  //alog("Invoking JS method %s.%s...", m_name.c_str(), getMethodName().c_str());

  const auto methodIt = m_methods.find(methodId);

  try {
    if (methodIt == m_methods.end()) {
      throw std::runtime_error("Could not find method " + m_name + "." + getMethodName());
    }

    // Method found -> call it
    const auto &jsMethod = methodIt->second;
    return jsMethod->invoke(m_jsBridgeContext, m_jsHeapPtr, args, false);
  } catch (const std::runtime_error &e) {
    std::string strError("Error while calling JS method " + m_name + "." + getMethodName() + ": " + e.what());
    throw std::runtime_error(strError);
  }
}

#elif defined(QUICKJS)

#include "QuickJsUtils.h"

JavaScriptObject::JavaScriptObject(const JsBridgeContext *jsBridgeContext, std::string strName, JSValue v, const JObjectArrayLocalRef &methods)
 : m_name(std::move(strName))
 , m_jsValue(v)
 , m_jsBridgeContext(jsBridgeContext) {

  JSContext *ctx = jsBridgeContext->getCContext();
  const JniContext *jniContext = jsBridgeContext->jniContext();
  const JniCache *jniCache = jsBridgeContext->getJniCache();
  const QuickJsUtils *utils = jsBridgeContext->getUtils();

  if (!JS_IsObject(v)) {
    throw std::runtime_error("JavaScript object " + strName + " cannot be accessed (not an object)");
  }

  if (utils->hasPropertyStr(v, "then")) {
    alog_warn("Registering a JS object from a promise... You probably need to call JsValue.await(), first!");
  }

  // Make sure that the object has all of the methods we want and add them
  const jsize numMethods = methods.getLength();
  for (jsize i = 0; i < numMethods; ++i) {
    JniLocalRef<jsBridgeMethod > method = methods.getElement<jsBridgeMethod>(i);
    JniCache::MethodInterface methodInterface = jniCache->methodInterface(method);

    // Sanity check that as of right now, the object we're proxying has a function with this name.
    std::string strMethodName = methodInterface.getName().c_str();
    JSValue methodValue = JS_GetPropertyStr(ctx, m_jsValue, strMethodName.c_str());
    if (JS_IsUndefined(methodValue)) {
      throw std::runtime_error("JS global " + m_name + " has no method called " + strMethodName);
    } else if (!JS_IsFunction(ctx, methodValue)) {
      throw std::runtime_error("JS property " + m_name + "." + strMethodName + " is not function");
    }

    JS_FreeValue(ctx, methodValue);

    try {
      JniLocalRef<jobject> javaMethod = methodInterface.getJavaMethod();
      jmethodID methodId = jniContext->fromReflectedMethod(javaMethod);

      // Build a call wrapper that handles marshalling the arguments and return value.
      auto jsMethod = new JavaScriptMethod(jsBridgeContext, method, strMethodName, false);

      m_methods.emplace(methodId, std::shared_ptr<JavaScriptMethod>(jsMethod));
    } catch (const std::invalid_argument &e) {
      // TODO: free values?
      throw std::invalid_argument("In proxied method \"" + m_name + "." + strMethodName + "\": " + e.what());
    }
  }
}

JValue JavaScriptObject::call(const JniLocalRef<jobject> &javaMethod, const JObjectArrayLocalRef &args) const {

  JSContext *ctx = m_jsBridgeContext->getCContext();
  const JniContext *jniContext = m_jsBridgeContext->jniContext();
  const JniCache *jniCache = m_jsBridgeContext->getJniCache();

  jmethodID methodId = jniContext->fromReflectedMethod(javaMethod);

  auto getMethodName = [&]() {
    return jniCache->getJavaMethodName(javaMethod).c_str();
  };

  //alog("Invoking JS method %s.%s...", m_name.c_str(), getMethodName().c_str());

  const auto methodIt = m_methods.find(methodId);

  try {
    if (methodIt == m_methods.end()) {
      throw std::runtime_error("Could not find method " + m_name + "." + getMethodName());
    }

    const JavaScriptMethod *jsMethod = methodIt->second.get();

    JSValue jsMethodValue = JS_GetPropertyStr(ctx, m_jsValue, jsMethod->getName().c_str());
    if (!JS_IsFunction(ctx, jsMethodValue)) {
      JS_FreeValue(ctx, jsMethodValue);
      throw std::runtime_error("Error while calling JS method");
    }

    return jsMethod->invoke(m_jsBridgeContext, jsMethodValue, m_jsValue, args, false);
  } catch (const std::runtime_error &e) {
    std::string strError("Error while calling JS method " + m_name + "." + getMethodName() + ": " + e.what());
    throw std::runtime_error(strError);
  }

  return JValue();
}

#endif
