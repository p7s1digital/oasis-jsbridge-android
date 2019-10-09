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

#include "JavaObject.h"
#include "JavaScriptLambda.h"
#include "JavaScriptObject.h"
#include "JavaType.h"
#include "JavaTypeProvider.h"
#include "JniCache.h"
#include "QuickJsUtils.h"
#include "java-types/Deferred.h"
#include "java-types/Object.h"
#include "custom_stringify.h"
#include "log.h"
#include "quickjs_console.h"
#include <functional>


// Internal
// ---

namespace {
  // Internal names used for properties in the QuickJS context's global stash and bound variables
  const char *JSBRIDGE_CPP_CLASS_PROP_NAME = "__jsbridge_cpp";
  const char *JAVA_EXCEPTION_PROP_NAME = "__java_exception";

  //int interrupt_handler(JSRuntime *rt, void *opaque) {
  //  return 0;
  //}
}


// Class methods
// ---

JsBridgeContext::JsBridgeContext()
 : m_javaTypeProvider(this) {
}

JsBridgeContext::~JsBridgeContext() {
  JS_FreeContext(m_ctx);
  JS_FreeRuntime(m_runtime);
}

void JsBridgeContext::init(JniContext *jniContext, const JniLocalRef<jobject> &jsBridgeObject) {
  m_jniContext = jniContext;

  m_runtime = JS_NewRuntime();

  //JS_SetInterruptHandler(rt, interrupt_handler, NULL)

  m_ctx = JS_NewContext(m_runtime);
  JS_SetMaxStackSize(m_ctx, 1 * 1024 * 1024);  // default: 256kb, now: 1MB

  m_jniCache = new JniCache(this, jsBridgeObject);
  m_utils = new QuickJsUtils(jniContext, m_ctx);

  // Store the JsBridgeContext instance in the global object so we can find our way back from a C callback
  JSValue cppWrapperObj = m_utils->createCppPtrValue(this, false);
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JS_SetPropertyStr(m_ctx, globalObj, JSBRIDGE_CPP_CLASS_PROP_NAME, cppWrapperObj);
  // No JS_FreeValue(m_ctx, cppWrapperObj) after JS_SetPropertyStr()
  JS_FreeValue(m_ctx, globalObj);

  // Set global + window (TODO)
  // See also https://wiki.duktape.org/howtoglobalobjectreference
  static const char *str1 = "var global = this; var window = this; window.open = function() {};\n";
  JSValue e = JS_Eval(m_ctx, str1, strlen(str1), "JsBridgeContext.cpp", 0);
  JS_FreeValue(m_ctx, e);

  quickjs_console_init(m_ctx);
}

void JsBridgeContext::initDebugger() {
  // Not supported yet
}

void JsBridgeContext::cancelDebug() {
  // Not supported yet
}

JValue JsBridgeContext::evaluateString(const JStringLocalRef &strCode, const JniLocalRef<jsBridgeParameter> &returnParameter,
                                       bool awaitJsPromise) const {
  JSValue v = JS_Eval(m_ctx, strCode.toUtf8Chars(), strCode.utf8Length(), "JsBridgeContext/evaluateString", 0);
  strCode.releaseChars();  // release chars now as we don't need them anymore

  if (JS_IsException(v)) {
    alog("Could not evaluate string");
    JS_FreeValue(m_ctx, v);
    queueJavaExceptionForJsError();
    return JValue();
  }

  bool isDeferred = awaitJsPromise && JS_IsObject(v) && m_utils->hasPropertyStr(v, "then");

  if (!isDeferred && returnParameter.isNull()) {
    // No return type given: try to guess it out of the JS value
    if (JS_IsBool(v) || JS_IsNumber(v) || JS_IsString(v)) {
      // The result is a supported scalar type - return it.
      JValue ret = m_javaTypeProvider.getObjectType()->toJava(v, false);
      JS_FreeValue(m_ctx, v);
      return ret;
    }

    if (JS_IsArray(m_ctx, v)) {
      JValue ret = m_javaTypeProvider.getObjectType()->toJavaArray(v, false);
      JS_FreeValue(m_ctx, v);
      return ret;
    }

    // The result is an unsupported type, undefined, or null.
    JS_FreeValue(m_ctx, v);
    return JValue();
  }

  auto returnType = m_javaTypeProvider.makeUniqueType(returnParameter, true /*boxed*/);

  JValue value;
  if (isDeferred && !returnType->isDeferred()) {
    value = m_javaTypeProvider.getDeferredType(returnParameter)->toJava(v, false /*inScript*/);
  } else {
    value = returnType->toJava(v, false);
  }

  JS_FreeValue(m_ctx, v);
  return value;
}

