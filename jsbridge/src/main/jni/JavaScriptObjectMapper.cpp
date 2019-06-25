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
#include "JavaScriptObjectMapper.h"
#include "JavaScriptObjectBase.h"
#include "StackChecker.h"


// Internal
// ---

namespace {
  // Internal names used for properties in a proxied JS object.
  // The \xff\xff part keeps the variable hidden from JS (visible through C API only).

  // We stuff JavaScriptObject pointers into an array and attach it to the proxied instance so we are
  // able to detach our local reference to the object when it is garbage collected in the JS VM.
  const char *CPP_OBJECT_MAP_PROP_NAME = "\xff\xff_cpp_object_map";
}

void JavaScriptObjectMapper::add(duk_context *ctx, const std::string &globalName, std::function<JavaScriptObjectBase *(void *jsHeapPtr)> &&cppObjectFactory) const {

  CHECK_STACK(ctx);

  // Get the global JS object
  if (!duk_get_global_string(ctx, globalName.c_str())) {
    duk_pop(ctx);  // (undefined) JS object
    throw std::invalid_argument("A global JS object called " + globalName + " was not found");
  }

  // Get the JS heap ptr
  void *jsHeapPtr = duk_get_heapptr(ctx, -1);
  if (jsHeapPtr == nullptr) {
    duk_pop(ctx);  // (undefined) JS object
    throw std::invalid_argument("JS global called " + globalName + " is not an object");
  }

  // Get or create the "cpp object map"
  if (!duk_get_prop_string(ctx, -1, CPP_OBJECT_MAP_PROP_NAME)) {
    duk_pop(ctx);  // undefined "cpp object map"

    // Create the new "cpp object map" property
    duk_push_object(ctx);
    duk_dup(ctx, -1);  // duplicate the above object (because it will be popped in the next line)
    duk_put_prop_string(ctx, -3, CPP_OBJECT_MAP_PROP_NAME);

    // Set the finalizer of the "cpp object map" to perform the "cpp object" cleanup
    duk_push_c_function(ctx, JavaScriptObjectMapper::cppObjectMapFinalizer, 1);
    duk_set_finalizer(ctx, -2);
  }

  if (duk_has_prop_string(ctx, -1, globalName.c_str())) {
    // The cpp wrapper with the given global name already exists: do not add it again
    duk_pop_2(ctx);  // cpp object map + JS object
    return;
  }

  // Create the C++ object via the factory
  JavaScriptObjectBase *cppObject;
  try {
    cppObject = cppObjectFactory(jsHeapPtr);
  } catch (const std::runtime_error &e) {
    duk_pop_2(ctx);  // cpp object map + JS object
    throw e;
  }

  // Attach the C++ object
  duk_push_pointer(ctx, cppObject);
  duk_put_prop_string(ctx, -2, globalName.c_str());

  duk_pop_2(ctx);  // cpp object map + JS object
}

JavaScriptObjectBase *JavaScriptObjectMapper::get(duk_context *ctx, const std::string &globalName) const {

  CHECK_STACK(ctx);

  // Get the JS object
  if (!duk_get_global_string(ctx, globalName.c_str())) {
    duk_pop(ctx);  // (undefined) JS object
    return nullptr;
  }

  // Get the C++ object instance
  if (!duk_get_prop_string(ctx, -1, CPP_OBJECT_MAP_PROP_NAME)) {
    duk_pop_2(ctx);  // (undefined) cpp object map + JS object
    return nullptr;
  }
  if (!duk_get_prop_string(ctx, -1, globalName.c_str())) {
    duk_pop_3(ctx);  // (undefined) this + cpp object map + JS object
    return nullptr;
  }

  auto cppObject = reinterpret_cast<JavaScriptObjectBase *>(duk_require_pointer(ctx, -1));

  duk_pop_3(ctx);  // C++ object + cpp object map + JS object
  return cppObject;
}

// static
duk_ret_t JavaScriptObjectMapper::cppObjectMapFinalizer(duk_context *ctx) {
  // For each 'cppObject' instance wrapped in the map
  duk_enum(ctx, -1, DUK_ENUM_INCLUDE_HIDDEN | DUK_ENUM_OWN_PROPERTIES_ONLY);
  while (duk_next(ctx, -1, 1 /*getValue*/)) {
    auto cppObject = reinterpret_cast<JavaScriptObjectBase *>(duk_get_pointer(ctx, -1));
    if (cppObject == nullptr) {
      duk_pop_2(ctx);  // enum key + value
      continue;
    }

    delete cppObject;

    duk_pop_2(ctx);  // enum key + value
  }

  return 0;
}
