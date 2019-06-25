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
#ifndef _JSBRIDGE_STACK_UNWINDER_H
#define _JSBRIDGE_STACK_UNWINDER_H

#include "duktape/duktape.h"

// Use RAII to unwind the JS stack when a function returns.
class StackUnwinder {
public:
  StackUnwinder(duk_context* ctx, int count)
      : m_context(ctx)
      , m_count(count) {}

  StackUnwinder(const StackUnwinder &) = delete;
  StackUnwinder& operator=(const StackUnwinder &) = delete;

  ~StackUnwinder() {
    duk_pop_n(m_context, m_count);
  }

private:
  duk_context *m_context;
  const int m_count;
};

#endif