void JsBridgeContext::evaluateFileContent(const JStringLocalRef &strCode, const std::string &strFileName) const {
  JSValue v = JS_Eval(m_ctx, strCode.toUtf8Chars(), strCode.utf8Length(), strFileName.c_str(), 0);
  strCode.releaseChars();  // release chars now as we don't need them anymore

  JS_FreeValue(m_ctx, v);

  if (JS_IsException(v)) {
    queueJavaExceptionForJsError();
  }
}

void JsBridgeContext::registerJavaObject(const std::string &strName, const JniLocalRef<jobject> &object,
                                         const JObjectArrayLocalRef &methods) {

  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  if (m_utils->hasPropertyStr(globalObj, strName.c_str())) {
    queueIllegalArgumentException("Cannot register Java object: global object called " + strName + " already exists");
    JS_FreeValue(m_ctx, globalObj);
    return;
  }

  JSValue javaObjectValue = JavaObject::create(this, strName.c_str(), object, methods);
  if (JS_IsUndefined(javaObjectValue)) {
    JS_FreeValue(m_ctx, globalObj);
    return;
  }

  // Save the JSValue as a global property
  JS_SetPropertyStr(m_ctx, globalObj, strName.c_str(), javaObjectValue);
  // No JS_FreeValue(m_ctx, javaObjectValue) after JS_SetPropertyStr()

  JS_FreeValue(m_ctx, globalObj);
}

void JsBridgeContext::registerJavaLambda(const std::string &strName, const JniLocalRef<jobject> &object,
                                         const JniLocalRef<jsBridgeMethod> &method) {

  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  if (m_utils->hasPropertyStr(globalObj, strName.c_str())) {
    queueIllegalArgumentException("Cannot register Java lambda: global object called " + strName + " already exists");
    JS_FreeValue(m_ctx, globalObj);
    return;
  }

  JSValue javaLambdaValue = JavaObject::createLambda(this, strName.c_str(), object, method);
  if (JS_IsUndefined(javaLambdaValue)) {
    JS_FreeValue(m_ctx, globalObj);
    return;
  }

  JS_SetPropertyStr(m_ctx, globalObj, strName.c_str(), javaLambdaValue);
  // No JS_FreeValue(m_ctx, javaLambdaHandlerValue) after JS_SetPropertyStr()

  JS_FreeValue(m_ctx, globalObj);
}

void JsBridgeContext::registerJsObject(const std::string &strName,
                                       const JObjectArrayLocalRef &methods) {
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSValue jsObjectValue = JS_GetPropertyStr(m_ctx, globalObj, strName.c_str());
  JS_FreeValue(m_ctx, globalObj);

  if (!JS_IsObject(jsObjectValue) || JS_IsNull(jsObjectValue)) {
    JS_FreeValue(m_ctx, jsObjectValue);
    throw std::invalid_argument("Cannot register " + strName + ". It does not exist or is not a valid object.");
  }

  // Check that it is not a promise!
  if (m_utils->hasPropertyStr(jsObjectValue, "then")) {
    alog_warn("Attempting to register a JS promise (%s)... JsValue.await() should probably be called, first...");
  }

  // Create the JavaScriptObject instance
  auto cppJsObject = new JavaScriptObject(this, strName, jsObjectValue, methods);  // auto-deleted

  // Wrap it inside the JS object
  m_utils->createMappedCppPtrValue(cppJsObject, jsObjectValue, strName.c_str());

  JS_FreeValue(m_ctx, jsObjectValue);
}

