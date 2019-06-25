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
#ifndef _JSBRIDGE_JAVASCRIPTOBJECTMAP_H
#define _JSBRIDGE_JAVASCRIPTOBJECTMAP_H

#include "duktape/duktape.h"
#include "utils.h"
#include <functional>
#include <string>

class JavaScriptObjectBase;

// Manage C++ wrapper instances (JavaScriptObject, JavaScriptLambda)
// - add C++ wrapper of a JS object with a given (global) name
// - provide access to the C++ wrapper by the name of the JS object
// - properly delete the C++ wrapper instance when the JS object is finalized
class JavaScriptObjectMapper {
public:
  JavaScriptObjectMapper() = default;

  JavaScriptObjectMapper(const JavaScriptObjectMapper&) = delete;
  JavaScriptObjectMapper& operator=(const JavaScriptObjectMapper&) = delete;

  void add(duk_context *, const std::string &globalName, std::function<JavaScriptObjectBase *(void *jsHeapPtr)> &&cppObjectFactory) const;
  JavaScriptObjectBase *get(duk_context *, const std::string &globalName) const;

private:
  static duk_ret_t cppObjectMapFinalizer(duk_context *);
};

#endif
