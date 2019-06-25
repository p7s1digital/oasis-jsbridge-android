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
#ifndef _JSBRIDGE_QUICKJS_CONSOLE_H
#define _JSBRIDGE_QUICKJS_CONSOLE_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "quickjs/quickjs.h"

extern void quickjs_console_init(JSContext *);

#if defined(__cplusplus)
}
#endif

#endif
