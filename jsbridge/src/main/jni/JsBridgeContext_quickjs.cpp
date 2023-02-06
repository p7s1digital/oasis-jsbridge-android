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
#include "JsBridgeContext.h"

#include "AutoReleasedJSValue.h"
#include "ExceptionHandler.h"
#include "JavaObject.h"
#include "JavaScriptLambda.h"
#include "JavaScriptObject.h"
#include "JavaType.h"
#include "JavaTypeProvider.h"
#include "JniCache.h"
#include "QuickJsUtils.h"
#include "custom_stringify.h"
#include "log.h"
#include "exceptions/JniException.h"
#include "exceptions/JsException.h"
#include "java-types/Deferred.h"
#include "java-types/Object.h"
#include <functional>


// Internal
// ---

namespace {
  const char *JSBRIDGE_CPP_CLASS_PROP_NAME = "__jsbridge_cpp";

  //int interrupt_handler(JSRuntime *rt, void *opaque) {
  //  return 0;
  //}

  JSModuleDef *jsModuleLoader(JSContext *ctx, const char *moduleName, void *opaque) {
    JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
    JniContext *jniContext = jsBridgeContext->getJniContext();
    auto contentRef = jsBridgeContext->getJniCache()->getJsBridgeInterface().callJsModuleLoader(JStringLocalRef(jniContext, moduleName));

    if (jniContext->exceptionCheck()) {
      throw JniException(jniContext);
    }

    if (contentRef.isNull()) {
      throw std::invalid_argument("JS module returned a null content");
    }

    const char *content = contentRef.toUtf8Chars();
    const size_t contentLength = contentRef.utf8Length();

    JSValue funcVal = JS_Eval(ctx, content, contentLength, moduleName, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);

    if (JS_IsException(funcVal)) {
      return nullptr;
    }

    auto m = (JSModuleDef *) JS_VALUE_GET_PTR(funcVal);
    JS_FreeValue(ctx, funcVal);

    return m;
  }

  void promiseRejectionTracker(JSContext *ctx, JSValueConst promise, JSValueConst reason, JS_BOOL isHandled, void *opaque) {
    if (isHandled) return;

    JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
    const ExceptionHandler *exceptionHandler = jsBridgeContext->getExceptionHandler();

    JsException jsException(jsBridgeContext, JS_DupValue(ctx, reason));
    JValue value(exceptionHandler->getJavaException(jsException));

    // Reject the Java Deferred
    jsBridgeContext->getJniCache()->getJsBridgeInterface().addUnhandledJsPromiseException(value);
  }
}


// Class methods
// ---

JsBridgeContext::JsBridgeContext()
 : m_javaTypeProvider(this) {
}

JsBridgeContext::~JsBridgeContext() {
  JS_FreeContext(m_ctx);
  JS_FreeRuntime(m_runtime);

  delete m_exceptionHandler;
  delete m_utils;
  delete m_jniCache;
}

void JsBridgeContext::init(JniContext *jniContext, const JniLocalRef<jobject> &jsBridgeObject) {
  m_jniContext = jniContext;

  m_runtime = JS_NewRuntime();

  //JS_SetInterruptHandler(rt, interrupt_handler, NULL)

  m_ctx = JS_NewContext(m_runtime);
  JS_SetMaxStackSize(m_runtime, 1 * 1024 * 1024);  // default: 256kb, now: 1MB

  m_jniCache = new JniCache(this, jsBridgeObject);
  m_utils = new QuickJsUtils(jniContext, m_ctx);
  m_exceptionHandler = new ExceptionHandler(this);

  // Store the JsBridgeContext instance in the global object so we can find our way back from a C callback
  JSValue cppWrapperObj = m_utils->createCppPtrValue(this, false);
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JS_SetPropertyStr(m_ctx, globalObj, JSBRIDGE_CPP_CLASS_PROP_NAME, cppWrapperObj);
  // No JS_FreeValue(m_ctx, cppWrapperObj) after JS_SetPropertyStr()
  JS_FreeValue(m_ctx, globalObj);

  // Unhandled promise exceptions
  JS_SetHostPromiseRejectionTracker(m_runtime, promiseRejectionTracker, nullptr);
}

