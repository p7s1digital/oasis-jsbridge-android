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
#ifndef _JSBRIDGE_AUTORELEASEDJSVALUE_H
#define _JSBRIDGE_AUTORELEASEDJSVALUE_H

#include "quickjs/quickjs.h"

#define JS_AUTORELEASE_VALUE(ctx, v) _AutoReleasedJSValue _autoReleased_ ## v(ctx, v)

// Utility class using RAII to automatically free the given JSValue when leaving the current scope.
// While it is usually easy enough too manually free the values via JS_Free, using this class can
// be very convenient in some cases, especially when dealing with C++ exceptions.
//
// Note: the class itself is internal, use JS_AUTORELEASE_VALUE(ctx, v)
class _AutoReleasedJSValue {

public:
  _AutoReleasedJSValue() = delete;
  _AutoReleasedJSValue(JSContext *ctx, JSValue v)
    : m_ctx(ctx), m_value(v) {}

  ~_AutoReleasedJSValue() {
    JS_FreeValue(m_ctx, m_value);
  }

  _AutoReleasedJSValue(const _AutoReleasedJSValue &) = delete;
  _AutoReleasedJSValue &operator=(const _AutoReleasedJSValue &) = delete;

private:
  JSContext *m_ctx;
  JSValue m_value;
};

#endif

