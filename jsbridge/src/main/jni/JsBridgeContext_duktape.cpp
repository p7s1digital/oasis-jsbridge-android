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
#include "JsBridgeContext.h"

#include "DuktapeUtils.h"
#include "JavaObject.h"
#include "JavaScriptLambda.h"
#include "JavaScriptObject.h"
#include "StackChecker.h"
#include "custom_stringify.h"
#include "log.h"
#include "duk_console.h"
#include "java-types/Deferred.h"
#include "jni-helpers/JniGlobalRef.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include "jni-helpers/JStringLocalRef.h"
#include "duktape/duk_trans_socket.h"
#include <functional>
#include <memory>
#include <string>
#include <stdexcept>


// Internal
// ---

namespace {
  // The \xff\xff part keeps the variable hidden from JavaScript (visible through C API only).
  const char *JSBRIDGE_CPP_CLASS_PROP_NAME = "\xff\xffjsbridge_cpp";
  const char* JAVA_EXCEPTION_PROP_NAME = "\xff\xffjava_exception";

  void debugger_detached(duk_context */*ctx*/, void *udata) {
      alog_info("Debugger detached, udata: %p\n", udata);
  }

  // Native functions called from JS
  // ---
  extern "C" {
    void fatalErrorHandler(void *, const char* msg) {
      alog_fatal("Fatal error: %s", msg);
      throw std::runtime_error(msg);
    }
  }  // extern "C"
} // anonymous namespace


// Class methods
// ---

JsBridgeContext::JsBridgeContext()
 : m_javaTypeProvider(this) {
}

JsBridgeContext::~JsBridgeContext() {
  // Delete the proxies before destroying the heap.
  duk_destroy_heap(m_ctx);
}

void JsBridgeContext::init(JniContext *jniContext) {

  m_currentJniContext = jniContext;

  m_ctx = duk_create_heap(nullptr, nullptr, nullptr, nullptr, fatalErrorHandler);

  if (!m_ctx) {
    throw std::bad_alloc();
  }

  m_utils = new DuktapeUtils(jniContext, m_ctx);

  // Stash the JsBridgeContext instance in the context, so we can find our way back from a Duktape C callback.
  duk_push_global_stash(m_ctx);
  duk_push_pointer(m_ctx, this);
  duk_put_prop_string(m_ctx, -2, JSBRIDGE_CPP_CLASS_PROP_NAME);
  duk_pop(m_ctx);

  // Set global + window (TODO)
  // See also https://wiki.duktape.org/howtoglobalobjectreference
  static const char *str1 = "var global = this; var window = this; window.open = function() {};\n";
  duk_eval_string_noresult(m_ctx, str1);

  // Console
  duk_console_init(m_ctx, 0 /*flags*/);
}

void JsBridgeContext::initDebugger() {

  // Call Java onDebuggerPending()
  jniContext()->callJsBridgeVoidMethod("onDebuggerPending", "()V");

  alog_info("Debugger enabled, create socket and wait for connection\n");
  duk_trans_socket_init();
  duk_trans_socket_waitconn();
  alog_info("Debugger connected, call duk_debugger_attach() and then execute requested file(s)/eval\n");

  // Call Java onDebuggerReady()
  jniContext()->callJsBridgeVoidMethod("onDebuggerReady", "()V");

  duk_debugger_attach(
      m_ctx,
      duk_trans_socket_read_cb,
      duk_trans_socket_write_cb,
      duk_trans_socket_peek_cb,
      duk_trans_socket_read_flush_cb,
      duk_trans_socket_write_flush_cb,
      nullptr,
      debugger_detached,
      (void *) m_ctx
  );
}

void JsBridgeContext::cancelDebug() {
    alog_info("Cancelling Duktape debug...");
    duk_trans_socket_finish();
}

