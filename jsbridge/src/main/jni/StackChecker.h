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
#ifndef _JSBRIDGE_STACK_CHECKER_H
#define _JSBRIDGE_STACK_CHECKER_H

#include "duktape/duktape.h"
#include "utils.h"
#include <string>
#include <stdexcept>

// Throws an exception and aborts the process if the Duktape stack has a different number of
// elements at the end of the C++ scope than where the object was constructed.  This will show a
// trace and Duktape stack details in logcat when running in the debugger. Use CHECK_STACK()
// below for a convenient way to add stack validation to debug builds only.
class StackChecker {
public:
  explicit StackChecker(duk_context *ctx, int offset)
    : m_context(ctx)
    , m_offset(offset)
    , m_top(duk_get_top(m_context)) {
  }

// TODO: do not throw here but provides a more appropriate mechanism to forward the exception!
  ~StackChecker() {
    if (m_top + m_offset == duk_get_top(m_context)) {
      return;
    }
    const auto actual = duk_get_top(m_context);
    try {
      duk_push_context_dump(m_context);
      throw stack_error(m_top, actual, duk_get_string(m_context, -1));
    } catch (const std::exception &e) {  // TODO
      throw stack_error(m_top, actual, "<no stack info>");
    }
  }

private:
  struct stack_error : public std::runtime_error {
    stack_error(duk_idx_t expected, duk_idx_t actual, const std::string& stack)
      : std::runtime_error("expected: " + std::to_string(expected) +
                           ", actual: " + std::to_string(actual) +
                           " - stack: " + stack) {
    }
  };

  duk_context* m_context;
  int m_offset;
  const duk_idx_t m_top;
};

#ifdef NDEBUG
#define CHECK_STACK(ctx) ((void)0)
#else
#define CHECK_STACK(ctx) const StackChecker _(ctx, 0)
#define CHECK_STACK_OFFSET(ctx, offset) const StackChecker _(ctx, offset)
#endif

#endif
