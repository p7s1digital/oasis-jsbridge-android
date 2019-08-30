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
#ifndef _JSBRIDGE_QUICKJS_UTILS_H
#define _JSBRIDGE_QUICKJS_UTILS_H

#include "jni-helpers/JniGlobalRef.h"
#include "jni-helpers/JStringLocalRef.h"
#include "quickjs/quickjs.h"
#include <functional>

static const char *CPP_OBJECT_MAP_PROP_NAME = "__cpp_object_map";


class QuickJsUtils {

public:
  QuickJsUtils() = delete;
  QuickJsUtils(const QuickJsUtils &) = delete;
  QuickJsUtils &operator=(const QuickJsUtils &) = delete;

  QuickJsUtils(const JniContext *, JSContext *);

  bool hasPropertyStr(JSValueConst this_obj, const char *prop) const;
  JStringLocalRef toJString(JSValueConst v) const;
  std::string toString(JSValueConst v) const;

  // Wrap a C++ instance inside a new JSValue and (optionally) ensure that it is
  // deleted when the JSValue gets finalized
  template <class T>
  JSValue createCppPtrValue(T *obj, bool deleteOnFinalize) const {
    JSValue cppWrapperObj = JS_NewObjectClass(m_ctx, js_cppwrapper_class_id);

    auto deleter = [deleteOnFinalize, obj]() {
      if (deleteOnFinalize) {
        delete obj;
      }
    };

    auto cppWrapper = new CppWrapper { obj, deleter };
    JS_SetOpaque(cppWrapperObj, cppWrapper);
    return cppWrapperObj;
  }

  // Access the instance wrapped in a JSValue via createCppPtrValue()
  template <class T>
  static T *getCppPtr(JSValueConst cppWrapperValue) {
    auto cppWrapper = reinterpret_cast<CppWrapper *>(JS_GetOpaque(cppWrapperValue, js_cppwrapper_class_id));
    return reinterpret_cast<T *>(cppWrapper->ptr);
  }

  // Wrap a C++ instance inside an existing JSValue as a new map entry with the given key.
  //
  // The existingValue will have the following structure:
  // {
  //   __cpp_object_map: {
  //     key1: [wrapped C++ instance 1]
  //     key2: [wrapped C++ instance 2]
  //     ...
  //   }
  //   ...
  // }
  template <class T>
  void createMappedCppPtrValue(T *obj, JSValueConst jsValue, const char *key) const {
    // Get or create CPP object map
    JSValue cppObjectMapValue = JS_GetPropertyStr(m_ctx, jsValue, CPP_OBJECT_MAP_PROP_NAME);
    if (JS_IsUndefined(cppObjectMapValue)) {
      cppObjectMapValue = JS_NewObject(m_ctx);
      JS_SetPropertyStr(m_ctx, jsValue, CPP_OBJECT_MAP_PROP_NAME, JS_DupValue(m_ctx, cppObjectMapValue));
    }

    // Store it in jsValue.cppObjectMap["lambda_global_name"]
    auto cppValue = createCppPtrValue<T>(obj, true /*deleteOnFinalize*/);
    JS_SetPropertyStr(m_ctx, cppObjectMapValue, key, cppValue);
    // No JS_FreeValue(m_ctx, cppValue) after JS_SetPropertyStr

    JS_FreeValue(m_ctx, cppObjectMapValue);
  }

  // Access a C++ instance mapped inside a JSValue via createMappedCppPtrValue()
  template <class T>
  T *getMappedCppPtrValue(JSValueConst jsValue, const char *key) const {
    // Get CPP object map
    JSValue cppObjectMapValue = JS_GetPropertyStr(m_ctx, jsValue, CPP_OBJECT_MAP_PROP_NAME);
    if (JS_IsUndefined(cppObjectMapValue)) {
      JS_FreeValue(m_ctx, cppObjectMapValue);
      return nullptr;
    }

    // Get C++ JavaScriptObject instance stored in jsObject.cppObjectMap["lambda_global_name"]
    JSValue cppJsObjectValue = JS_GetPropertyStr(m_ctx, cppObjectMapValue, key);
    T *cppObject = nullptr;
    if (JS_IsObject(cppJsObjectValue)) {
      cppObject = getCppPtr<T>(cppJsObjectValue);
    }
    JS_FreeValue(m_ctx, cppJsObjectValue);

    return cppObject;
  }

  // Wrap a JNI ref inside a new JSValue and ensure that it's properly
  // released when the JSValue gets finalized
  template <class T>
  JSValue createJavaRefValue(const JniRef<T> &ref) const {
    JSValue cppWrapperObj = JS_NewObjectClass(m_ctx, js_cppwrapper_class_id);

    auto globalRefPtr = new JniGlobalRef<T>(ref);

    auto deleter = [globalRefPtr]() {
      delete globalRefPtr;
    };

    auto cppWrapper = new CppWrapper { globalRefPtr, deleter };
    JS_SetOpaque(cppWrapperObj, cppWrapper);

    return cppWrapperObj;
  }

  // Access a JNI ref wrapped in a JSValue via createJavaRefValue()
  template <class T>
  JniLocalRef<T> getJavaRef(JSValueConst v) const {
    auto cppWrapper = reinterpret_cast<CppWrapper *>(JS_GetOpaque(v, js_cppwrapper_class_id));
    auto globalRefPtr = reinterpret_cast<JniGlobalRef<T> *>(cppWrapper->ptr);

    return JniLocalRef<T>(*globalRefPtr);
  }

public:  // internal
  typedef struct {
    void *ptr;
    std::function<void()> deleter;
  } CppWrapper;

  static JSClassID js_finalizer_class_id;
  static JSClassID js_cppwrapper_class_id;

private:
  const JniContext *m_jniContext;
  JSContext *m_ctx;
};

#endif
