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
#include "Void.h"
#include "../JsBridgeContext.h"

namespace JavaTypes {

Void::Void(const JsBridgeContext *jsBridgeContext, JavaTypeId id, bool boxed)
 : JavaType(jsBridgeContext, id)
 , m_boxed(boxed) {
}

#if defined(DUKTAPE)

JValue Void::pop(bool) const {
  duk_pop(m_ctx);

  if (m_boxed) {
    // Create and return a new Void or Unit instance (TODO: cache me!)
    const auto &javaClass = getJavaClass();
    jmethodID newInstance = getJniContext()->getMethodID(getJavaClass(), "<init>", "()V");
    auto instance = getJniContext()->newObject<jthrowable>(getJavaClass(), newInstance);
    return JValue(instance);
  }

  return JValue();
}

JValue Void::popArray(uint32_t count, bool expanded, bool inScript) const {
  expanded ? duk_pop_n(m_ctx, count) : duk_pop(m_ctx);

  const char *message = "Cannot pop an array of Void values!";
  m_jsBridgeContext->throwTypeException(message, inScript);

  // Unreachable
  return JValue();
}

duk_ret_t Void::push(const JValue &, bool /*inScript*/) const {
  if (m_boxed) {
    // If void is boxed, it is actually a value which must be pushed
    duk_push_undefined(m_ctx);
    return 1;
  }

  return 0;
}

duk_ret_t Void::pushArray(const JniLocalRef<jarray> &, bool /*expand*/, bool inScript) const {
  const char *message = "Cannot push an array of Void values!";
  m_jsBridgeContext->throwTypeException(message, inScript);

  // Unreachable
  return DUK_RET_ERROR;
}

#elif defined(QUICKJS)

JValue Void::toJava(JSValueConst, bool) const {
  if (m_boxed) {
    // Create and return a new Void or Unit instance
    const auto &javaClass = getJavaClass();
    jmethodID newInstance = getJniContext()->getMethodID(getJavaClass(), "<init>", "()V");
    auto instance = getJniContext()->newObject<jthrowable>(getJavaClass(), newInstance);
    return JValue(instance);
  }

  return JValue();
}

JValue Void::toJavaArray(JSValueConst, bool inScript) const {
  const char *message = "Cannot transfer from JS to Java an array of Void values";
  m_jsBridgeContext->throwTypeException(message, inScript);

  // Unreachable
  return JValue();
}

JSValue Void::fromJava(const JValue &, bool inScript) const {
  return JS_UNDEFINED;
}

JSValue Void::fromJavaArray(const JniLocalRef<jarray> &, bool inScript) const {
  const char *message = "Cannot transfer from Java to JS an array of Void values!";
  m_jsBridgeContext->throwTypeException(message, inScript);

  return JS_EXCEPTION;
}

#endif

JValue Void::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                        const std::vector<JValue> &args) const {
  if (m_boxed) {
    m_jniContext->callObjectMethodA<jobject>(javaThis, methodId, args);
  } else {
    m_jniContext->callVoidMethodA(javaThis, methodId, args);
  }

  // Explicitly release all values now because they won't be used afterwards
  JValue::releaseAll(args);

  if (m_jsBridgeContext->hasPendingJniException()) {
    m_jsBridgeContext->rethrowJniException();
  }

  return JValue();
}

}  // namespace JavaTypes

