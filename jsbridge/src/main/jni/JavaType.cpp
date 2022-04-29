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
#include "JniCache.h"
#include "exceptions/JniException.h"
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
#include "log.h"
#include <string>
#include <vector>

using namespace JavaTypes;

JavaType::JavaType(const JsBridgeContext *jsBridgeContext, JavaTypeId id)
 : m_jsBridgeContext(jsBridgeContext)
 , m_jniContext(jsBridgeContext->getJniContext())
#if defined(DUKTAPE)
 , m_ctx(m_jsBridgeContext->getDuktapeContext())
#elif defined(QUICKJS)
 , m_ctx(m_jsBridgeContext->getQuickJsContext())
#endif
 , m_id(id) {
}

JniLocalRef<jclass> JavaType::getJavaClass() const {
 return m_jsBridgeContext->getJniCache()->getJavaClass(m_id) ;
}

#if defined(DUKTAPE)

#include "StackChecker.h"

JValue JavaType::popArray(uint32_t count, bool expanded) const {
 CHECK_STACK_OFFSET(m_ctx, 0 - count);

 count = expanded ? count : duk_get_length(m_ctx, -1);

 JObjectArrayLocalRef objectArray(m_jniContext, count, getJavaClass());
 if (objectArray.isNull()) {
  duk_pop_n(m_ctx, expanded ? count : 1);  // pop the expanded elements or the array
  throw JniException(m_jniContext);
 }

 for (int i = count - 1; i >= 0; --i) {
  if (!expanded) {
    duk_get_prop_index(m_ctx, -1, static_cast<duk_uarridx_t>(i));
  }
  JValue elementValue = pop();
  const JniLocalRef<jobject> &jElement = elementValue.getLocalRef();
  objectArray.setElement(i, jElement);

  if (m_jniContext->exceptionCheck()) {
    duk_pop_n(m_ctx, expanded ? std::max(i, 0) : 1);  // pop remaining expanded elements or array
    throw JniException(m_jniContext);
  }
 }

 if (!expanded) {
   duk_pop(m_ctx);  // pop the array
 }

 return JValue(objectArray);
}

duk_ret_t JavaType::pushArray(const JniLocalRef<jarray>& values, bool expand) const {
 JObjectArrayLocalRef objectArray(values.staticCast<jobjectArray>());
 const auto count = objectArray.getLength();

 CHECK_STACK_OFFSET(m_ctx, expand ? count : 1);

 if (!expand) {
  duk_push_array(m_ctx);
 }

 for (jsize i = 0; i < count; ++i) {
  JniLocalRef<jobject> object = objectArray.getElement(i);
  try {
   push(JValue(object));
   if (!expand) {
     duk_put_prop_index(m_ctx, -2, static_cast<duk_uarridx_t>(i));
   }
  } catch (const std::exception &) {
    duk_pop_n(m_ctx, expand ? i : 1);  // pop expanded elements which have been pushed or array
    throw;
  }
 }

 return expand ? count : 1;
}

#elif defined(QUICKJS)

#include "QuickJsUtils.h"

JValue JavaType::toJavaArray(JSValueConst jsValue) const {
  if (JS_IsNull(jsValue) || JS_IsUndefined(jsValue)) {
    return JValue();
  }

  JSValue lengthValue = JS_GetPropertyStr(m_ctx, jsValue, "length");
  assert(JS_IsNumber(lengthValue));
  uint32_t count = JS_VALUE_GET_INT(lengthValue);
  JS_FreeValue(m_ctx, lengthValue);

  JObjectArrayLocalRef objectArray(m_jniContext, (jsize) count, getJavaClass());
  if (objectArray.isNull()) {
    throw JniException(m_jniContext);
  }

  assert(JS_IsArray(m_ctx, jsValue));
  for (uint32_t i = 0; i < count; ++i) {
    JSValue elementJsValue = JS_GetPropertyUint32(m_ctx, jsValue, i);
    JValue elementJavaValue = toJava(elementJsValue);
    JS_FreeValue(m_ctx, elementJsValue);
    const JniLocalRef<jobject> &jElement = elementJavaValue.getLocalRef();
    objectArray.setElement((jsize) i, jElement);

    if (m_jniContext->exceptionCheck()) {
      throw JniException(m_jniContext);
    }
  }

  return JValue(objectArray);
}

JSValue JavaType::fromJavaArray(const JniLocalRef<jarray>& values) const {
  JObjectArrayLocalRef objectArray(values.staticCast<jobjectArray>());
  const auto size = objectArray.getLength();

  JSValue jsArray = JS_NewArray(m_ctx);

  for (jsize i = 0; i < size; ++i) {
    JniLocalRef<jobject> object = objectArray.getElement(i);
    try {
      JSValue elementValue = fromJava(JValue(object));
      JS_SetPropertyUint32(m_ctx, jsArray, static_cast<uint32_t>(i), elementValue);
    } catch (const std::exception &) {
      JS_FreeValue(m_ctx, jsArray);
      throw;
    }
  }

  return jsArray;
}

#endif

JValue JavaType::callMethod(jmethodID methodId, const JniRef<jobject> &javaThis, const std::vector<JValue> &args) const {

 JniLocalRef<jobject> returnValue = m_jniContext->callObjectMethodA(javaThis, methodId, args);

 // Release all values now because they won't be used afterwards
 JValue::releaseAll(args);

 if (m_jniContext->exceptionCheck()) {
   throw JniException(m_jniContext);
 }

 return JValue(returnValue);
}
