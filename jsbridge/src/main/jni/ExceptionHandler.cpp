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
#include "AutoReleasedJSValue.h"
#include "ExceptionHandler.h"
#include "JniCache.h"
#include "JsBridgeContext.h"
#include "custom_stringify.h"
#include "exceptions/JniException.h"
#include "exceptions/JsException.h"
#include "jni-helpers/JniContext.h"
#include "log.h"

#if defined(DUKTAPE)
# include "DuktapeUtils.h"
#elif defined(QUICKJS)
# include "QuickJsUtils.h"
#endif


// Internal
// ---

namespace {
  const char *JAVA_EXCEPTION_PROP_NAME = "__java_exception";
}


// Class methods
// ---

ExceptionHandler::ExceptionHandler(const JsBridgeContext *jsBridgeContext)
 : m_jsBridgeContext(jsBridgeContext) {
}

// Throws a C++ JsException based on the current JavaScript error:
// - Duktape: error at the top of the Duktape stack (note: the error will be popped!)
// - QuickJS: the current exception exception fetched via JS_GetException
JsException ExceptionHandler::getCurrentJsException() const {
#if defined(DUKTAPE)
  auto ret = JsException(m_jsBridgeContext, -1);
  duk_pop(m_jsBridgeContext->getDuktapeContext());
  return std::move(ret);
#elif defined(QUICKJS)
  JSValue exceptionValue = JS_GetException(m_jsBridgeContext->getQuickJsContext());
  return JsException(m_jsBridgeContext, exceptionValue);
#endif
}

void ExceptionHandler::jsThrow(const std::exception &e) const {
#if defined(DUKTAPE)
  duk_context *ctx = m_jsBridgeContext->getDuktapeContext();

  if (auto jniException = dynamic_cast<const JniException *>(&e)) {
    pushJavaException(jniException->getThrowable());
    duk_throw(ctx);
  } else if (auto jsException = dynamic_cast<const JsException *>(&e)) {
    jsException->pushError();
    duk_throw(ctx);
  } else if (dynamic_cast<const std::invalid_argument *>(&e)) {
    duk_error(ctx, DUK_ERR_TYPE_ERROR, e.what());
  } else {
    duk_error(ctx, DUK_ERR_ERROR, e.what());
  }
#elif defined(QUICKJS)
  JSContext *ctx = m_jsBridgeContext->getQuickJsContext();

  if (auto jniException = dynamic_cast<const JniException *>(&e)) {
    JS_Throw(ctx, javaExceptionToJsValue(jniException->getThrowable()));
    // No JS_FreeValue(m_ctx, errorValue) after JS_Throw()
  } else if (auto jsException = dynamic_cast<const JsException *>(&e)) {
    JS_Throw(ctx, jsException->getValue());
    // No JS_FreeValue(m_ctx, errorValue) after JS_Throw()
  } else if (dynamic_cast<const std::invalid_argument *>(&e)) {
    JS_ThrowTypeError(m_jsBridgeContext->getQuickJsContext(), "%s", e.what());
  } else {
    JS_ThrowInternalError(m_jsBridgeContext->getQuickJsContext(), "%s", e.what());
  }
#endif
}

void ExceptionHandler::jniThrow(const std::exception &e) const {
  const JniContext *jniContext = m_jsBridgeContext->getJniContext();

  if (auto jniException = dynamic_cast<const JniException *>(&e)) {
    jniContext->throw_(jniException->getThrowable());
  } else if (auto jsException = dynamic_cast<const JsException *>(&e)) {
    jniContext->throw_(getJavaException(*jsException));
  } else if (dynamic_cast<const std::invalid_argument *>(&e)) {
    const JniCache *jniCache = m_jsBridgeContext->getJniCache();
    jniContext->throwNew(jniCache->getIllegalArgumentExceptionClass(), e.what());
  } else {
    const JniCache *jniCache = m_jsBridgeContext->getJniCache();
    jniContext->throwNew(jniCache->getRuntimeExceptionClass(), e.what());
  }
}