void JsBridgeContext::registerJsLambda(const std::string &strName,
                                       const JniLocalRef<jsBridgeMethod> &method) {
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSValue jsLambdaValue = JS_GetPropertyStr(m_ctx, globalObj, strName.c_str());
  JS_FreeValue(m_ctx, globalObj);

  if (!JS_IsFunction(m_ctx, jsLambdaValue)) {
    JS_FreeValue(m_ctx, jsLambdaValue);
    throw std::invalid_argument("Cannot register " + strName + ". It does not exist or is not a valid function.");
  }

  // Create the JavaScriptObject instance
  auto cppJsLambda = new JavaScriptLambda(this, method, strName, jsLambdaValue);  // auto-deleted

  // Wrap it inside the JS object
  m_utils->createMappedCppPtrValue(cppJsLambda, jsLambdaValue, strName.c_str());

  JS_FreeValue(m_ctx, jsLambdaValue);
}

JValue JsBridgeContext::callJsMethod(const std::string &objectName,
                                     const JniLocalRef<jobject> &javaMethod,
                                     const JObjectArrayLocalRef &args) {

  // Get the JS object
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSValue jsObjectValue = JS_GetPropertyStr(m_ctx, globalObj, objectName.c_str());
  JS_FreeValue(m_ctx, globalObj);

  if (!JS_IsObject(jsObjectValue)) {
    throw std::invalid_argument("The JS object " + objectName + " cannot be accessed (not an object)");
  }

  // Get C++ JavaScriptObject instance
  auto cppJsObject = m_utils->getMappedCppPtrValue<JavaScriptObject>(jsObjectValue, objectName.c_str());
  if (cppJsObject == nullptr) {
    throw std::invalid_argument("Cannot access the JS object " + objectName +
                                " because it does not exist or has been deleted!");
  }

  JValue ret = cppJsObject->call(jsObjectValue, javaMethod, args);
  JS_FreeValue(m_ctx, jsObjectValue);

  return std::move(ret);
}

JValue JsBridgeContext::callJsLambda(const std::string &strFunctionName,
                                     const JObjectArrayLocalRef &args,
                                     bool awaitJsPromise) {
  // Get the JS function
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSValue jsLambdaValue = JS_GetPropertyStr(m_ctx, globalObj, strFunctionName.c_str());
  JS_FreeValue(m_ctx, globalObj);

  if (!JS_IsFunction(m_ctx, jsLambdaValue)) {
    JS_FreeValue(m_ctx, jsLambdaValue);
    throw std::invalid_argument("The JS method " + strFunctionName + " cannot be called (not a function)");
  }

  // Get C++ JavaScriptLambda instance
  auto cppJsLambda = m_utils->getMappedCppPtrValue<JavaScriptLambda>(jsLambdaValue, strFunctionName.c_str());
  JS_FreeValue(m_ctx, jsLambdaValue);
  if (cppJsLambda == nullptr) {
    queueJsException("Cannot invoke the JS function " + strFunctionName +
                     " because it does not exist or has been deleted!");
    return JValue();
  }

  return cppJsLambda->call(this, args, awaitJsPromise);
}

void JsBridgeContext::assignJsValue(const std::string &strGlobalName, const JStringLocalRef &strCode) {

  JSValue v = JS_Eval(m_ctx, strCode.toUtf8Chars(), strCode.utf8Length(), strGlobalName.c_str(), 0);
  strCode.releaseChars();  // release chars now as we don't need them anymore

  if (JS_IsException(v)) {
    JS_FreeValue(m_ctx, v);
    queueJavaExceptionForJsError();
    return;
  }

  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JS_SetPropertyStr(m_ctx, globalObj, strGlobalName.c_str(), v);
  // No JS_FreeValue(m_ctx, v) after JS_SetPropertyStr()
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

  JS_SetPropertyStr(m_ctx, globalObj, strGlobalName.c_str(), functionValue);
  // No JS_FreeValue(m_ctx, functionValue) after JS_SetPropertyStr
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
        queueJavaExceptionForJsError();
      }
      break;
    }
  }
}

void JsBridgeContext::throwTypeException(const std::string &message, bool inScript) const {
  if (inScript) {
    JS_ThrowTypeError(m_ctx, "%s", message.c_str());
  } else {
    throw std::invalid_argument(message);
  }
}

void JsBridgeContext::queueIllegalArgumentException(const std::string &message) const {
  m_jniContext->throwNew(m_jniCache->getIllegalArgumentExceptionClass(), message.c_str());
}

