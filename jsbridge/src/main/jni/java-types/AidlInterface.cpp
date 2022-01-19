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
 : JavaType(jsBridgeContext, JavaTypeId::FunctionX)
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

  JObjectArrayLocalRef javaMethods = getJniJavaMethods();
  const DuktapeUtils *utils = m_jsBridgeContext->getUtils();

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
  duk_pop(m_ctx);  // JS object

  // 4. Call native createAidlInterfaceProxy(id, aidlStub, javaMethods)
  JniLocalRef<jobject> aidlInterfaceObject = getJniCache()->getJsBridgeInterface().createAidlInterfaceProxy(
      JStringLocalRef(m_jniContext, jsAidlInterfaceGlobalName.c_str()),
      m_parameter
  );
  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  return JValue(aidlInterfaceObject);
}

JValue AidlInterface::popArray(uint32_t count, bool expanded) const {
  throw std::invalid_argument("Cannot pop an array of AidlInterface's!");
}

// Get a native function, register it and push a JS wrapper
duk_ret_t AidlInterface::push(const JValue &value) const {
  throw std::invalid_argument("Cannot push AidlInterface!");
}

duk_ret_t AidlInterface::pushArray(const JniLocalRef<jarray> &, bool /*expand*/) const {
  throw std::invalid_argument("Cannot push an array of AidlInterface's!");
}

#elif defined(QUICKJS)

// Get a JS function, register and create a native wrapper (JavaScriptLambda)
// - C++ -> Java: call createJsLambdaProxy with <functionId> as argument
// - Java -> C++: call callJsLambda (with <functionId> + args parameters)
JValue FunctionX::toJava(JSValueConst v) const {
  const QuickJsUtils *utils = m_jsBridgeContext->getUtils();
  assert(utils != nullptr);

  if (!JS_IsFunction(m_ctx, v) && !JS_IsNull(v)) {
    throw std::invalid_argument("Cannot convert return value to FunctionX");
  }

  static int jsFunctionCount = 0;
  std::string jsFunctionGlobalName = JS_FUNCTION_GLOBAL_NAME_PREFIX + std::to_string(++jsFunctionCount);

  const JniRef<jsBridgeMethod> &jniJavaMethod = getJniJavaMethod();

  // 1. Duplicate it into the global object with prop name <functionId>
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JS_SetPropertyStr(m_ctx, globalObj, jsFunctionGlobalName.c_str(), JS_DupValue(m_ctx, v));
  JS_FreeValue(m_ctx, globalObj);

  // 2. Create the  C++ JavaScriptLambda instance
  auto javaScriptLambda = new JavaScriptLambda(m_jsBridgeContext, jniJavaMethod, jsFunctionGlobalName, v);

  // 3. Wrap it inside the JS function
  utils->createMappedCppPtrValue<JavaScriptLambda>(javaScriptLambda, v, jsFunctionGlobalName.c_str());

  // 4. Call native createJsLambdaProxy(id, javaMethod)
  JniLocalRef<jobject> javaFunction = getJniCache()->getJsBridgeInterface().createJsLambdaProxy(
      JStringLocalRef(m_jniContext, jsFunctionGlobalName.c_str()),
      jniJavaMethod
  );
  if (m_jniContext->exceptionCheck()) {
    throw JniException(m_jniContext);
  }

  return JValue(javaFunction);
}

JValue FunctionX::toJavaArray(JSValueConst) const {
  throw std::invalid_argument("Cannot transfer from JS to Java an array of functions!");
}

// Get a native function, register it and return JS wrapper
JSValue FunctionX::fromJava(const JValue &value) const {
  const QuickJsUtils *utils = m_jsBridgeContext->getUtils();
  assert(utils != nullptr);

  // 1. Get C++ JavaMethod instance
  const std::shared_ptr<JavaMethod> &javaMethodPtr = getCppJavaMethod();

  // 2. C++: create the JValue object which is a Java FunctionX instance
  const JniLocalRef<jobject> &javaFunctionObject = value.getLocalRef();
  if (javaFunctionObject.isNull()) {
    return JS_NULL;
  }

  // 3. C++: create a JS function which invokes the JavaMethod with the above Java this
  auto payload = new CallJavaLambdaPayload { JniGlobalRef<jobject>(javaFunctionObject), javaMethodPtr };
  JSValue payloadValue = utils->createCppPtrValue<CallJavaLambdaPayload>(payload, true);
  JSValue invokeFunctionValue = JS_NewCFunctionData(m_ctx, callJavaLambda, 1, 0, 1, &payloadValue);

  JS_FreeValue(m_ctx, payloadValue);

  return invokeFunctionValue;
}

JSValue FunctionX::fromJavaArray(const JniLocalRef<jarray> &) const {
  throw std::invalid_argument("Cannot transfer from Java to JS an array of functions!");
}

#endif


// Private methods
// ---

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