JValue JsBridgeContext::evaluateString(const std::string &strCode, const JniLocalRef<jsBridgeParameter> &returnParameter,
                                      bool awaitJsPromise) const {
  CHECK_STACK(m_ctx);

  JNIEnv *env = jniContext()->getJNIEnv();
  assert(env != nullptr);

  if (duk_peval_string(m_ctx, strCode.c_str()) != DUK_EXEC_SUCCESS) {
    alog("Could not evaluate string:\n%s", strCode.c_str());
    queueJavaExceptionForJsError();
    duk_pop(m_ctx);
    return JValue();
  }

  bool isDeferred = awaitJsPromise && duk_is_object(m_ctx, -1) && duk_has_prop_string(m_ctx, -1, "then");
  if (!isDeferred && returnParameter.isNull()) {
    // No return type given: try to guess it out of the JS value
    const int supportedTypeMask = DUK_TYPE_MASK_BOOLEAN | DUK_TYPE_MASK_NUMBER | DUK_TYPE_MASK_STRING;

    if (duk_check_type_mask(m_ctx, -1, supportedTypeMask)) {
      // The result is a supported scalar type - return it.
      return m_javaTypeProvider.getObjectType()->pop(false);
    }

    if (duk_is_array(m_ctx, -1)) {
      return m_javaTypeProvider.getObjectType()->popArray(1, false /*expand*/, false /*inScript*/);
    }

    // The result is an unsupported type, undefined, or null.
    duk_pop(m_ctx);
    return JValue();
  }

  auto returnType = m_javaTypeProvider.makeUniqueType(returnParameter, true /*boxed*/);

  if (isDeferred && !returnType->isDeferred()) {
    return m_javaTypeProvider.getDeferredType(returnParameter)->pop(false /*inScript*/);
  }

  return returnType->pop(false /*inScript*/);
}

void JsBridgeContext::evaluateFileContent(const std::string &strCode, const std::string &strFileName) const {

  CHECK_STACK(m_ctx);

  duk_push_string(m_ctx, strFileName.c_str());

  if (duk_pcompile_string_filename(m_ctx, DUK_COMPILE_EVAL, strCode.c_str()) == DUK_EXEC_SUCCESS) {
    if (duk_pcall(m_ctx, 0) != DUK_EXEC_SUCCESS) {
      alog("Could not execute file %s", strFileName.c_str());
      queueJavaExceptionForJsError();
      return;
    }
    duk_pop(m_ctx);
  } else {
    alog("Could not compile file %s", strFileName.c_str());
    queueJavaExceptionForJsError();
  }
}

void JsBridgeContext::registerJavaObject(const std::string &strName, const JniLocalRef<jobject> &object,
                                        const JObjectArrayLocalRef &methods) {
  CHECK_STACK(m_ctx);

  duk_push_global_object(m_ctx);

  if (duk_has_prop_string(m_ctx, -1, strName.c_str())) {
    duk_pop(m_ctx);
    queueIllegalArgumentException("A global object called " + strName + " already exists");
    return;
  }

  JavaObject::push(this, strName, object, methods);

  // Make our bound Java object a property of the Duktape global object (so it's a JS global).
  duk_put_prop_string(m_ctx, -2, strName.c_str());

  // Pop the Duktape global object off the stack.
  duk_pop(m_ctx);
}

void JsBridgeContext::registerJavaLambda(const std::string &strName, const JniLocalRef<jobject> &object,
                                         const JniLocalRef<jsBridgeMethod> &method) {

  CHECK_STACK(m_ctx);

  duk_push_global_object(m_ctx);

  if (duk_has_prop_string(m_ctx, -1, strName.c_str())) {
    duk_pop(m_ctx);
    queueIllegalArgumentException("A global object called " + strName + " already exists");
    return;
  }

  JavaObject::pushLambda(this, strName, object, method);

  // Make our Java lambda a property of the Duktape global object (so it's a JS global).
  duk_put_prop_string(m_ctx, -2, strName.c_str());

  // Pop the Duktape global object off the stack.
  duk_pop(m_ctx);
}

void JsBridgeContext::registerJsObject(const std::string &strName,
                                       const JObjectArrayLocalRef &methods) {
  CHECK_STACK(m_ctx);

  duk_get_global_string(m_ctx, strName.c_str());

  if (!duk_is_object(m_ctx, -1) || duk_is_null(m_ctx, -1)) {
    duk_pop(m_ctx);
    throw std::invalid_argument("Cannot register " + strName + ". It does not exist or is not a valid object");
  }

  // Check that it is not a promise!
  if (duk_is_object(m_ctx, -1) && duk_has_prop_string(m_ctx, -1, "then")) {
    alog_warn("Attempting to register a JS promise (%s)... JsValue.await() should probably be called, first...");
  }

  // Create the JavaScriptObject instance (which takes over jsObjectValue and will free it in its destructor)
  auto cppJsObject = new JavaScriptObject(this, strName, -1, methods);  // auto-deleted

  // Wrap it inside the JS object
  m_utils->createMappedCppPtrValue(cppJsObject, -1, strName.c_str());

  duk_pop(m_ctx);  // JS object
}

