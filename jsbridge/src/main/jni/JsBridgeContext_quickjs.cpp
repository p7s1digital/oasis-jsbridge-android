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

#include "ArgumentLoader.h"
#include "JavaObject.h"
#include "JavaScriptLambda.h"
#include "JavaScriptObject.h"
#include "JavaType.h"
#include "QuickJsUtils.h"
#include "quickjs_console.h"
#include "log.h"
#include "custom_stringify.h"
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
 : m_javaTypes() {
}

JsBridgeContext::~JsBridgeContext() {
  JS_FreeContext(m_ctx);
  JS_FreeRuntime(m_runtime);
}

void JsBridgeContext::init(JniContext *jniContext) {
  m_currentJniContext = jniContext;

  m_runtime = JS_NewRuntime();

  //JS_SetInterruptHandler(rt, interrupt_handler, NULL)

  m_ctx = JS_NewContext(m_runtime);
  JS_SetMaxStackSize(m_ctx, 1 * 1024 * 1024);  // default: 256kb, now: 1MB

  m_utils = new QuickJsUtils(jniContext, m_ctx);

  // Store the JsBridgeContext instance in the global object so we can find our way back from a C callback
  JSValue cppWrapperObj = m_utils->createCppPtrValue(this, false);
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JS_SetPropertyStr(m_ctx, globalObj, JSBRIDGE_CPP_CLASS_PROP_NAME, cppWrapperObj);
  // No JS_FreeValue(m_ctx, cppWrapperObj) after JS_SetPropertyStr()
  JS_FreeValue(m_ctx, globalObj);

  m_objectType = m_javaTypes.getObjectType(this);

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

JValue JsBridgeContext::evaluateString(const std::string &strCode, const JniLocalRef<jsBridgeParameter> &returnParameter,
                                      bool awaitJsPromise) const {

  JSValue v = JS_Eval(m_ctx, strCode.c_str(), strCode.size(), "JsBridgeContext/evaluateString", 0);

  if (JS_IsException(v)) {
    JS_FreeValue(m_ctx, v);
    queueJavaExceptionForJsError();
    return JValue();
  }

  bool isDeferred = awaitJsPromise && JS_IsObject(v) && m_utils->hasPropertyStr(v, "then");

  if (!isDeferred && returnParameter.isNull()) {
    // No return type given: try to guess it out of the JS value
    if (JS_IsBool(v) || JS_IsNumber(v) || JS_IsString(v)) {
      // The result is a supported scalar type - return it.
      JValue ret = m_objectType->toJava(v, false, nullptr);
      JS_FreeValue(m_ctx, v);
      return ret;
    }

    if (JS_IsArray(m_ctx, v)) {
      JValue ret = m_objectType->toJavaArray(v, false, nullptr);
      JS_FreeValue(m_ctx, v);
      return ret;
    }

    // The result is an unsupported type, undefined, or null.
    JS_FreeValue(m_ctx, v);
    return JValue();
  }

  const JniRef<jclass> &jsBridgeParameterClass = jniContext()->getJsBridgeParameterClass();
  jmethodID getParameterClass = jniContext()->getMethodID(jsBridgeParameterClass, "getJava", "()Ljava/lang/Class;");
  JniLocalRef<jclass> returnClass = jniContext()->callObjectMethod<jclass>(returnParameter, getParameterClass);

  const JavaType *returnType = m_javaTypes.get(this, returnClass, true /*boxed*/);

  ArgumentLoader argumentLoader(returnType, returnParameter, false);

  JValue value;

  if (isDeferred && !returnType->isDeferred()) {
    value = argumentLoader.toJavaDeferred(v, &m_javaTypes);
  } else {
    value = argumentLoader.toJava(v);
  }

  JS_FreeValue(m_ctx, v);
  return value;
}

void JsBridgeContext::evaluateFileContent(const std::string &strCode, const std::string &strFileName) const {
  JSValue v = JS_Eval(m_ctx, strCode.c_str(), strCode.size(), strFileName.c_str(), 0);

  if (JS_IsException(v)) {
    JS_FreeValue(m_ctx, v);
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

  // Create the JavaScriptObject instance (which takes over jsObjectValue and will free it in its destructor)
  auto cppJsObject = new JavaScriptObject(this, strName, jsObjectValue, methods);  // auto-deleted

  // Wrap it inside the JS object
  m_utils->createMappedCppPtrValue(cppJsObject, jsObjectValue, strName.c_str());
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

  // Create the JavaScriptObject instance (which takes over jsLambdaValue and will free it in its destructor)
  auto cppJsLambda = new JavaScriptLambda(this, method, strName, jsLambdaValue);  // auto-deleted

  // Wrap it inside the JS object
  m_utils->createMappedCppPtrValue(cppJsLambda, jsLambdaValue, strName.c_str());
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

  JS_FreeValue(m_ctx, jsObjectValue);

  return cppJsObject->call(javaMethod, args);
}

JValue JsBridgeContext::callJsLambda(const std::string &strFunctionName,
                                     const JObjectArrayLocalRef &args,
                                     bool awaitJsPromise) {
  // Get the JS function
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSValue jsLambdaValue = JS_GetPropertyStr(m_ctx, globalObj, strFunctionName.c_str());
  JS_FreeValue(m_ctx, globalObj);

  if (!JS_IsFunction(m_ctx, jsLambdaValue)) {
    throw std::invalid_argument("The JS method " + strFunctionName + " cannot be called (not a function)");
  }

  // Get C++ JavaScriptLambda instance
  auto cppJsLambda = m_utils->getMappedCppPtrValue<JavaScriptLambda>(jsLambdaValue, strFunctionName.c_str());
  if (cppJsLambda == nullptr) {
    queueJsException("Cannot invoke the JS function " + strFunctionName +
                     " because it does not exist or has been deleted!");
    return JValue();
  }

  return cppJsLambda->call(this, args, awaitJsPromise);
}

void JsBridgeContext::assignJsValue(const std::string &strGlobalName, const std::string &strCode) {

  JSValue v = JS_Eval(m_ctx, strCode.c_str(), strCode.size(), strGlobalName.c_str(), 0);

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

void JsBridgeContext::newJsFunction(const std::string &strGlobalName, const JObjectArrayLocalRef &args, const std::string &strCode) {
  JSValue codeValue = JS_NewString(m_ctx, strCode.c_str());

  jsize argCount = args.getLength();
  JSValue functionArgValues[argCount + 1];

  for (jsize i = 0; i < argCount; ++i) {
    JStringLocalRef argString(args.getElement<jstring>(i));
    functionArgValues[i] = JS_NewString(m_ctx, argString.c_str());
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

void JsBridgeContext::completeJsPromise(const std::string &strId, bool isFulfilled, const JniLocalRef<jobject> &value, const JniLocalRef<jclass> &valueClass) {
  // Get the global PromiseObject
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSValue promiseObj = JS_GetPropertyStr(m_ctx, globalObj, strId.c_str());
  JS_FreeValue(m_ctx, globalObj);
  if (!JS_IsObject(promiseObj)) {
    alog("Could not find PromiseObject with id %s", strId.c_str());
    return;
  }

  // Get the resolve/reject function
  const char *resolveOrRejectStr = isFulfilled ? "resolve" : "reject";
  JSValue resolveOrReject = JS_GetPropertyStr(m_ctx, promiseObj, resolveOrRejectStr);
  if (JS_IsFunction(m_ctx, resolveOrReject)) {
    // Call it with the Promise value
    JSValue promiseParam = m_javaTypes.get(this, valueClass)->fromJava(JValue(value), false /*inScript*/, nullptr);
    JSValue ret = JS_Call(m_ctx, resolveOrReject, promiseObj, 1, &promiseParam);
    if (JS_IsException(ret)) {
      alog("Could not complete Promise with id %s", strId.c_str());
    }

    JS_FreeValue(m_ctx, ret);
    JS_FreeValue(m_ctx, promiseParam);
  } else {
    alog("Could not complete Promise with id %s: cannot find %s", strId.c_str(), resolveOrRejectStr);
  }

  JS_FreeValue(m_ctx, resolveOrReject);
  JS_FreeValue(m_ctx, promiseObj);
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

void JsBridgeContext::queueIllegalArgumentException(const std::string &message) const {
  JniLocalRef<jclass> illegalArgumentException = jniContext()->findClass("java/lang/IllegalArgumentException");
  jniContext()->throwNew(illegalArgumentException, message.c_str());
}

void JsBridgeContext::queueJsException(const std::string &message) const {
  JniLocalRef<jclass> exceptionClass = jniContext()->findClass("de/prosiebensat1digital/oasisjsbridge/JsException");

  jmethodID newException = jniContext()->getMethodID(
      exceptionClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)V");

  auto exception = jniContext()->newObject<jthrowable>(
      exceptionClass,
      newException,
      JStringLocalRef(),  // jsonValue
      JStringLocalRef(jniContext(), message.c_str()),  // detailedMessage
      JStringLocalRef(),  // jsStackTrace
      JniLocalRef<jthrowable>()
  );
  jniContext()->throw_(exception);
}

//void JsBridgeContext::queueNullPointerException(const std::string &message) const {
//}

// TODO: rename and document it!
void JsBridgeContext::checkRethrowJsError() const {
  if (!jniContext()->exceptionCheck()) {
    return;
  }

  // Get (and clear) the Java exception and read its message
  auto exception = JniLocalRef<jthrowable>(jniContext(), jniContext()->exceptionOccurred(), true);
  jniContext()->exceptionClear();
  auto exceptionClass = jniContext()->getObjectClass(exception);
  jmethodID getMessage = jniContext()->getMethodID(exceptionClass, "getMessage","()Ljava/lang/String;");
  JStringLocalRef message(jniContext()->callObjectMethod<jstring>(exception, getMessage));


  // Propagate Java exception to JavaScript (and store pointer to Java exception)
  // ---

  JSValue errorValue = JS_NewError(m_ctx);
  JSValue messageValue = JS_NewString(m_ctx, message.c_str());
  JS_SetPropertyStr(m_ctx, errorValue, "message", messageValue);
  // No JS_FreeValue(m_ctx, messageValue) after JS_SetPropertyStr()

  JSValue javaExceptionValue = m_utils->createJavaRefValue(exception);
  JS_SetPropertyStr(m_ctx, errorValue, JAVA_EXCEPTION_PROP_NAME, javaExceptionValue);
  // No JS_FreeValue(m_ctx, javaExceptionValue) after JS_SetPropertyStr()

  JS_Throw(m_ctx, errorValue);
  // No JS_FreeValue(m_ctx, errorValue) after JS_Throw()
}

void JsBridgeContext::queueJavaExceptionForJsError() const {
  jniContext()->throw_(getJavaExceptionForJsError());
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

  JniLocalRef<jclass> exceptionClass = jniContext()->findClass("de/prosiebensat1digital/oasisjsbridge/JsException");
  jmethodID newException = jniContext()->getMethodID(
      exceptionClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)V");

  JSValue exceptionValue = JS_GetException(m_ctx);
  JSValue jsonValue = custom_stringify(m_ctx, exceptionValue);
  JStringLocalRef jsonString = m_utils->toJString(jsonValue);

  // Is there an exception thrown from a Java method?
  JniLocalRef<jthrowable> cause;
  JSValue javaExceptionValue = JS_GetPropertyStr(m_ctx, exceptionValue, JAVA_EXCEPTION_PROP_NAME);
  if (!JS_IsUndefined(javaExceptionValue)) {
    cause = JniLocalRef<jthrowable>(jniContext(), m_utils->getJavaRef<jthrowable>(javaExceptionValue).toNewRawGlobalRef());
  }

  //JS_FreeValue(m_ctx, javaExceptionValue);
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

    ret = jniContext()->newObject<jthrowable>(
        exceptionClass,
        newException,
        jsonString,  // jsonValue
        JStringLocalRef(jniContext(), msg.c_str()),  // detailedMessage
        JStringLocalRef(jniContext(), stack.c_str()),  // jsStackTrace
        cause
    );
  }

  if (ret.isNull()) {
    // Not an error or no stacktrace, just convert to a string.
    JStringLocalRef strThrownValue = m_utils->toJString(exceptionValue);

    ret = jniContext()->newObject<jthrowable>(
        exceptionClass,
        newException,
        jsonString,  // jsonValue
        strThrownValue,  // detailedMessage
        JStringLocalRef(),  // jsStackTrace
        cause
    );
  }

  JS_FreeValue(m_ctx, jsonValue);
  //JS_FreeValue(m_ctx, exceptionValue);
  return ret;
}
