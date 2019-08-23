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
#ifndef _JSBRIDGE_UTILS_H
#define _JSBRIDGE_UTILS_H

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

void alog(const char *format, ...);
void alog_info(const char *format, ...);
void alog_warn(const char *format, ...);
void alog_error(const char *format, ...);
void alog_fatal(const char *format, ...);

void backtraceToLogcat(JNIEnv *);

#ifdef __cplusplus
}
#endif

#endif
