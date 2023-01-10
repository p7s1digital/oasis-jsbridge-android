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
#include "AidlInterface.h"

#include "ExceptionHandler.h"
#include "JniCache.h"
#include "JsBridgeContext.h"
#include "JavaMethod.h"
#include "JavaScriptObject.h"
#include "log.h"
#include "exceptions/JniException.h"
#include "jni-helpers/JniContext.h"
#include "jni-helpers/JniGlobalRef.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include <memory>

#if defined(DUKTAPE)
# include "DuktapeUtils.h"
# include "StackChecker.h"
#elif defined(QUICKJS)
# include "QuickJsUtils.h"
#endif

#define EXTRACT_QUALIFIED_FUNCTION_NAME

namespace {
  const char *JS_AIDL_INTERFACE_GLOBAL_NAME_PREFIX = "__javaTypes_aidlInterface_";  // Note: initial "\xff\xff" removed because of JNI string conversion issues
}


namespace JavaTypes {

AidlInterface::AidlInterface(const JsBridgeContext *jsBridgeContext, const JniRef<jsBridgeParameter> &parameter)
 : JavaType(jsBridgeContext, JavaTypeId::AidlInterface)
 , m_parameter(parameter) {
}

#if defined(DUKTAPE)

// Pop a JS object, register and create a native wrapper (JavaScriptObject)
// - C++ -> Java: not supported
// - Java -> C++: call JS methods
JValue AidlInterface::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (!duk_is_object(m_ctx, -1) && !duk_is_null(m_ctx, -1)) {
    const char *message = "Cannot convert return value to AidlInterface";
    duk_pop(m_ctx);
    CHECK_STACK_NOW();
    throw std::invalid_argument(message);
  }

  const DuktapeUtils *utils = m_jsBridgeContext->getUtils();
  std::string aidlInterfaceJavaName = m_jsBridgeContext->getJniCache()->getParameterInterface(m_parameter).getJavaName().toStdString();

  // First check if this is already a known AIDL interface instance
  if (duk_get_prop_string(m_ctx, -1, aidlInterfaceJavaName.c_str())) {
    JniLocalRef<jobject> javaRef = utils->getJavaRef<jobject>(-1);
    duk_pop(m_ctx);  // prop aidlInterfaceJavaName
    duk_pop(m_ctx);  // JS object
    return JValue(javaRef);
  }
  duk_pop(m_ctx);  // prop aidlInterfaceJavaName

  JObjectArrayLocalRef javaMethods = getJniJavaMethods();

  static int jsAidlInterfaceCount = 0;
  std::string jsAidlInterfaceGlobalName = JS_AIDL_INTERFACE_GLOBAL_NAME_PREFIX + std::to_string(++jsAidlInterfaceCount);

  // 1. Get the JS object which needs to be accessed from native
  duk_require_object(m_ctx, -1);
  duk_idx_t jsObjectIdx = duk_normalize_index(m_ctx, -1);

  // 2. Duplicate it into the global object with prop name <functionId>
  duk_push_global_object(m_ctx);
  duk_dup(m_ctx, jsObjectIdx);
  duk_put_prop_string(m_ctx, -2, jsAidlInterfaceGlobalName.c_str());
  duk_pop(m_ctx);  // global object

  // 3. Create JavaScriptObject C++ object and wrap it inside the JS object
  auto javaScriptObject = new JavaScriptObject(m_jsBridgeContext, jsAidlInterfaceGlobalName, jsObjectIdx, javaMethods, false);
  utils->createMappedCppPtrValue<JavaScriptObject>(javaScriptObject, -1, jsAidlInterfaceGlobalName.c_str());

  // 4. Call native createAidlInterfaceProxy(id, aidlStub, javaMethods)
  JniLocalRef<jobject> aidlInterfaceObject = getJniCache()->getJsBridgeInterface().createAidlInterfaceProxy(
      JStringLocalRef(m_jniContext, jsAidlInterfaceGlobalName.c_str()),
      m_parameter
  );
  if (m_jniContext->exceptionCheck()) {
    duk_pop(m_ctx);  // JS object
    throw JniException(m_jniContext);
  }

  // 5. Store the Java reference
  utils->pushJavaRefValue(aidlInterfaceObject);
  duk_put_prop_string(m_ctx, jsObjectIdx, aidlInterfaceJavaName.c_str());