void JsBridgeContext::startDebugger(int /*port*/) {
  // Not supported yet
}

void JsBridgeContext::cancelDebug() {
  // Not supported yet
}

void JsBridgeContext::enableModuleLoader() {
  JS_SetModuleLoaderFunc(m_runtime, nullptr, jsModuleLoader, nullptr);
}

JValue JsBridgeContext::evaluateString(const JStringLocalRef &strCode, const JniLocalRef<jsBridgeParameter> &returnParameter,
                                       bool awaitJsPromise) const {
  JSValue v = JS_Eval(m_ctx, strCode.toUtf8Chars(), strCode.utf8Length(), "eval", JS_EVAL_TYPE_GLOBAL);
  JS_AUTORELEASE_VALUE(m_ctx, v);

  strCode.releaseChars();  // release chars now as we don't need them anymore

  if (JS_IsException(v)) {
    alog("Could not evaluate string");
    throw m_exceptionHandler->getCurrentJsException();
  }

  bool isDeferred = awaitJsPromise && JS_IsObject(v) && m_utils->hasPropertyStr(v, "then");

  if (!isDeferred && returnParameter.isNull()) {
    // No return type given: try to guess it out of the JS value
    if (JS_IsBool(v) || JS_IsNumber(v) || JS_IsString(v)) {
      // The result is a supported scalar type - return it.
      return m_javaTypeProvider.getObjectType()->toJava(v);
    }

    if (JS_IsArray(m_ctx, v)) {
      return m_javaTypeProvider.getObjectType()->toJavaArray(v);
    }

    // The result is an unsupported type, undefined, or null.
    return JValue();
  }

  auto returnType = m_javaTypeProvider.makeUniqueType(returnParameter, true /*boxed*/);

  JValue value;
  if (isDeferred && !returnType->isDeferred()) {
    value = m_javaTypeProvider.getDeferredType(returnParameter)->toJava(v);
  } else {
    value = returnType->toJava(v);
  }

  return value;
}

void JsBridgeContext::evaluateFileContent(const JStringLocalRef &strCode, const std::string &strFileName, bool asModule) const {
  const int flags = asModule ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL;
  JSValue v = JS_Eval(m_ctx, strCode.toUtf8Chars(), strCode.utf8Length(), strFileName.c_str(), flags);
  JS_AUTORELEASE_VALUE(m_ctx, v);

  strCode.releaseChars();  // release chars now as we don't need them anymore

  if (JS_IsException(v)) {
    throw m_exceptionHandler->getCurrentJsException();
  }
}

void JsBridgeContext::registerJavaObject(const std::string &strName, const JniLocalRef<jobject> &object,
                                         const JObjectArrayLocalRef &methods) {

  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JS_AUTORELEASE_VALUE(m_ctx, globalObj);

  if (m_utils->hasPropertyStr(globalObj, strName.c_str())) {
    throw std::invalid_argument("Cannot register Java object: global object called " + strName + " already exists");
  }

  JSValue javaObjectValue = JavaObject::create(this, strName.c_str(), object, methods);

  // Save the JSValue as a global property
  JS_SetPropertyStr(m_ctx, globalObj, strName.c_str(), javaObjectValue);
  // No JS_FreeValue(m_ctx, javaObjectValue) after JS_SetPropertyStr()
}

void JsBridgeContext::registerJavaLambda(const std::string &strName, const JniLocalRef<jobject> &object,
                                         const JniLocalRef<jsBridgeMethod> &method) {

  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JS_AUTORELEASE_VALUE(m_ctx, globalObj);

  if (m_utils->hasPropertyStr(globalObj, strName.c_str())) {
    throw std::invalid_argument("Cannot register Java lambda: global object called " + strName + " already exists");
  }

  JSValue javaLambdaValue = JavaObject::createLambda(this, strName.c_str(), object, method);

  JS_SetPropertyStr(m_ctx, globalObj, strName.c_str(), javaLambdaValue);
  // No JS_FreeValue(m_ctx, javaLambdaHandlerValue) after JS_SetPropertyStr()
}

