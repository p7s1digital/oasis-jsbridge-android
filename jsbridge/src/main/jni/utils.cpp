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
#include "utils.h"
#include <android/log.h>

#define LOG_TAG "JsBridgeJni"

#define _JSBRIDGE_ALOG_HELPER(priority, format) \
  va_list args;\
  va_start(args, format);\
  __android_log_vprint(priority, LOG_TAG, format, args);\
  va_end(args);

void alog(const char *format, ...) {
  _JSBRIDGE_ALOG_HELPER(ANDROID_LOG_DEBUG, format);
}

void alog_info(const char *format, ...) {
  _JSBRIDGE_ALOG_HELPER(ANDROID_LOG_INFO, format);
}

void alog_warn(const char *format, ...) {
  _JSBRIDGE_ALOG_HELPER(ANDROID_LOG_WARN, format);
}

void alog_error(const char *format, ...) {
  _JSBRIDGE_ALOG_HELPER(ANDROID_LOG_ERROR, format);
}

void alog_fatal(const char *format, ...) {
  _JSBRIDGE_ALOG_HELPER(ANDROID_LOG_FATAL, format);
}

void backtraceToLogcat(JNIEnv *env) {
#ifndef NDEBUG
  // Abuse the JNI to produce an error with stack trace in debug mode (see https://stackoverflow.com/a/35586148)
  env->FindClass(NULL);
#endif
}