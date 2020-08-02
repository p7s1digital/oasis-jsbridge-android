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
#include "JsException.h"
#include "JsBridgeContext.h"

#if defined(QUICKJS)
# include "QuickJsUtils.h"
#endif

namespace {
#if defined(DUKTAPE)
  const char *ERROR_PROP_NAME_PREFIX = "JsException_error_";

  std::string createMessage(const JsBridgeContext *jsBridgeContext, duk_idx_t idx) {
    duk_context *ctx = jsBridgeContext->getDuktapeContext();

    duk_dup(ctx, idx);
    std::string ret = duk_safe_to_string(ctx, -1);
    duk_pop(ctx);

    return ret;
  }
#elif defined(QUICKJS)
  std::string createMessage(const JsBridgeContext *jsBridgeContext, JSValueConst exceptionValue) {
    JSContext *ctx = jsBridgeContext->getQuickJsContext();
    const QuickJsUtils *utils = jsBridgeContext->getUtils();

    if (JS_IsError(ctx, exceptionValue)) {
      return utils->toString(JS_GetPropertyStr(ctx, exceptionValue, "message"));
    }

    return utils->toString(exceptionValue);
  }
#endif
}

#if defined(DUKTAPE)

JsException::JsException(const JsBridgeContext *jsBridgeContext, duk_idx_t idx)
 : m_jsBridgeContext(jsBridgeContext)
 , m_what(createMessage(jsBridgeContext, idx)) {

  duk_context *ctx = jsBridgeContext->getDuktapeContext();

  static int counter = 0;
  m_errorPropName = ERROR_PROP_NAME_PREFIX + std::to_string(++counter);

  duk_idx_t errorIdx = duk_normalize_index(ctx, idx);

  duk_push_heap_stash(ctx);
  duk_dup(ctx, errorIdx);  // duplicate error
  duk_put_prop_string(ctx, -2, m_errorPropName.c_str());
  duk_pop(ctx);  // heap stash
}

JsException::JsException(JsException &&other)
 : m_jsBridgeContext(other.m_jsBridgeContext) {

  std::swap(m_what, other.m_what);
  std::swap(m_errorPropName, other.m_errorPropName);
}

JsException::~JsException() {
  if (m_errorPropName.empty()) {
    return;
  }

  duk_context *ctx = m_jsBridgeContext->getDuktapeContext();

  duk_push_heap_stash(ctx);
  duk_del_prop_string(ctx, -1, m_errorPropName.c_str());
  duk_pop(ctx);  // heap stash
}

void JsException::pushError() const {
  duk_context *ctx = m_jsBridgeContext->getDuktapeContext();

  duk_push_heap_stash(ctx);
  duk_get_prop_string(ctx, -1, m_errorPropName.c_str());
  duk_remove(ctx, -2);  // heap stash
}

#elif defined(QUICKJS)

JsException::JsException(const JsBridgeContext *jsBridgeContext, JSValue exceptionValue)
 : m_jsBridgeContext(jsBridgeContext)
 , m_value(exceptionValue)
 , m_what(createMessage(jsBridgeContext, exceptionValue)) {
}

JsException::~JsException() {
  JS_FreeValue(m_jsBridgeContext->getQuickJsContext(), m_value);
}

#endif
