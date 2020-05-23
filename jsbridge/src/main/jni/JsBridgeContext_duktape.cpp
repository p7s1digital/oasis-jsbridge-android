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
#include "ExceptionHandler.h"
#include "JavaObject.h"
#include "JavaScriptLambda.h"
#include "JavaScriptObject.h"
#include "JniCache.h"
#include "StackChecker.h"
#include "log.h"
#include "exceptions/JsException.h"
#include "java-types/Deferred.h"
#include "jni-helpers/JniGlobalRef.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include "jni-helpers/JStringLocalRef.h"
#include "duktape/duk_trans_socket.h"
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>


// Internal
// ---

namespace {
  const char *JSBRIDGE_CPP_CLASS_PROP_NAME = "\xff\xffjsbridge_cpp";

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

  delete m_exceptionHandler;
  delete m_utils;
  delete m_jniCache;
}

void JsBridgeContext::init(JniContext *jniContext, const JniLocalRef<jobject> &jsBridgeObject) {

  m_jniContext = jniContext;

  m_ctx = duk_create_heap(nullptr, nullptr, nullptr, nullptr, fatalErrorHandler);

  if (!m_ctx) {
    throw std::bad_alloc();
  }

  m_jniCache = new JniCache(this, jsBridgeObject);
  m_utils = new DuktapeUtils(jniContext, m_ctx);
  m_exceptionHandler = new ExceptionHandler(this);

  // Stash the JsBridgeContext instance in the context, so we can find our way back from a Duktape C callback.
  duk_push_global_stash(m_ctx);
  duk_push_pointer(m_ctx, this);
  duk_put_prop_string(m_ctx, -2, JSBRIDGE_CPP_CLASS_PROP_NAME);
  duk_pop(m_ctx);

  // Set global + window (TODO)
  // See also https://wiki.duktape.org/howtoglobalobjectreference
  static const char *str1 = "var global = this; var window = this; window.open = function() {};\n";
  duk_eval_string_noresult(m_ctx, str1);
}

