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
#include "JavaType.h"

#include "JsBridgeContext.h"
#include "StackUnwinder.h"
#include "utils.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include "jni-helpers/JValue.h"
#include "jni-helpers/JniLocalRef.h"
#include <string>
#include <vector>

using namespace JavaTypes;

JavaType::JavaType(const JsBridgeContext *jsBridgeContext, JniGlobalRef<jclass> classRef)
 : m_jsBridgeContext(jsBridgeContext)
 , m_jniContext(m_jsBridgeContext->jniContext())
 , m_ctx(m_jsBridgeContext->getCContext())
 , m_classRef(std::move(classRef)) {
}

JValue JavaType::popArray(uint32_t count, bool expanded, bool inScript, const AdditionalData *additionalData) const {
  // If we're not expanded, pop the array off the stack no matter what.
  const StackUnwinder _(m_ctx, expanded ? 0 : 1);
  count = expanded ? count : duk_get_length(m_ctx, -1);

  JObjectArrayLocalRef objectArray(m_jniContext, count, m_classRef);

  for (int i = count - 1; i >= 0; --i) {
    if (!expanded) {
      duk_get_prop_index(m_ctx, -1, i);
    }
    JValue elementValue = pop(inScript, additionalData);
    const JniLocalRef<jobject> &jElement = elementValue.getLocalRef();
    objectArray.setElement(i, jElement);
    m_jsBridgeContext->checkRethrowJsError();
  }

  return JValue(objectArray);
}

duk_ret_t JavaType::pushArray(const JniLocalRef<jarray>& values, bool expand, bool inScript, const AdditionalData *additionalData) const {
  JObjectArrayLocalRef objectArray(values.staticCast<jobjectArray>());
  const auto size = objectArray.getLength();

  if (!expand) {
    duk_push_array(m_ctx);
  }

  for (jsize i = 0; i < size; ++i) {
    JniLocalRef<jobject> object = objectArray.getElement(i);
    try {
      push(JValue(object), inScript, additionalData);
      if (!expand) {
        duk_put_prop_index(m_ctx, -2, static_cast<duk_uarridx_t>(i));
      }
    } catch (std::invalid_argument &e) {
      duk_pop_n(m_ctx, expand ? i : 1);
      throw e;
    }
  }
  return expand ? size : 1;
}

JniLocalRef<jclass> JavaType::getArrayClass() const {
  return m_jniContext->getObjectClass(JObjectArrayLocalRef(m_jniContext, 0, getClass()));
}

JValue JavaType::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis,
                            const std::vector<JValue> &args) const {

  JniLocalRef<jobject> returnValue = m_jniContext->callObjectMethodA(javaThis, methodId, args);
  m_jsBridgeContext->checkRethrowJsError();

  // Release all values now because they won't be used afterwards
  JValue::releaseAll(args);

  return JValue(returnValue);
}

