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
#ifndef _JSBRIDGE_JNIREFHELPER_H
#define _JSBRIDGE_JNIREFHELPER_H

#include <jni.h>

class JniContext;

class JniRefHelper {
public:
  JniRefHelper() = delete;
  JniRefHelper(const JniRefHelper &) = delete;
  JniRefHelper &operator=(const JniRefHelper &) = delete;

  ~JniRefHelper() = delete;

  static JNIEnv *getJNIEnv(const JniContext *);
};

#endif
