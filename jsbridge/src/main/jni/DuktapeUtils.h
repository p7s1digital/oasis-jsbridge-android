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
#ifndef _JSBRIDGE_DUKTAPE_UTILS_H
#define _JSBRIDGE_DUKTAPE_UTILS_H

#include "StackChecker.h"
#include <functional>
#include <duktape/duktape.h>

static const char *CPP_WRAPPER_PROP_NAME = "__cpp_wrapper";
static const char *CPP_OBJECT_MAP_PROP_NAME = "__cpp_object_map";

class JniContext;

class DuktapeUtils {

public:
  DuktapeUtils() = delete;
  DuktapeUtils(const DuktapeUtils &) = delete;
  DuktapeUtils &operator=(const DuktapeUtils &) = delete;

  DuktapeUtils(const JniContext *, duk_context *);

  // Wrap a C++ instance inside a new JSValue and (optionally) ensure that it is
  // deleted when the JSValue gets finalized
  template <class T>
  void pushCppPtrValue(T *obj, bool deleteOnFinalize) const {
    CHECK_STACK_OFFSET(m_ctx, 1);

    auto deleter = [deleteOnFinalize, obj]() {
      if (deleteOnFinalize) {
        delete obj;
      }
    };

    duk_push_object(m_ctx);
    duk_push_pointer(m_ctx, new CppWrapper { obj, deleter });
    duk_put_prop_string(m_ctx, -2, CPP_WRAPPER_PROP_NAME);

    if (deleteOnFinalize) {
      duk_push_c_function(m_ctx, &DuktapeUtils::cppWrapperFinalizer, 1);
      duk_set_finalizer(m_ctx, -2);
    }
  }

  // Access the instance wrapped at the given index via createCppPtrValue()
  template <class T>
  T *getCppPtr(duk_idx_t index) const {
    CHECK_STACK(m_ctx);

    if (!duk_get_prop_string(m_ctx, index, CPP_WRAPPER_PROP_NAME)) {
      duk_pop(m_ctx);
      return nullptr;
    }

    auto cppWrapper = reinterpret_cast<CppWrapper *>(duk_require_pointer(m_ctx, -1));
    duk_pop(m_ctx);  // CPP wrapper
    return reinterpret_cast<T *>(cppWrapper->ptr);
  }

  // Wrap a C++ instance at a given index as a new map entry with the given key.
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
  void createMappedCppPtrValue(T *obj, duk_idx_t index, const char *key) const {
    CHECK_STACK(m_ctx);

    if (!duk_is_object(m_ctx, index) || duk_is_null(m_ctx, index)) {
      throw std::runtime_error("Cannot create a mapped CPP pointer value: not an object!");
    }

    index = duk_normalize_index(m_ctx, index);

    // Get or create CPP object map
    if (!duk_get_prop_string(m_ctx, index, CPP_OBJECT_MAP_PROP_NAME)) {
      duk_pop(m_ctx);
      duk_push_object(m_ctx);
      duk_dup(m_ctx, -1);
      duk_put_prop_string(m_ctx, index, CPP_OBJECT_MAP_PROP_NAME);
    }

    // Store it in object_at_index.cppObjectMap["lambda_global_name"]
    pushCppPtrValue<T>(obj, true /*deleteOnFinalize*/);
    duk_put_prop_string(m_ctx, -2, key);

    duk_pop(m_ctx);  // CPP object map
  }

  // Access a C++ instance mapped inside a JSValue via createMappedCppPtrValue()
  template <class T>
  T *getMappedCppPtrValue(duk_idx_t index, const char *key) const {
    CHECK_STACK(m_ctx);

    // Get CPP object map
    if (!duk_get_prop_string(m_ctx, index, CPP_OBJECT_MAP_PROP_NAME)) {
      duk_pop(m_ctx);  // undefined CPP object map
      return nullptr;
    }

    // Get C++ JavaScriptObject instance stored in jsObject.cppObjectMap["lambda_global_name"]
    T *cppObject = nullptr;
    if (duk_get_prop_string(m_ctx, -1, key)) {
      cppObject = getCppPtr<T>(-1);
    }

    duk_pop_2(m_ctx);  // CPP object wrapper + CPP object map
    return cppObject;
  }

  // Wrap a JNI ref inside a new JSValue and ensure that it's properly
  // released when the JSValue gets finalized
  template <class T>
  void pushJavaRefValue(const JniRef<T> &ref) const {
    CHECK_STACK_OFFSET(m_ctx, 1);

    auto globalRefPtr = new JniGlobalRef<T>(ref);
    pushCppPtrValue(globalRefPtr, true);
  }

  // Access a JNI ref wrapped in a JSValue via createJavaRefValue()
  template <class T>
  JniLocalRef<T> getJavaRef(duk_idx_t index) const {
    auto globalRefPtr = getCppPtr<JniGlobalRef<T>>(index);
    return JniLocalRef<T>(*globalRefPtr);
  }

private:
  typedef struct {
    void *ptr;
    std::function<void()> deleter;
  } CppWrapper;

  static duk_ret_t cppWrapperFinalizer(duk_context *);

  const JniContext *m_jniContext;
  duk_context *m_ctx;
};

#endif
