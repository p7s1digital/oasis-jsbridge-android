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
#include "JavaObjectWrapper.h"

#include "JavaObject.h"
#include "JsBridgeContext.h"
#include "JniCache.h"
#include "jni-helpers/JniContext.h"

#include <exceptions/JniException.h>
#include <log.h>

namespace JavaTypes {

JavaObjectWrapper::JavaObjectWrapper(const JsBridgeContext *jsBridgeContext)
 : JavaType(jsBridgeContext, JavaTypeId::JavaObjectWrapper) {

}

#if defined(DUKTAPE)

#include "StackChecker.h"

JValue JavaObjectWrapper::pop() const {
  CHECK_STACK_OFFSET(m_ctx, -1);

  if (!duk_is_object(m_ctx, -1) && !duk_is_undefined(m_ctx, -1) && !duk_is_null(m_ctx, -1)) {
    duk_pop(m_ctx);
    return JValue();
  }

  JniLocalRef<jobject> javaWrapperObject;
  auto javaWrappedObject = JavaObject::getJavaThis(m_jsBridgeContext, -1);
  auto javaObject = getJniCache()->javaObjectWrapperFromJavaObject(javaWrappedObject);
  duk_pop(m_ctx);

  return JValue(javaObject);
}

duk_ret_t JavaObjectWrapper::push(const JValue &value) const {
  CHECK_STACK_OFFSET(m_ctx, 1);

  const JniLocalRef<jobject> &javaJavaObjectWrapper = value.getLocalRef();

  if (javaJavaObjectWrapper.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  auto javaWrappedObject = getJniCache()->getJavaObjectWrapperJavaObject(javaJavaObjectWrapper);
  return JavaObject::push(m_jsBridgeContext, "<wrappedJavaObject>", javaWrappedObject);
}

#elif defined(QUICKJS)

#include "QuickJsUtils.h"

JValue JavaObjectWrapper::toJava(JSValueConst v) const {
  if (!JS_IsObject(v) && !JS_IsNull(v) && !JS_IsUndefined(v)) {
    return JValue();
  }

  auto javaWrappedObject = JavaObject::getJavaThis(m_jsBridgeContext, v);
  auto javaObject = getJniCache()->javaObjectWrapperFromJavaObject(javaWrappedObject);

  return JValue(javaObject);
}

JSValue JavaObjectWrapper::fromJava(const JValue &value) const {
  const JniLocalRef<jobject> &javaJavaObjectWrapper = value.getLocalRef();

  if (javaJavaObjectWrapper.isNull()) {
    return JS_NULL;
  }

  auto javaWrappedObject = getJniCache()->getJavaObjectWrapperJavaObject(javaJavaObjectWrapper);
  return JavaObject::create(m_jsBridgeContext, "<wrappedJavaObject>", javaWrappedObject);
}

#endif

}  // namespace JavaTypes

