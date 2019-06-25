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

#include "JsBridgeContext.h"
#include "JavaScriptMethod.h"
#include "StackChecker.h"
#include "jni-helpers/JValue.h"
#include "jni-helpers/JniContext.h"

#if defined(DUKTAPE)

JavaScriptLambda::JavaScriptLambda(const JsBridgeContext *jsBridgeContext, const JniRef<jsBridgeMethod> &method, std::string strName, void *jsHeapPtr)
 : JavaScriptObjectBase()
 , m_method(nullptr)
 , m_jsHeapPtr(jsHeapPtr) {

  duk_context *ctx = jsBridgeContext->getCContext();
  CHECK_STACK(ctx);

  duk_push_heapptr(ctx, m_jsHeapPtr);

  if (!duk_is_object(ctx, -1)) {
    duk_pop(ctx);
    throw std::runtime_error("JavaScript object " + strName + "cannot be accessed from its weak ptr");
  }

  m_method = new JavaScriptMethod(jsBridgeContext, method, std::move(strName), true);

  duk_pop(ctx);  // JS lambda
}

JavaScriptLambda::~JavaScriptLambda() {
  delete m_method;
}

JValue JavaScriptLambda::call(const JsBridgeContext *jsBridgeContext, const JObjectArrayLocalRef &args, bool awaitJsPromise) const {
  return m_method->invoke(jsBridgeContext, m_jsHeapPtr, args, awaitJsPromise);
}

#elif defined(QUICKJS)

JavaScriptLambda::JavaScriptLambda(const JsBridgeContext *jsBridgeContext, const JniRef<jsBridgeMethod> &method, std::string strName, JSValue v)
 : m_method(nullptr)
 , m_jsValue(v) {

  m_ctx = jsBridgeContext->getCContext();

  if (!JS_IsFunction(m_ctx, v)) {
    throw std::runtime_error("JavaScript lambda " + strName + " cannot be accessed (not a function)");
  }

  m_method = new JavaScriptMethod(jsBridgeContext, method, std::move(strName), true);
}

JavaScriptLambda::~JavaScriptLambda() {
  JS_FreeValue(m_ctx, m_jsValue);
  delete m_method;
}

JValue JavaScriptLambda::call(const JsBridgeContext *jsBridgeContext, const JObjectArrayLocalRef &args, bool awaitJsPromise) const {
  return m_method->invoke(jsBridgeContext, m_jsValue, JS_UNDEFINED, args, awaitJsPromise);
}

#endif