void JsBridgeContext::startDebugger(int port) {

  // Call Java onDebuggerPending()
  m_jniCache->getJsBridgeInterface().onDebuggerPending();

  alog_info("Debugger enabled, create socket and wait for connection\n");
  duk_trans_socket_init(port);
  duk_trans_socket_waitconn(port);
  alog_info("Debugger connected, call duk_debugger_attach() and then execute requested file(s)/eval\n");

  // Call Java onDebuggerReady()
  m_jniCache->getJsBridgeInterface().onDebuggerReady();

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

JValue JsBridgeContext::evaluateString(const JStringLocalRef &strCode, const JniLocalRef<jsBridgeParameter> &returnParameter,
                                       bool awaitJsPromise) const {
  CHECK_STACK(m_ctx);

  //alog("Evaluating string: %s", strCode.toUtf8Chars());

  duk_int_t ret = duk_peval_string(m_ctx, strCode.toUtf8Chars());
  strCode.releaseChars();  // release chars now as we don't need them anymore

  if (ret != DUK_EXEC_SUCCESS) {
    alog("Could not evaluate string");
    throw m_exceptionHandler->getCurrentJsException();
  }

  bool isDeferred = awaitJsPromise && duk_is_object(m_ctx, -1) && duk_has_prop_string(m_ctx, -1, "then");
  if (!isDeferred && returnParameter.isNull()) {
    // No return type given: try to guess it out of the JS value
    const int supportedTypeMask = DUK_TYPE_MASK_BOOLEAN | DUK_TYPE_MASK_NUMBER | DUK_TYPE_MASK_STRING;

    if (duk_check_type_mask(m_ctx, -1, supportedTypeMask)) {
      // The result is a supported scalar type - return it.
      return m_javaTypeProvider.getObjectType()->pop();
    }

    if (duk_is_array(m_ctx, -1)) {
      return m_javaTypeProvider.getObjectType()->popArray(1, false /*expand*/);
    }

    // The result is an unsupported type, undefined, or null.
    duk_pop(m_ctx);
    return JValue();
  }

  auto returnType = m_javaTypeProvider.makeUniqueType(returnParameter, true /*boxed*/);

  if (isDeferred && !returnType->isDeferred()) {
    return m_javaTypeProvider.getDeferredType(returnParameter)->pop();
  }

  return returnType->pop();
}

void JsBridgeContext::evaluateFileContent(const JStringLocalRef &strCode, const std::string &strFileName) const {

  CHECK_STACK(m_ctx);

  duk_push_string(m_ctx, strFileName.c_str());

  duk_int_t ret = duk_pcompile_string_filename(m_ctx, DUK_COMPILE_EVAL, strCode.toUtf8Chars());
  strCode.releaseChars();  // release chars now as we don't need them anymore

  if (ret != DUK_EXEC_SUCCESS) {
    alog("Could not compile file %s", strFileName.c_str());
    throw m_exceptionHandler->getCurrentJsException();
  }

  if (duk_pcall(m_ctx, 0) != DUK_EXEC_SUCCESS) {
    alog("Could not execute file %s", strFileName.c_str());
    throw m_exceptionHandler->getCurrentJsException();
  }

  duk_pop(m_ctx);  // unused pcall result
}

void JsBridgeContext::registerJavaObject(const std::string &strName, const JniLocalRef<jobject> &object,
                                         const JObjectArrayLocalRef &methods) {
  CHECK_STACK(m_ctx);

  duk_push_global_object(m_ctx);

  if (duk_has_prop_string(m_ctx, -1, strName.c_str())) {
    duk_pop(m_ctx);  // global object
    throw std::invalid_argument("A global object called " + strName + " already exists");
  }

  try {
    JavaObject::push(this, strName, object, methods);
  } catch (const std::exception &) {
    duk_pop(m_ctx);  // global object
    throw;
  }

  // Make our bound Java object a property of the Duktape global object (so it's a JS global).
  duk_put_prop_string(m_ctx, -2, strName.c_str());

  duk_pop(m_ctx);  // global object
}

void JsBridgeContext::registerJavaLambda(const std::string &strName, const JniLocalRef<jobject> &object,
                                         const JniLocalRef<jsBridgeMethod> &method) {

  CHECK_STACK(m_ctx);

  duk_push_global_object(m_ctx);

  if (duk_has_prop_string(m_ctx, -1, strName.c_str())) {
    duk_pop(m_ctx);  // global object
    throw std::invalid_argument("A global object called " + strName + " already exists");
  }

  try {
    JavaObject::pushLambda(this, strName, object, method);
  } catch (const std::exception &) {
    duk_pop(m_ctx);  // global object
    throw;
  }

  // Make our Java lambda a property of the Duktape global object (so it's a JS global).
  duk_put_prop_string(m_ctx, -2, strName.c_str());

  duk_pop(m_ctx);  // global object
}

void JsBridgeContext::registerJsObject(const std::string &strName,
                                       const JObjectArrayLocalRef &methods,
                                       bool check) {
  CHECK_STACK(m_ctx);

  duk_get_global_string(m_ctx, strName.c_str());

  try {
    // Create the JavaScriptObject instance (which takes over jsObjectValue and will free it in its destructor)
    auto cppJsObject = new JavaScriptObject(this, strName, -1, methods, check);  // auto-deleted

    // Wrap it inside the JS object
    m_utils->createMappedCppPtrValue(cppJsObject, -1, strName.c_str());

    duk_pop(m_ctx);  // JS object
  } catch (const std::exception &) {
    duk_pop(m_ctx);  // JS object
    throw;
  }
}

void JsBridgeContext::registerJsLambda(const std::string &strName,
                                       const JniLocalRef<jsBridgeMethod> &method) {
  CHECK_STACK(m_ctx);

  duk_get_global_string(m_ctx, strName.c_str());

  try {
    // Create the JavaScriptObject instance
    auto cppJsLambda = new JavaScriptLambda(this, method, strName, -1);  // auto-deleted

    // Wrap it inside the JS object
    m_utils->createMappedCppPtrValue(cppJsLambda, -1, strName.c_str());

    duk_pop(m_ctx);  // JS lambda
  } catch (const std::exception &) {
    duk_pop(m_ctx);  // JS lambda
    throw;
  }
}

JValue JsBridgeContext::callJsMethod(const std::string &objectName,
                                     const JniLocalRef<jobject> &javaMethod,
                                     const JObjectArrayLocalRef &args,
                                     bool awaitJsPromise) {
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
    duk_pop(m_ctx);
    throw std::invalid_argument("Cannot access the JS object " + objectName +
                                " because it does not exist or has been deleted!");
  }

  duk_pop(m_ctx);
  return cppJsObject->call(javaMethod, args, awaitJsPromise);
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
    duk_pop(m_ctx);
    throw std::invalid_argument("Cannot access the JS object " + strFunctionName +
                                " because it does not exist or has been deleted!");
  }

  duk_pop(m_ctx);
  return cppJsLambda->call(this, args, awaitJsPromise);
}

