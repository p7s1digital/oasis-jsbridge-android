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
#ifndef _JSBRIDGE_JNIVALUECONVERTER_H
#define _JSBRIDGE_JNIVALUECONVERTER_H

#include "JniRef.h"
#include "JValue.h"
#include <jni.h>
#include <utility>

namespace JniValueConverter {
  // Explicitly define conversion for supported types (forbid jstring, jobject, ...)
  static jboolean toJniValue(jboolean v) { return v; }
  static jint toJniValue(jint v) { return v; }
  static jlong toJniValue(jlong v) { return v; }
  static jfloat toJniValue(jfloat v) { return v; }
  static jdouble toJniValue(jdouble v) { return v; }

  // nullptr to null jobject
  static jobject toJniValue(std::nullptr_t) { return nullptr; }

  // JniRef to jobject
  template<typename JniT>
  static JniT toJniValue(const JniRef<JniT> &ref) {
      return ref.get();
  }

  // JValue to jobject
  static jobject toJniValue(const JValue &value) {
      return value.get().l;
  }

  // Call toJniValue for each item of the variadic values
  static auto toJniValues = [](auto &&variadicValues) {
    return toJniValue(std::forward<decltype(variadicValues)>(variadicValues));
  };
}

#endif