  duk_pop(m_ctx);  // JS object

  return JValue(aidlInterfaceObject);
}

duk_ret_t AidlInterface::push(const JValue &value) const {
  throw std::invalid_argument("Cannot push an AidlInterface!");
}

#elif defined(QUICKJS)

// Get a JS object, register and create a native wrapper (JavaScriptObject)
// - C++ -> Java: not supported
// - Java -> C++: call JS methods
JValue AidlInterface::toJava(JSValueConst v) const {
  const QuickJsUtils *utils = m_jsBridgeContext->getUtils();
  assert(utils != nullptr);

  if (!JS_IsObject(v) || JS_IsNull(v)) {
    throw std::invalid_argument("Cannot convert return value to AidlInterface");
  }

  std::string aidlInterfaceJavaName = m_jsBridgeContext->getJniCache()->getParameterInterface(m_parameter).getJavaName().toStdString();

  // First check if this is already a known AIDL interface instance
  JSValue existingJavaAidlInterfaceValue = JS_GetPropertyStr(m_ctx, v, aidlInterfaceJavaName.c_str());
  if (!JS_IsUndefined(existingJavaAidlInterfaceValue)) {
    JniLocalRef<jobject> javaRef = utils->getJavaRef<jobject>(existingJavaAidlInterfaceValue);
    JS_FreeValue(m_ctx, existingJavaAidlInterfaceValue);
    return JValue(javaRef);
  }

  static int jsAidlInterfaceCount = 0;
  std::string jsAidlInterfaceGlobalName = JS_AIDL_INTERFACE_GLOBAL_NAME_PREFIX + std::to_string(++jsAidlInterfaceCount);

  JObjectArrayLocalRef javaMethods = getJniJavaMethods();

  // 1. Duplicate it into the global object with prop name <aidlInterfaceId>
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JS_SetPropertyStr(m_ctx, globalObj, jsAidlInterfaceGlobalName.c_str(), JS_DupValue(m_ctx, v));
  JS_FreeValue(m_ctx, globalObj);

  // 2. Create the C++ JavaScriptObject instance
  auto javaScriptObject = new JavaScriptObject(m_jsBridgeContext, jsAidlInterfaceGlobalName, v, javaMethods, false);

  // 3. Wrap it inside the JS object
  utils->createMappedCppPtrValue<JavaScriptObject>(javaScriptObject, v, jsAidlInterfaceGlobalName.c_str());

  // 4. Call native createAidlInterfaceProxy(id, aidlStub, javaMethods)
  JniLocalRef<jobject> aidlInterfaceObject = getJniCache()->getJsBridgeInterface().createAidlInterfaceProxy(
      JStringLocalRef(m_jniContext, jsAidlInterfaceGlobalName.c_str()),
      m_parameter
  );
  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  // 5. Store the Java reference
  JSValue javaRefJsValue = utils->createJavaRefValue(aidlInterfaceObject);
  JS_SetPropertyStr(m_ctx, v, aidlInterfaceJavaName.c_str(), javaRefJsValue);

  return JValue(aidlInterfaceObject);
}

JSValue AidlInterface::fromJava(const JValue &value) const {
   throw std::invalid_argument("Cannot push an AidlInterface!");
}

#endif


// Private methods
// ---

JniLocalRef<jclass> AidlInterface::getJavaClass() const {
  ParameterInterface parameterInterface = m_jsBridgeContext->getJniCache()->getParameterInterface(m_parameter);
  return parameterInterface.getJava();
}

JObjectArrayLocalRef AidlInterface::getJniJavaMethods() const {
  if (!m_lazyJniJavaMethods.isNull()) {
    return JObjectArrayLocalRef(m_lazyJniJavaMethods);
  }

  JObjectArrayLocalRef methods = getJniCache()->getParameterInterface(m_parameter).getMethods();

  m_lazyJniJavaMethods = JniGlobalRef<jobjectArray>(methods);
  if (m_lazyJniJavaMethods.isNull()) {
    alog_warn("Could not create JsBridge methods from parameter!");
  }

  return JObjectArrayLocalRef(m_lazyJniJavaMethods);
}

}  // namespace JavaTypes
