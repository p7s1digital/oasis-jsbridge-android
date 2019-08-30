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
#include "DuktapeUtils.h"

DuktapeUtils::DuktapeUtils(const JniContext *jniContext, duk_context *ctx)
 : m_jniContext(jniContext)
 , m_ctx(ctx) {
}

// static
duk_ret_t DuktapeUtils::cppWrapperFinalizer(duk_context *ctx) {
  CHECK_STACK(ctx);

  duk_get_prop_string(ctx, 0, CPP_WRAPPER_PROP_NAME);
  auto cppWrapper = reinterpret_cast<CppWrapper *>(duk_require_pointer(ctx, -1));
  cppWrapper->deleter();

  duk_pop(ctx);  // CPP wrapper
  return 0;
}

