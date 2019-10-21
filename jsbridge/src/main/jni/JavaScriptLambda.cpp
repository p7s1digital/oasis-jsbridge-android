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
#include "JavaScriptLambda.h"

#include "AutoReleasedJSValue.h"
#include "JavaType.h"
#include "JsBridgeContext.h"
#include "JavaScriptMethod.h"
#include "jni-helpers/JValue.h"
#include "jni-helpers/JniContext.h"

JavaScriptLambda::~JavaScriptLambda() {
  delete m_method;
}

#if defined(DUKTAPE)

#include "StackChecker.h"

JavaScriptLambda::JavaScriptLambda(const JsBridgeContext *jsBridgeContext, const JniRef<jsBridgeMethod> &method, std::string strName, duk_idx_t jsLambdaIndex)
 : m_method(nullptr) {
  duk_context *ctx = jsBridgeContext->getDuktapeContext();
  CHECK_STACK(ctx);

  m_jsHeapPtr = duk_get_heapptr(ctx, jsLambdaIndex);
  duk_push_heapptr(ctx, m_jsHeapPtr);

  if (!duk_is_function(ctx, -1)) {
    duk_pop(ctx);
    throw std::runtime_error("JavaScript object " + strName + "cannot be accessed");
  }

  m_method = new JavaScriptMethod(jsBridgeContext, method, std::move(strName), true);
  duk_pop(ctx);  // JS lambda
}

JValue JavaScriptLambda::call(const JsBridgeContext *jsBridgeContext, const JObjectArrayLocalRef &args, bool awaitJsPromise) const {
  return m_method->invoke(jsBridgeContext, m_jsHeapPtr, args, awaitJsPromise);
}

#elif defined(QUICKJS)

JavaScriptLambda::JavaScriptLambda(const JsBridgeContext *jsBridgeContext, const JniRef<jsBridgeMethod> &method, std::string strName, JSValue jsLambdaValue)
 : m_method(nullptr)
 , m_name(std::move(strName)) {

  m_ctx = jsBridgeContext->getQuickJsContext();

  if (!JS_IsFunction(m_ctx, jsLambdaValue)) {
    throw std::runtime_error("JavaScript lambda " + strName + " cannot be accessed (not a function)");
  }

  m_method = new JavaScriptMethod(jsBridgeContext, method, strName, true);
}

JValue JavaScriptLambda::call(const JsBridgeContext *jsBridgeContext, const JObjectArrayLocalRef &args, bool awaitJsPromise) const {
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JSValue jsLambdaValue = JS_GetPropertyStr(m_ctx, globalObj, m_name.c_str());
  JS_FreeValue(m_ctx, globalObj);

  JS_AUTORELEASE_VALUE(m_ctx, jsLambdaValue);

  if (!JS_IsFunction(m_ctx, jsLambdaValue) || JS_IsNull(jsLambdaValue)) {
    throw std::invalid_argument(
        "Cannot call " + m_name + " lambda. It does not exist or is not a valid function.");
  }

  return m_method->invoke(jsBridgeContext, jsLambdaValue, JS_UNDEFINED, args, awaitJsPromise);
}

#endif