void JsBridgeContext::queueJsException(const std::string &message) const {
  auto exception = m_jniCache->newJsException(
      JStringLocalRef(),  // jsonValue
      JStringLocalRef(m_jniContext, message.c_str()),  // detailedMessage
      JStringLocalRef(),  // jsStackTrace
      JniLocalRef<jthrowable>()  // cause
  );
  m_jniContext->throw_(exception);
}

//void JsBridgeContext::queueNullPointerException(const std::string &message) const {
//}

bool JsBridgeContext::hasPendingJniException() const {
  return m_jniContext->exceptionCheck();
}

void JsBridgeContext::rethrowJniException() const {
  if (!m_jniContext->exceptionCheck()) {
    return;
  }

  // Get (and clear) the Java exception and read its message
  JniLocalRef<jthrowable> exception(m_jniContext, m_jniContext->exceptionOccurred(), JniLocalRefMode::Borrowed);
  m_jniContext->exceptionClear();

  auto exceptionClass = m_jniContext->getObjectClass(exception);
  jmethodID getMessage = m_jniContext->getMethodID(exceptionClass, "getMessage", "()Ljava/lang/String;");
  JStringLocalRef message = m_jniContext->callStringMethod(exception, getMessage);


  // Propagate Java exception to JavaScript (and store pointer to Java exception)
  // ---

  JSValue errorValue = JS_NewError(m_ctx);
  JSValue messageValue = JS_NewString(m_ctx, message.toUtf8Chars());
  JS_SetPropertyStr(m_ctx, errorValue, "message", messageValue);
  // No JS_FreeValue(m_ctx, messageValue) after JS_SetPropertyStr()

  JSValue javaExceptionValue = m_utils->createJavaRefValue(exception);
  JS_SetPropertyStr(m_ctx, errorValue, JAVA_EXCEPTION_PROP_NAME, javaExceptionValue);
  // No JS_FreeValue(m_ctx, javaExceptionValue) after JS_SetPropertyStr()

  JS_Throw(m_ctx, errorValue);
  // No JS_FreeValue(m_ctx, errorValue) after JS_Throw()
}

void JsBridgeContext::queueJavaExceptionForJsError() const {
  m_jniContext->throw_(getJavaExceptionForJsError());
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


// Private methods
// ---

JniLocalRef<jthrowable> JsBridgeContext::getJavaExceptionForJsError() const {
  JniLocalRef<jthrowable> ret;

  JSValue exceptionValue = JS_GetException(m_ctx);
  JSValue jsonValue = custom_stringify(m_ctx, exceptionValue);
  JStringLocalRef jsonString = m_utils->toJString(jsonValue);

  // Is there an exception thrown from a Java method?
  JniLocalRef<jthrowable> cause;
  JSValue javaExceptionValue = JS_GetPropertyStr(m_ctx, exceptionValue, JAVA_EXCEPTION_PROP_NAME);
  if (!JS_IsUndefined(javaExceptionValue)) {
    cause = m_utils->getJavaRef<jthrowable>(javaExceptionValue);
  }
  JS_FreeValue(m_ctx, javaExceptionValue);

  if (JS_IsError(m_ctx, exceptionValue)) {
    // Error message
    std::string msg = m_utils->toString(JS_GetPropertyStr(m_ctx, exceptionValue, "message"));
    std::string stack;

    // Get the stack trace
    JSValue stackValue = JS_GetPropertyStr(m_ctx, exceptionValue, "stack");
    if (!JS_IsUndefined(stackValue)) {
      stack = m_utils->toString(stackValue);
    }
    JS_FreeValue(m_ctx, stackValue);

    ret = m_jniCache->newJsException(
        jsonString,  // jsonValue
        JStringLocalRef(m_jniContext, msg.c_str()),  // detailedMessage
        JStringLocalRef(m_jniContext, stack.c_str()),  // jsStackTrace
        cause
    );
  }

  if (ret.isNull()) {
    // Not an error or no stacktrace, just convert to a string.
    JStringLocalRef strThrownValue = m_utils->toJString(exceptionValue);

    ret = m_jniCache->newJsException(
        jsonString,  // jsonValue
        strThrownValue,  // detailedMessage
        JStringLocalRef(),  // jsStackTrace
        cause
    );
  }

  JS_FreeValue(m_ctx, jsonValue);
  JS_FreeValue(m_ctx, exceptionValue);
  return ret;
}