void JsBridgeContext::assignJsValue(const std::string &strGlobalName, const JStringLocalRef &strCode) {
  CHECK_STACK(m_ctx);

  duk_int_t ret = duk_peval_string(m_ctx, strCode.toUtf8Chars());
  strCode.releaseChars();  // release chars now as we don't need them anymore

  if (ret != DUK_EXEC_SUCCESS) {
    alog("Could not assign JS value %s", strGlobalName.c_str());
    throw m_exceptionHandler->getCurrentJsException();
  }

  duk_put_global_string(m_ctx, strGlobalName.c_str());
}

void JsBridgeContext::deleteJsValue(const std::string &strGlobalName) {
  CHECK_STACK(m_ctx);

  duk_push_global_object(m_ctx);
  duk_del_prop_string(m_ctx, -1, strGlobalName.c_str());
  duk_pop(m_ctx);
}

void JsBridgeContext::copyJsValue(const std::string &strGlobalNameTo, const std::string &strGlobalNameFrom) {
  CHECK_STACK(m_ctx);

  duk_push_global_object(m_ctx);
  duk_get_prop_string(m_ctx, -1, strGlobalNameFrom.c_str());
  duk_put_prop_string(m_ctx, -2, strGlobalNameTo.c_str());
  duk_pop(m_ctx);
}

void JsBridgeContext::newJsFunction(const std::string &strGlobalName, const JObjectArrayLocalRef &args, const JStringLocalRef &strCode) {
  CHECK_STACK(m_ctx);

  // Push global Function (which can be constructed with "new Function"
  duk_get_global_string(m_ctx, "Function");

  // This fails with "constructor requires 'new'"
  //duk_require_constructor_call(m_ctx);

  // Push all arguments (as string)
  jsize argCount = args.getLength();
  for (jsize i = 0; i < argCount; ++i) {
    JStringLocalRef argString(args.getElement<jstring>(i));
    duk_push_string(m_ctx, argString.toUtf8Chars());
  }

  // Push JS code as string
  duk_push_string(m_ctx, strCode.toUtf8Chars());
  strCode.releaseChars();  // release chars now as we don't need them anymore

  // New Function(arg1, arg2, ..., jsCode)
  if (duk_pnew(m_ctx, argCount + 1) != DUK_EXEC_SUCCESS) {
    throw m_exceptionHandler->getCurrentJsException();
  }

  duk_put_global_string(m_ctx, strGlobalName.c_str());
}

void JsBridgeContext::convertJavaValueToJs(const std::string &strGlobalName, const JniLocalRef<jobject> &javaValue, const JniLocalRef<jsBridgeParameter> &parameter) {

  auto type = m_javaTypeProvider.makeUniqueType(parameter, true /*boxed*/);

  type->push(JValue(javaValue));
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

