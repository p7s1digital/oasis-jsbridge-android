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
#include "FunctionX.h"

#include "JniCache.h"
#include "JsBridgeContext.h"
#include "JavaMethod.h"
#include "JavaScriptLambda.h"
#include "log.h"
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

namespace {
  const char *JS_FUNCTION_GLOBAL_NAME_PREFIX = "__javaTypes_functionX_";  // Note: initial "\xff\xff" removed because of JNI string conversion issues
  const char *PAYLOAD_PROP_NAME = "\xff\xffpayload";

  struct CallJavaLambdaPayload {
    JniGlobalRef<jobject> javaThis;
    std::shared_ptr<JavaMethod> javaMethodPtr;
  };

#if defined(DUKTAPE)
  extern "C"
  duk_ret_t callJavaLambda(duk_context *ctx) {
    CHECK_STACK(ctx);

    duk_push_current_function(ctx);
    if (!duk_get_prop_string(ctx, -1, PAYLOAD_PROP_NAME)) {
      duk_pop_2(ctx);  // (undefined) javaThis + current function
      return DUK_RET_ERROR;
    }

    auto payload = reinterpret_cast<const CallJavaLambdaPayload *>(duk_require_pointer(ctx, -1));
    duk_pop_2(ctx);  // Payload + current function

    JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
    assert(jsBridgeContext != nullptr);

    CHECK_STACK_NOW();
    return payload->javaMethodPtr->invoke(jsBridgeContext, payload->javaThis);
  }

  extern "C"
  duk_ret_t finalizeJavaLambda(duk_context *ctx) {
    CHECK_STACK(ctx);

    JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
    assert(jsBridgeContext != nullptr);

    if (duk_get_prop_string(ctx, -1, PAYLOAD_PROP_NAME)) {
      delete reinterpret_cast<const CallJavaLambdaPayload *>(duk_require_pointer(ctx, -1));
    }

    duk_pop(ctx);  // Payload
    return 0;
  }
#elif defined(QUICKJS)
  JSValue callJavaLambda(JSContext *ctx, JSValue, int argc, JSValueConst *argv, int /*magic*/, JSValueConst *datav) {
    JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
    assert(jsBridgeContext != nullptr);

    const QuickJsUtils *utils = jsBridgeContext->getUtils();
    assert(utils != nullptr);

    // Get the C++ CallJavaLambdaPayload from data
    auto payload = QuickJsUtils::getCppPtr<CallJavaLambdaPayload>(*datav);
    if (payload->javaMethodPtr.get() == nullptr) {
      const char *message = "Cannot call Java lambda: JavaMethod is null";
      JS_ThrowTypeError(ctx, "%s", message);
      return JS_EXCEPTION;
    }

    return payload->javaMethodPtr->invoke(jsBridgeContext, payload->javaThis, argc, argv);
  }
#endif
}


namespace JavaTypes {

FunctionX::FunctionX(const JsBridgeContext *jsBridgeContext, const JniRef<jsBridgeParameter> &parameter)
 : JavaType(jsBridgeContext, JavaTypeId::FunctionX)
 , m_parameter(parameter) {
}

#if defined(DUKTAPE)

// Pop a JS function, register and create a native wrapper (JavaScriptLambda)
// - C++ -> Java: call createJsLambdaProxy with <functionId> as argument
// - Java -> C++: call callJsLambda (with <functionId> + args parameters)
JValue FunctionX::pop(bool inScript) const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (!duk_is_function(m_ctx, -1) && !duk_is_null(m_ctx, -1)) {
    const char *message = "Cannot convert return value to FunctionX";
    duk_pop(m_ctx);
    CHECK_STACK_NOW();
    m_jsBridgeContext->throwTypeException(message, inScript);
  }

  static int jsFunctionCount = 0;
  std::string jsFunctionGlobalName = JS_FUNCTION_GLOBAL_NAME_PREFIX + std::to_string(++jsFunctionCount);

  const JniRef<jsBridgeMethod> &javaMethod = getJniJavaMethod();
  const DuktapeUtils *utils = m_jsBridgeContext->getUtils();

  // 1. Get the JS function which needs to be triggered from native
  duk_require_function(m_ctx, -1);
  duk_idx_t jsFuncIdx = duk_normalize_index(m_ctx, -1);

  // 2. Duplicate it into the global object with prop name <functionId>
  duk_push_global_object(m_ctx);
  duk_dup(m_ctx, jsFuncIdx);
  duk_put_prop_string(m_ctx, -2, jsFunctionGlobalName.c_str());
  duk_pop(m_ctx);  // global object

  // 3. Create JavaScriptLambda C++ object and wrap it inside the JS function
  auto javaScriptLambda = new JavaScriptLambda(m_jsBridgeContext, javaMethod, jsFunctionGlobalName, -1);
  utils->createMappedCppPtrValue<JavaScriptLambda>(javaScriptLambda, -1, jsFunctionGlobalName.c_str());
  duk_pop(m_ctx);  // JS function

  // 4. Call native createJsLambdaProxy(id, javaMethod)
  JniLocalRef<jobject> javaFunction = getJniCache()->getJsBridgeInterface().createJsLambdaProxy(
      JStringLocalRef(m_jniContext, jsFunctionGlobalName.c_str()),
      javaMethod
  );
  m_jsBridgeContext->rethrowJniException();

