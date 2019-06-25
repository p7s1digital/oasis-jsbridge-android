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
#ifndef JSBRIDGE_JNIREF_H
#define JSBRIDGE_JNIREF_H

#include <jni.h>

class JniContext;

// Common functionalities of JniLocalRef and JniGlobalRef.
template <class T>
class JniRef {
public:
  virtual ~JniRef() = default;

  virtual const JniContext *getJniContext() const = 0;

  virtual T get() const = 0;
  virtual T toNewRawLocalRef() const = 0;
  virtual T toNewRawGlobalRef() const = 0;
  virtual T toNewRawWeakGlobalRef() const = 0;
  virtual bool isNull() const = 0;

  //virtual operator T () const = 0;

  // Explicitly disable a dangerous cast to bool
  explicit operator bool() const = delete;

protected:
  JniRef() = default;
};

template<typename T>
struct is_jniref {
  static const bool value = false;
};

template<typename RefT>
struct is_jniref<JniRef<RefT>>{
  static const bool value = true;
};

#endif