void JsBridgeContext::registerJsLambda(const std::string &strName,
                                       const JniLocalRef<jsBridgeMethod> &method) {
  CHECK_STACK(m_ctx);

  duk_get_global_string(m_ctx, strName.c_str());

  if (!duk_is_function(m_ctx, -1)) {
    duk_pop(m_ctx);
    throw std::invalid_argument("Cannot register " + strName + ". It does not exist or is not a valid function.");
  }

  // Create the JavaScriptObject instance
  auto cppJsLambda = new JavaScriptLambda(this, method, strName, -1);  // auto-deleted

  // Wrap it inside the JS object
  m_utils->createMappedCppPtrValue(cppJsLambda, -1, strName.c_str());

  duk_pop(m_ctx);  // JS lambda
}

JValue JsBridgeContext::callJsMethod(const std::string &objectName,
                                     const JniLocalRef<jobject> &javaMethod,
                                     const JObjectArrayLocalRef &args) {
  CHECK_STACK(m_ctx);

  // Get the JS object
  duk_get_global_string(m_ctx, objectName.c_str());
  if (!duk_is_object(m_ctx, -1) || duk_is_null(m_ctx, -1)) {
    duk_pop(m_ctx);
    throw std::invalid_argument("The JS object " + objectName + " cannot be accessed (not an object)");
  }

  // Get C++ JavaScriptObject instance
  auto cppJsObject = m_utils->getMappedCppPtrValue<JavaScriptObject>(-1, objectName.c_str());
  if (cppJsObject == nullptr) {
    throw std::invalid_argument("Cannot access the JS object " + objectName +
                                " because it does not exist or has been deleted!");
  }

  duk_pop(m_ctx);
  return cppJsObject->call(javaMethod, args);
}

JValue JsBridgeContext::callJsLambda(const std::string &strFunctionName,
                                     const JObjectArrayLocalRef &args,
                                     bool awaitJsPromise) {
  CHECK_STACK(m_ctx);

  // Get the JS lambda
  duk_get_global_string(m_ctx, strFunctionName.c_str());
  if (!duk_is_function(m_ctx, -1)) {
    duk_pop(m_ctx);
    throw std::invalid_argument("The JS method " + strFunctionName + " cannot be called (not a function)");
  }

  // Get C++ JavaScriptLambda instance
  auto cppJsLambda = m_utils->getMappedCppPtrValue<JavaScriptLambda>(-1, strFunctionName.c_str());
  if (cppJsLambda == nullptr) {
    throw std::invalid_argument("Cannot access the JS object " + strFunctionName +
                                " because it does not exist or has been deleted!");
  }

  duk_pop(m_ctx);
  CHECK_STACK_NOW();

  return cppJsLambda->call(this, args, awaitJsPromise);
}

void JsBridgeContext::assignJsValue(const std::string &strGlobalName, const std::string &strCode) {
  CHECK_STACK(m_ctx);

  if (duk_peval_string(m_ctx, strCode.c_str()) != DUK_EXEC_SUCCESS) {
    alog("Could not assign JS value:\n%s", strCode.c_str());
    queueJavaExceptionForJsError();
    duk_pop(m_ctx);
    return;
  }

  duk_put_global_string(m_ctx, strGlobalName.c_str());
}

void JsBridgeContext::newJsFunction(const std::string &strGlobalName, const JObjectArrayLocalRef &args, const std::string &strCode) {
  CHECK_STACK(m_ctx);

  // Push global Function (which can be constructed with "new Function"
  duk_get_global_string(m_ctx, "Function");

  // This fails with "constructor requires 'new'"
  //duk_require_constructor_call(m_ctx);

  // Push all arguments (as string)
  jsize argCount = args.getLength();
  for (jsize i = 0; i < argCount; ++i) {
    JStringLocalRef argString(args.getElement<jstring>(i));
    duk_push_string(m_ctx, argString.c_str());
  }

  // Push JS code as string
  duk_push_string(m_ctx, strCode.c_str());

  // New Function(arg1, arg2, ..., jsCode)
  if (duk_pnew(m_ctx, argCount + 1) != DUK_EXEC_SUCCESS) {
    queueJavaExceptionForJsError();
    duk_pop(m_ctx);
    return;
  }

  duk_put_global_string(m_ctx, strGlobalName.c_str());
}