void JsBridgeContext::registerJsObject(const std::string &strName,
                                       const JObjectArrayLocalRef &methods,
                                       bool check) {
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSValue jsObjectValue = JS_GetPropertyStr(m_ctx, globalObj, strName.c_str());
  JS_FreeValue(m_ctx, globalObj);

  JS_AUTORELEASE_VALUE(m_ctx, jsObjectValue);

  // Create the JavaScriptObject instance
  auto cppJsObject = new JavaScriptObject(this, strName, jsObjectValue, methods, check);  // auto-deleted

  // Wrap it inside the JS object
  m_utils->createMappedCppPtrValue(cppJsObject, jsObjectValue, strName.c_str());
}

void JsBridgeContext::registerJsLambda(const std::string &strName,
                                       const JniLocalRef<jsBridgeMethod> &method) {

  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSValue jsLambdaValue = JS_GetPropertyStr(m_ctx, globalObj, strName.c_str());
  JS_FreeValue(m_ctx, globalObj);

  JS_AUTORELEASE_VALUE(m_ctx, jsLambdaValue);

  // Create the JavaScriptObject instance
  auto cppJsLambda = new JavaScriptLambda(this, method, strName, jsLambdaValue);  // auto-deleted

  // Wrap it inside the JS object
  m_utils->createMappedCppPtrValue(cppJsLambda, jsLambdaValue, strName.c_str());
}

JValue JsBridgeContext::callJsMethod(const std::string &objectName,
                                     const JniLocalRef<jobject> &javaMethod,
                                     const JObjectArrayLocalRef &args,
                                     bool awaitJsPromise) {

  // Get the JS object
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSValue jsObjectValue = JS_GetPropertyStr(m_ctx, globalObj, objectName.c_str());
  JS_FreeValue(m_ctx, globalObj);

  JS_AUTORELEASE_VALUE(m_ctx, jsObjectValue);

  if (!JS_IsObject(jsObjectValue)) {
    throw std::invalid_argument("The JS object " + objectName + " cannot be accessed (not an object)");
  }

  // Get C++ JavaScriptObject instance
  auto cppJsObject = m_utils->getMappedCppPtrValue<JavaScriptObject>(jsObjectValue, objectName.c_str());
  if (cppJsObject == nullptr) {
    throw std::invalid_argument("Cannot access the JS object " + objectName +
                                " because it does not exist or has been deleted!");
  }

  return cppJsObject->call(jsObjectValue, javaMethod, args, awaitJsPromise);
}

JValue JsBridgeContext::callJsLambda(const std::string &strFunctionName,
                                     const JObjectArrayLocalRef &args,
                                     bool awaitJsPromise) {
  // Get the JS function
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSValue jsLambdaValue = JS_GetPropertyStr(m_ctx, globalObj, strFunctionName.c_str());
  JS_FreeValue(m_ctx, globalObj);

  JS_AUTORELEASE_VALUE(m_ctx, jsLambdaValue);

  if (!JS_IsFunction(m_ctx, jsLambdaValue)) {
    throw std::invalid_argument("The JS method " + strFunctionName + " cannot be called (not a function)");
  }

  // Get C++ JavaScriptLambda instance
  auto cppJsLambda = m_utils->getMappedCppPtrValue<JavaScriptLambda>(jsLambdaValue, strFunctionName.c_str());
  if (cppJsLambda == nullptr) {
    throw std::invalid_argument("Cannot invoke the JS function " + strFunctionName +
                                " because it does not exist or has been deleted!");
  }

  return cppJsLambda->call(this, args, awaitJsPromise);
}

void JsBridgeContext::assignJsValue(const std::string &strGlobalName, const JStringLocalRef &strCode) {
  JSValue v = JS_Eval(m_ctx, strCode.toUtf8Chars(), strCode.utf8Length(), strGlobalName.c_str(), 0);
  strCode.releaseChars();  // release chars now as we don't need them anymore

  if (JS_IsException(v)) {
    throw m_exceptionHandler->getCurrentJsException();
  }

  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JS_SetPropertyStr(m_ctx, globalObj, strGlobalName.c_str(), v);
  // No JS_FreeValue(m_ctx, v) after JS_SetPropertyStr()
  JS_FreeValue(m_ctx, globalObj);
}

