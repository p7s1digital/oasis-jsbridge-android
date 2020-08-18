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
#include "QuickJsUtils.h"
#include "log.h"

// static
JSClassID QuickJsUtils::js_cppwrapper_class_id;

namespace {
  void js_cppwrapper_finalizer(JSRuntime *, JSValue val) {
    auto cppWrapper = reinterpret_cast<QuickJsUtils::CppWrapper *>(JS_GetOpaque(val, QuickJsUtils::js_cppwrapper_class_id));
    cppWrapper->deleter();
    delete cppWrapper;
  }

  JSClassDef js_cppwrapper_class = {
      "CPPWRAPPER",
      .finalizer = js_cppwrapper_finalizer,
  };
}

QuickJsUtils::QuickJsUtils(const JniContext *jniContext, JSContext *ctx)
 : m_jniContext(jniContext)
 , m_ctx(ctx) {
  // class ID (created once)
  JS_NewClassID(&js_cppwrapper_class_id);

  // class (created once per runtime)
  JSRuntime *rt = JS_GetRuntime(ctx);
  JS_NewClass(rt, js_cppwrapper_class_id, &js_cppwrapper_class);
}

bool QuickJsUtils::hasPropertyStr(JSValueConst this_obj, const char *prop) const {
  JSAtom atom = JS_NewAtom(m_ctx, prop);
  bool ret = JS_HasProperty(m_ctx, this_obj, atom) == 1;
  JS_FreeAtom(m_ctx, atom);
  return ret;
}

JStringLocalRef QuickJsUtils::toJString(JSValueConst v) const {
  // Seems that QuickJS crashes on Android when calling JS_ToString() for some objects
  // (assumption: objects wrapping C pointers)
  //if (JS_IsObject(v)) {
  //  alog("BW - QuickJsUtils::toJString() - converting object to [object Object]...");
  //  return JStringLocalRef(m_jniContext, "[object Object]");
  //}

  const char *cstr = JS_ToCString(m_ctx, v);
  JStringLocalRef ret(m_jniContext, cstr);
  JS_FreeCString(m_ctx, cstr);
  return ret;
}

std::string QuickJsUtils::toString(JSValueConst v) const {
  // Seems that QuickJS crashes on Android when calling JS_ToString() for some objects
  // (assumption: objects wrapping C pointers)
  //if (JS_IsObject(v)) {
  //  alog("BW - QuickJsUtils::toString() - converting object to [object Object]...");
  //  return "[object Object]";
  //}

  alog("BW - QuickJsUtils::toString() - P0 - tag = %d", JS_VALUE_GET_TAG(v));
  const char *cstr = JS_ToCString(m_ctx, v);
  alog("BW - QuickJsUtils::toString() - P1");
  alog("BW - QuickJsUtils::toString() - P2 - cstr = %s", cstr);
  std::string ret = cstr;
  JS_FreeCString(m_ctx, cstr);
  alog("BW - QuickJsUtils::toString() - PE");
  return ret;
}