JniLocalRef<jthrowable> ExceptionHandler::getJavaException(const JsException &jsException) const {
#if defined(DUKTAPE)
  auto ctx = m_jsBridgeContext->getDuktapeContext();
  const JniContext *jniContext = m_jsBridgeContext->getJniContext();

  CHECK_STACK(ctx);

  jsException.pushError();

  // Create the JSON string
  const char *jsonStringRaw = nullptr;
  if (custom_stringify(ctx, -1) == DUK_EXEC_SUCCESS) {
    jsonStringRaw = duk_get_string(ctx, -1);
  }
  JStringLocalRef jsonString(jniContext, jsonStringRaw);
  duk_pop(ctx);  // stringified string

  duk_dup(ctx, -1);  // JS error
  const std::string stack = duk_safe_to_stacktrace(ctx, -1);
  duk_pop(ctx);  // duplicated JS error

  std::size_t firstEndOfLine = stack.find('\n');
  std::string strJsStacktrace = firstEndOfLine == std::string::npos ? "" : stack.substr(firstEndOfLine, std::string::npos);

  // Is there an exception thrown from a Java method?
  JniLocalRef<jthrowable> cause;
  if (duk_is_object(ctx, -1) && !duk_is_null(ctx, -1) && duk_has_prop_string(ctx, -1, JAVA_EXCEPTION_PROP_NAME)) {
    duk_get_prop_string(ctx, -1, JAVA_EXCEPTION_PROP_NAME);
    cause = m_jsBridgeContext->getUtils()->getJavaRef<jthrowable>(-1);
    duk_pop(ctx);  // Java exception
  }

  duk_pop(ctx);  // error

  return m_jsBridgeContext->getJniCache()->newJsException(
      jsonString,  // jsonValue
      JStringLocalRef(jniContext, jsException.what()),  // detailedMessage
      JStringLocalRef(jniContext, strJsStacktrace.c_str()),  // jsStackTrace
      cause
  );
#elif defined(QUICKJS)
  auto ctx = m_jsBridgeContext->getQuickJsContext();
  const JniContext *jniContext = m_jsBridgeContext->getJniContext();
  const JniCache *jniCache = m_jsBridgeContext->getJniCache();
  const QuickJsUtils *utils = m_jsBridgeContext->getUtils();

  JniLocalRef<jthrowable> ret;

  JSValue exceptionValue = jsException.getValue();
  JSValue jsonValue = custom_stringify(ctx, exceptionValue);
  JStringLocalRef jsonString;
  if (JS_IsException(jsonValue)) {
    JS_GetException(ctx);
  } else {
    jsonString = utils->toJString(jsonValue);
  }

  // Is there an exception thrown from a Java method?
  JniLocalRef<jthrowable> cause;
  if (JS_IsObject(exceptionValue) && !JS_IsNull(exceptionValue)) {
    JSValue javaExceptionValue = JS_GetPropertyStr(ctx, exceptionValue, JAVA_EXCEPTION_PROP_NAME);
    if (!JS_IsUndefined(javaExceptionValue)) {
      cause = utils->getJavaRef<jthrowable>(javaExceptionValue);
      JS_FreeValue(ctx, javaExceptionValue);
    }
  }

  std::string stack;

  if (JS_IsError(ctx, exceptionValue)) {
    // Get the stack trace
    JSValue stackValue = JS_GetPropertyStr(ctx, exceptionValue, "stack");
    if (!JS_IsUndefined(stackValue)) {
      stack = utils->toString(stackValue);
    }
    JS_FreeValue(ctx, stackValue);
  }

  ret = jniCache->newJsException(
      jsonString,  // jsonValue
      JStringLocalRef(jniContext, jsException.what()),  // detailedMessage
      JStringLocalRef(jniContext, stack.c_str()),  // jsStackTrace
      cause
  );

  JS_FreeValue(ctx, jsonValue);
  return ret;
#endif
}

#if defined(DUKTAPE)

void ExceptionHandler::pushJavaException(const JniLocalRef<jthrowable> &throwable) const {
  const JniContext *jniContext = m_jsBridgeContext->getJniContext();
  auto exceptionClass = jniContext->getObjectClass(throwable);
  jmethodID getMessage = jniContext->getMethodID(exceptionClass, "getMessage", "()Ljava/lang/String;");
  JStringLocalRef messageRef = jniContext->callStringMethod(throwable, getMessage);


  // Propagate Java exception to JavaScript (and store pointer to Java exception)
  // ---

  auto ctx = m_jsBridgeContext->getDuktapeContext();

  duk_push_error_object(ctx, DUK_ERR_ERROR, messageRef.toUtf8Chars());

  m_jsBridgeContext->getUtils()->pushJavaRefValue(throwable);
  duk_put_prop_string(ctx, -2, JAVA_EXCEPTION_PROP_NAME);
}

#elif defined(QUICKJS)

JSValue ExceptionHandler::javaExceptionToJsValue(const JniLocalRef<jthrowable> &throwable) const {
  const JniContext *jniContext = m_jsBridgeContext->getJniContext();
  auto exceptionClass = jniContext->getObjectClass(throwable);
  jmethodID getMessage = jniContext->getMethodID(exceptionClass, "getMessage", "()Ljava/lang/String;");
  JStringLocalRef messageRef = jniContext->callStringMethod(throwable, getMessage);


  // Propagate Java exception to JavaScript (and store pointer to Java exception)
  // ---

  auto ctx = m_jsBridgeContext->getQuickJsContext();

  JSValue errorValue = JS_NewError(ctx);
  JSValue messageValue = JS_NewString(ctx, messageRef.toUtf8Chars());
  JS_SetPropertyStr(ctx, errorValue, "message", messageValue);
  // No JS_FreeValue(m_ctx, messageValue) after JS_SetPropertyStr()

  JSValue javaExceptionValue = m_jsBridgeContext->getUtils()->createJavaRefValue(throwable);
  JS_SetPropertyStr(ctx, errorValue, JAVA_EXCEPTION_PROP_NAME, javaExceptionValue);
  // No JS_FreeValue(m_ctx, javaExcueptionValue) after JS_SetPropertyStr()

  return errorValue;
}

#endif