void JsBridgeContext::deleteJsValue(const std::string &strGlobalName) {
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSAtom atom = JS_NewAtom(m_ctx, strGlobalName.c_str());
  JS_DeleteProperty(m_ctx, globalObj, atom, 0);
  JS_FreeAtom(m_ctx, atom);
  JS_FreeValue(m_ctx, globalObj);
}

void JsBridgeContext::copyJsValue(const std::string &strGlobalNameTo, const std::string &strGlobalNameFrom) {
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSValue valueFrom = JS_GetPropertyStr(m_ctx, globalObj, strGlobalNameFrom.c_str());
  JS_SetPropertyStr(m_ctx, globalObj, strGlobalNameTo.c_str(), valueFrom);
  JS_FreeValue(m_ctx, globalObj);
}

void JsBridgeContext::newJsFunction(const std::string &strGlobalName, const JObjectArrayLocalRef &args, const JStringLocalRef &strCode) {
  JSValue codeValue = JS_NewString(m_ctx, strCode.toUtf8Chars());
  strCode.releaseChars();  // release chars now as we don't need them anymore

  jsize argCount = args.getLength();
  JSValue functionArgValues[argCount + 1];

  for (jsize i = 0; i < argCount; ++i) {
    JStringLocalRef argString(args.getElement<jstring>(i));
    functionArgValues[i] = JS_NewString(m_ctx, argString.toUtf8Chars());
  }

  functionArgValues[argCount] = codeValue;

  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSValue functionObj = JS_GetPropertyStr(m_ctx, globalObj, "Function");
  assert(JS_IsConstructor(m_ctx, functionObj));
  JSValue functionValue = JS_CallConstructor(m_ctx, functionObj, argCount + 1, functionArgValues);
  JS_FreeValue(m_ctx, functionObj);

  for (jsize i = 0; i <= argCount; ++i) {
    JS_FreeValue(m_ctx, functionArgValues[i]);
  }

  if (JS_IsException(functionValue)) {
    throw m_exceptionHandler->getCurrentJsException();
  }

  JS_SetPropertyStr(m_ctx, globalObj, strGlobalName.c_str(), functionValue);
  // No JS_FreeValue(m_ctx, functionValue) after JS_SetPropertyStr
  JS_FreeValue(m_ctx, globalObj);
}

void JsBridgeContext::convertJavaValueToJs(const std::string &strGlobalName, const JniLocalRef<jobject> &javaValue, const JniLocalRef<jsBridgeParameter> &parameter) {

  auto type = m_javaTypeProvider.makeUniqueType(parameter, true /*boxed*/);

  JSValue value = type->fromJava(JValue(javaValue));
  if (JS_IsException(value)) {
    throw m_exceptionHandler->getCurrentJsException();
  }

  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JS_SetPropertyStr(m_ctx, globalObj, strGlobalName.c_str(), value);
  // No JS_FreeValue(m_ctx, value) after JS_SetPropertyStr
  JS_FreeValue(m_ctx, globalObj);
}

void JsBridgeContext::processPromiseQueue() {
  JSContext *ctx1;
  int err;

  // Execute the pending jobs
  while (true) {
    err = JS_ExecutePendingJob(m_runtime, &ctx1);
    if (err <= 0) {
      if (err < 0) {
        throw m_exceptionHandler->getCurrentJsException();
      }
      break;
    }
  }
}

// static
JsBridgeContext *JsBridgeContext::getInstance(JSContext *ctx) {
  //return QuickJsUtils::getCppPtrStatic<JsBridgeContext>(ctx, JSBRIDGE_CPP_CLASS_PROP_NAME);
  JSValue globalObj = JS_GetGlobalObject(ctx);
  JSValue cppWrapperObj = JS_GetPropertyStr(ctx, globalObj, JSBRIDGE_CPP_CLASS_PROP_NAME);
  JS_FreeValue(ctx, globalObj);

  auto that = QuickJsUtils::getCppPtr<JsBridgeContext>(cppWrapperObj);
  JS_FreeValue(ctx, cppWrapperObj);
  return that;
}

