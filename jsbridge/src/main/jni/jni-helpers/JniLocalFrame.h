/*
 * Copyright (C) 2019 ProSiebenSat1.Digital GmbH.
 *
 * Originally based on Duktape Android:
 * Copyright (C) 2015 Square, Inc.
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
#ifndef _JSBRIDGE_LOCALFRAME_H
#define _JSBRIDGE_LOCALFRAME_H

#include "JniContext.h"
#include <jni.h>
#include <memory>

class JniContext;

// RAII wrapper that allocates a new local reference frame for the JVM and releases it when leaving
// scope.
struct JniLocalFrame {
  JniLocalFrame(JniContext *jniContext, std::size_t capacity)
      : m_env(jniContext->getJNIEnv()) {

    assert(m_env);

    jint ret = m_env->PushLocalFrame(capacity);
    ++counter;
    if (ret) {
      // Out of memory.
      throw std::bad_alloc();
    }
  }

  ~JniLocalFrame() {
    m_env->PopLocalFrame(nullptr);
    --counter;
  }

  // No copying allowed.
  JniLocalFrame(const JniLocalFrame &) = delete;
  JniLocalFrame& operator=(const JniLocalFrame &) = delete;

private:
  static int counter;
  JNIEnv *m_env;
};

#endif