void JsBridgeContext::processPromiseQueue() {
  // No built-in promise
}

// static
JsBridgeContext *JsBridgeContext::getInstance(duk_context *ctx) {
  duk_push_global_stash(ctx);
  duk_get_prop_string(ctx, -1, JSBRIDGE_CPP_CLASS_PROP_NAME);
  auto duktapeContext = reinterpret_cast<JsBridgeContext *>(duk_require_pointer(ctx, -1));
  duk_pop_2(ctx);

  return duktapeContext;
}

void JsBridgeContext::throwTypeException(const std::string &message, bool inScript) const {
  if (inScript) {
    duk_error(m_ctx, DUK_RET_TYPE_ERROR, message.c_str());
  } else {
    throw std::invalid_argument(message);
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
      JniLocalRef<jthrowable>()  // cause
  );
  jniContext()->throw_(exception);
}

//void JsBridgeContext::queueNullPointerException(const std::string &message) const {
//  JniLocalRef<jclass> exceptionClass = findClass("java/lang/NullPointerException");
//  m_jniEnv->ThrowNew(exceptionClass.create(), message.c_str());
//}

bool JsBridgeContext::hasPendingJniException() const {
  return jniContext()->exceptionCheck();
}

void JsBridgeContext::rethrowJniException() const {
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
  duk_push_error_object(m_ctx, DUK_ERR_ERROR, message.c_str());
  duk_push_pointer(m_ctx, exception.toNewRawLocalRef());
  duk_put_prop_string(m_ctx, -2, JAVA_EXCEPTION_PROP_NAME);

  duk_throw(m_ctx);
}

// Sets up a Java {@code DuktapeException} based on the Duktape JavaScript error at the top of the
// Duktape stack. The exception will be thrown to the Java caller when the current JNI call returns.
//
void JsBridgeContext::queueJavaExceptionForJsError() const {
  jniContext()->throw_(getJavaExceptionForJsError());
}

JniLocalRef<jthrowable> JsBridgeContext::getJavaExceptionForJsError() const {
  CHECK_STACK(m_ctx);

  JniLocalRef<jclass> exceptionClass = jniContext()->findClass("de/prosiebensat1digital/oasisjsbridge/JsException");

  jmethodID newException = jniContext()->getMethodID(
      exceptionClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)V");

  // Create the JSON string
  const char *jsonStringRaw = nullptr;
  if (custom_stringify(m_ctx, -1) == DUK_EXEC_SUCCESS) {
    jsonStringRaw = duk_require_string(m_ctx, -1);
  }
  JStringLocalRef jsonString(jniContext(), jsonStringRaw);
  duk_pop(m_ctx);  // stringified string

  duk_dup(m_ctx, -1);  // JS error
  const std::string stack = duk_safe_to_stacktrace(m_ctx, -1);
  duk_pop(m_ctx);  // duplicated JS error

  std::size_t firstEndOfLine = stack.find('\n');
  std::string strFirstLine = firstEndOfLine == std::string::npos ? stack : stack.substr(0, firstEndOfLine);
  std::string strJsStacktrace = firstEndOfLine == std::string::npos ? "" : stack.substr(firstEndOfLine, std::string::npos);

  // Is there an exception thrown from a Java method?
  JniLocalRef<jthrowable> cause;
  if (duk_is_object(m_ctx, -1) && !duk_is_null(m_ctx, -1) && duk_has_prop_string(m_ctx, -1, JAVA_EXCEPTION_PROP_NAME)) {
    duk_get_prop_string(m_ctx, -1, JAVA_EXCEPTION_PROP_NAME);
    cause = JniLocalRef<jthrowable>(jniContext(), static_cast<jthrowable>(duk_get_pointer(m_ctx, -1)));
    duk_pop(m_ctx);  // Java exception
  }

  return jniContext()->newObject<jthrowable>(
      exceptionClass,
      newException,
      jsonString,  // jsonValue
      JStringLocalRef(jniContext(), strFirstLine.c_str()),  // detailedMessage
      JStringLocalRef(jniContext(), strJsStacktrace.c_str()),  // jsStackTrace
      cause
  );
}
