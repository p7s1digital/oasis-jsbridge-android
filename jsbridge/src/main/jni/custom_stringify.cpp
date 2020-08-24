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

#include "custom_stringify.h"
#include <cstring>

static const char *customStringifyJs = R"(
// Custom stringify which probably handles Error instances
// See https://stackoverflow.com/questions/18391212/is-it-not-possible-to-stringify-an-error-using-json-stringify
globalThis.__jsBridge__stringify = function(value, keepErrorStack) {
  if (value === undefined) return "";

  var replaceErrors = function(_key, value) {
    if (value instanceof Error) {
      // Replace Error instance into plain JS objects using Error own properties
      return Object.getOwnPropertyNames(value).reduce(function(acc, key) {
        if (!keepErrorStack && key === "stack") return acc;
        acc[key] = value[key];
        return acc;
      }, {});
    }

    return value;
  }

  return JSON.stringify(value, replaceErrors);
};)";

#if defined(DUKTAPE)

#include "StackChecker.h"

// [... object ...] -> [... object ... jsonString]
duk_int_t custom_stringify(duk_context *ctx, duk_idx_t idx, bool keepErrorStack) {
  CHECK_STACK_OFFSET(ctx, 1);

  idx = duk_normalize_index(ctx, idx);

#if 0
  // 1. Fast/easy way. BUT: (like JSON.stringify) does not properly serialize Error instances!
  duk_dup(ctx, idx);
  duk_json_encode(ctx, -1);  // object to JSON string
  return DUK_EXEC_SUCCESS;
#else
  // 2. Custom stringify which properly serializes Error instances
  duk_get_global_string(ctx, "__jsBridge__stringify");
  if (duk_is_undefined(ctx, -1)) {
    duk_pop(ctx);  // (undefined) __jsBridge__stringify
    duk_eval_string_noresult(ctx, customStringifyJs);
    duk_get_global_string(ctx, "__jsBridge__stringify");
  }

  duk_dup(ctx, idx);
  duk_push_boolean(ctx, keepErrorStack);
  return duk_pcall(ctx, 2);
#endif
}

#elif defined(QUICKJS)

JSValue custom_stringify(JSContext *ctx, JSValueConst v, bool keepErrorStack) {
  JSValue ret;

#if 0
  // 1. JSON stringify (but does not properly serialize Error instances!)
  ret = JS_JSONStringify(ctx, v, JS_UNDEFINED, JS_UNDEFINED);
#else
  // 2. Custom stringify which properly serializes Error instances
  JSValue globalObj = JS_GetGlobalObject(ctx);
  JSValue jsbridgeStringifyValue = JS_GetPropertyStr(ctx, globalObj, "__jsBridge__stringify");
  if (JS_IsUndefined(jsbridgeStringifyValue)) {
    jsbridgeStringifyValue = JS_Eval(ctx, customStringifyJs, strlen(customStringifyJs), "custom_stringify.cpp", 0);
    JS_SetPropertyStr(ctx, globalObj, "__jsBridge__stringify", JS_DupValue(ctx, jsbridgeStringifyValue));
  }

  JSValue args[] = { v, JS_NewBool(ctx, keepErrorStack) };
  ret = JS_Call(ctx, jsbridgeStringifyValue, JS_NULL, 2, args);

  JS_FreeValue(ctx, jsbridgeStringifyValue);
  JS_FreeValue(ctx, globalObj);
#endif
  return ret;
}

#include <cstring>

#endif