  return JValue(javaFunction);
}

JValue FunctionX::popArray(uint32_t count, bool expanded, bool inScript) const {
  expanded ? duk_pop_n(m_ctx, count) : duk_pop(m_ctx);

  const char *message = "Cannot pop an array of functions!";
  m_jsBridgeContext->throwTypeException(message, inScript);

  // Unreachable
  return JValue();
}

// Get a native function, register it and push a JS wrapper
duk_ret_t FunctionX::push(const JValue &value, bool /*inScript*/) const {

  // 1. C++: create the JValue object which is a Java FunctionX instance
  const JniLocalRef<jobject> &javaFunctionObject = value.getLocalRef();

  if (javaFunctionObject.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  // 2. C++: create and push a JS function which invokes the JavaMethod with the above Java this
  const duk_idx_t funcIdx = duk_push_c_function(m_ctx, callJavaLambda, DUK_VARARGS);

  // Bind Payload
  auto payload = new CallJavaLambdaPayload { JniGlobalRef<jobject>(javaFunctionObject), getCppJavaMethod() };
  duk_push_pointer(m_ctx, payload);
  duk_put_prop_string(m_ctx, funcIdx, PAYLOAD_PROP_NAME);

  // Finalizer (which releases the JavaMethod instance)
  duk_push_c_function(m_ctx, finalizeJavaLambda, 1);
  duk_set_finalizer(m_ctx, funcIdx);

  return 1;
}

duk_ret_t FunctionX::pushArray(const JniLocalRef<jarray> &, bool /*expand*/, bool inScript) const {
  const char *message = "Cannot push an array of functions!";
  m_jsBridgeContext->throwTypeException(message, inScript);

  // Unreachable
  return DUK_RET_ERROR;
}

#elif defined(QUICKJS)

// Get a JS function, register and create a native wrapper (JavaScriptLambda)
// - C++ -> Java: call createJsLambdaProxy with <functionId> as argument
// - Java -> C++: call callJsLambda (with <functionId> + args parameters)
JValue FunctionX::toJava(JSValueConst v, bool inScript) const {
  const QuickJsUtils *utils = m_jsBridgeContext->getUtils();
  assert(utils != nullptr);

  if (!JS_IsFunction(m_ctx, v) && !JS_IsNull(v)) {
    const char *message = "Cannot convert return value to FunctionX";
    m_jsBridgeContext->throwTypeException(message, inScript);
    return JValue();
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
  utils->createMappedCppPtrValue<JavaScriptLambda>(javaScriptLambda, JS_DupValue(m_ctx, v), jsFunctionGlobalName.c_str());

  // 4. Call native createJsLambdaProxy(id, javaMethod)
  JniLocalRef<jobject> javaFunction = getJniCache()->getJsBridgeInterface().createJsLambdaProxy(
      JStringLocalRef(m_jniContext, jsFunctionGlobalName.c_str()),
      jniJavaMethod
  );
  if (m_jsBridgeContext->hasPendingJniException()) {
    m_jsBridgeContext->rethrowJniException();
    return JValue();
  }

  return JValue(javaFunction);
}

JValue FunctionX::toJavaArray(JSValueConst, bool inScript) const {
  const char *message = "Cannot transfer from JS to Java an array of functions!";
  m_jsBridgeContext->throwTypeException(message, inScript);

  // Unreachable
  return JValue();
}

// Get a native function, register it and return JS wrapper
JSValue FunctionX::fromJava(const JValue &value, bool inScript) const {
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

  return invokeFunctionValue;
}

JSValue FunctionX::fromJavaArray(const JniLocalRef<jarray> &, bool inScript) const {
  const char *message = "Cannot transfer from Java to JS an array of functions!";
  m_jsBridgeContext->throwTypeException(message, inScript);

  return JS_EXCEPTION;
}
#endif


// Private methods
// ---

const JniRef<jsBridgeMethod> &FunctionX::getJniJavaMethod() const {
  if (!m_lazyJniJavaMethod.isNull()) {
    return m_lazyJniJavaMethod;
  }

  JniLocalRef<jsBridgeMethod> invokeMethod = getJniCache()->getParameterInterface(m_parameter).getInvokeMethod();

  m_lazyJniJavaMethod = JniGlobalRef<jsBridgeMethod>(invokeMethod);
  if (m_lazyJniJavaMethod.isNull()) {
    alog_warn("Could not create JsBridge method instance from parameter!");
  }

  return m_lazyJniJavaMethod;
}

const std::shared_ptr<JavaMethod> &FunctionX::getCppJavaMethod() const {
  if (m_lazyCppJavaMethod.get() != nullptr)   {
    return m_lazyCppJavaMethod;
  }

#ifdef NDEBUG
  static const char *functionXName = "<FunctionX>";
#else
  JStringLocalRef paramNameRef = getJniCache()->getParameterInterface(m_parameter).getName();
  std::string methodName = "<method>";
  std::string paramName = paramNameRef.isNull() ? "_" : paramNameRef.str();
  std::string functionXName = "<FunctionX>/" + methodName + "::" + paramName;
#endif

  m_lazyCppJavaMethod = std::make_shared<JavaMethod>(m_jsBridgeContext, getJniJavaMethod(), functionXName, true /*isLambda*/);
  return m_lazyCppJavaMethod;
}

}  // namespace JavaTypes
