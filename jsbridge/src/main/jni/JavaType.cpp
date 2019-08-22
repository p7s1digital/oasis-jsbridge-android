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
#include "JavaTypeProvider.h"
#include "java-types/Deferred.h"
#include "java-types/FunctionX.h"
#include "java-types/JsValue.h"
#include "java-types/JsonObjectWrapper.h"
#include "java-types/Object.h"
#include "jni-helpers/JArrayLocalRef.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include "jni-helpers/JStringLocalRef.h"
#include "jni-helpers/JValue.h"
#include "jni-helpers/JniLocalRef.h"
#include <string>
#include <vector>

using namespace JavaTypes;

JavaType::JavaType(const JsBridgeContext *jsBridgeContext, JavaTypeId id)
 : m_jsBridgeContext(jsBridgeContext)
 , m_jniContext(jsBridgeContext->jniContext())
 , m_ctx(m_jsBridgeContext->getCContext())
 , m_id(id) {
}

const JniRef<jclass> &JavaType::getJavaClass() const {
 return m_jsBridgeContext->getJavaTypeProvider().getJavaClass(m_id) ;
}

#if defined(DUKTAPE)

#include "StackUnwinder.h"

JValue JavaType::popArray(uint32_t count, bool expanded, bool inScript) const {
 // If we're not expanded, pop the array off the stack no matter what.
 const StackUnwinder _(m_ctx, expanded ? 0 : 1);
 count = expanded ? count : duk_get_length(m_ctx, -1);

 JObjectArrayLocalRef objectArray(m_jniContext, count, getJavaClass());

 for (int i = count - 1; i >= 0; --i) {
  if (!expanded) {
   duk_get_prop_index(m_ctx, -1, static_cast<duk_uarridx_t>(i));
  }
  JValue elementValue = pop(inScript);
  const JniLocalRef<jobject> &jElement = elementValue.getLocalRef();
  objectArray.setElement(i, jElement);
  m_jsBridgeContext->checkRethrowJsError();
 }

 return JValue(objectArray);
}

duk_ret_t JavaType::pushArray(const JniLocalRef<jarray>& values, bool expand, bool inScript) const {
 JObjectArrayLocalRef objectArray(values.staticCast<jobjectArray>());
 const auto size = objectArray.getLength();

 if (!expand) {
  duk_push_array(m_ctx);
 }

 for (jsize i = 0; i < size; ++i) {
  JniLocalRef<jobject> object = objectArray.getElement(i);
  try {
   push(JValue(object), inScript);
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

#elif defined(QUICKJS)

#include "QuickJsUtils.h"

JValue JavaType::toJavaArray(JSValueConst jsValue, bool inScript) const {
  if (JS_IsNull(jsValue) || JS_IsUndefined(jsValue)) {
    return JValue();
  }

  JSValue lengthValue = JS_GetPropertyStr(m_ctx, jsValue, "length");
  assert(JS_IsNumber(lengthValue));
  uint32_t count = JS_VALUE_GET_INT(lengthValue);
  JS_FreeValue(m_ctx, lengthValue);

  JObjectArrayLocalRef objectArray(m_jniContext, count, getJavaClass());

  assert(JS_IsArray(m_ctx, jsValue));
  for (uint32_t i = 0; i < count; ++i) {
    JSValue elementJsValue = JS_GetPropertyUint32(m_ctx, jsValue, i);
    JValue elementJavaValue = toJava(elementJsValue, inScript);
    JS_FreeValue(m_ctx, elementJsValue);
    const JniLocalRef<jobject> &jElement = elementJavaValue.getLocalRef();
    objectArray.setElement(i, jElement);
    m_jsBridgeContext->checkRethrowJsError();
  }

  return JValue(objectArray);
}

JSValue JavaType::fromJavaArray(const JniLocalRef<jarray>& values, bool inScript) const {
  JObjectArrayLocalRef objectArray(values.staticCast<jobjectArray>());
  const auto size = objectArray.getLength();

  JSValue jsArray = JS_NewArray(m_ctx);

  for (jsize i = 0; i < size; ++i) {
    JniLocalRef<jobject> object = objectArray.getElement(i);
    try {
      JSValue elementValue = fromJava(JValue(object), inScript);
      JS_SetPropertyUint32(m_ctx, jsArray, static_cast<uint32_t>(i), elementValue);
    } catch (std::invalid_argument &e) {
      JS_FreeValue(m_ctx, jsArray);
      throw e;
    }
  }

  return jsArray;
}

#endif

JValue JavaType::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis, const std::vector<JValue> &args) const {

 JniLocalRef<jobject> returnValue = m_jniContext->callObjectMethodA(javaThis, methodId, args);
 m_jsBridgeContext->checkRethrowJsError();

 // Release all values now because they won't be used afterwards
 JValue::releaseAll(args);

 return JValue(returnValue);
}

