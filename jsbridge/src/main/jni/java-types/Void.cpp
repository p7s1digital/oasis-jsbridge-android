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

#ifdef DUKTAPE
# include "JsBridgeContext.h"
#endif

namespace JavaTypes {

Void::Void(const JsBridgeContext *jsBridgeContext, const JniGlobalRef<jclass> &classRef, bool pushUndefined, bool popObject)
 : JavaType(jsBridgeContext, classRef)
 , m_pushUndefined(pushUndefined)
 , m_useJavaClassAsRetVal(popObject) {
}

#if defined(DUKTAPE)

JValue Void::pop(bool, const AdditionalData *) const {
  duk_pop(m_ctx);

  if (m_useJavaClassAsRetVal) {
    jmethodID newInstance = getJniContext()->getMethodID(getClass(), "<init>", "()V");
    auto instance = getJniContext()->newObject<jthrowable>(getClass(), newInstance);
    return JValue(instance);
  }

  return JValue();
}

duk_ret_t Void::push(const JValue &, bool inScript, const AdditionalData *) const {
  if (m_pushUndefined) {
    duk_push_undefined(m_ctx);
    return 1;
  }

  return 0;
}

#elif defined(QUICKJS)

JValue Void::toJava(JSValueConst v, bool, const AdditionalData *) const {
  if (m_useJavaClassAsRetVal) {
    jmethodID newInstance = getJniContext()->getMethodID(getClass(), "<init>", "()V");
    auto instance = getJniContext()->newObject<jthrowable>(getClass(), newInstance);
    return JValue(instance);
  }

  return JValue();
}

JSValue Void::fromJava(const JValue &, bool inScript, const AdditionalData *) const {
  return JS_UNDEFINED;
}

#endif

JValue Void::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                        const std::vector<JValue> &args) const {

  if (m_useJavaClassAsRetVal) {
    m_jniContext->callObjectMethodA<jobject>(javaThis, methodId, args);
  } else {
    m_jniContext->callVoidMethodA(javaThis, methodId, args);
  }
  m_jsBridgeContext->checkRethrowJsError();

  // Explicitly release all values now because they won't be used afterwards
  JValue::releaseAll(args);

  return JValue();
}

}  // namespace JavaTypes

