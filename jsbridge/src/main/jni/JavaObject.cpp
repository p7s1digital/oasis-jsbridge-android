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
#include "JavaObject.h"

#include "JavaMethod.h"
#include "JavaType.h"
#include "JniCache.h"
#include "JsBridgeContext.h"

#if defined(DUKTAPE)
# include "StackChecker.h"
#include "JniCache.h"

#elif defined(QUICKJS)
# include "QuickJsUtils.h"
#endif

namespace {
  const char *JAVA_THIS_PROP_NAME = "\xff\xffjava_this";
  const char *JAVA_METHOD_PROP_NAME = "\xff\xffjava_method";
}

#if defined(DUKTAPE)

namespace {
  // Called by Duktape when JS invokes a method on our bound Java object
  extern "C"
  duk_ret_t javaMethodHandler(duk_context *ctx) {
    CHECK_STACK(ctx);

    JsBridgeContext *duktapeContext = JsBridgeContext::getInstance(ctx);
    assert(duktapeContext != nullptr);

    JniContext *jniContext = duktapeContext->jniContext();
    JNIEnv *env = jniContext->getJNIEnv();
    assert(env != nullptr);

    // Get JavaMethod instance bound to the function itself
    duk_push_current_function(ctx);
    duk_get_prop_string(ctx, -1, JAVA_METHOD_PROP_NAME);
    if (duk_is_null_or_undefined(ctx, -1)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "Cannot execute Java method: Java method not found!");
      duk_pop_2(ctx);
      return DUK_RET_ERROR;
    }
    auto method = static_cast<JavaMethod *>(duk_require_pointer(ctx, -1));
    duk_pop_2(ctx);  // Java method + current function

    // JS this -> Java this
    duk_push_this(ctx);
    duk_get_prop_string(ctx, -1, JAVA_THIS_PROP_NAME);
    if (duk_is_null_or_undefined(ctx, -1)) {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "Cannot execute Java method: Java object not found!");
      duk_pop_2(ctx);
      return DUK_RET_ERROR;
    }
    auto thisObjectRaw = reinterpret_cast<jobject>(duk_require_pointer(ctx, -1));
    JniLocalRef <jobject> thisObject(jniContext, env->NewLocalRef(thisObjectRaw));
    duk_pop_2(ctx);

    CHECK_STACK_NOW();
    return method->invoke(duktapeContext, thisObject);
  }

  // Called by Duktape to handle finalization of bound Java objects
  extern "C"
  duk_ret_t javaObjectFinalizer(duk_context *ctx) {
    CHECK_STACK(ctx);

    JsBridgeContext *duktapeContext = JsBridgeContext::getInstance(ctx);
    assert(duktapeContext != nullptr);

    JniContext *jniContext = duktapeContext->jniContext();

    if (duk_get_prop_string(ctx, -1, JAVA_THIS_PROP_NAME)) {
      // Remove the global reference from the bound Java object
      JniGlobalRef<jobject>::deleteRawGlobalRef(jniContext, static_cast<jobject>(duk_require_pointer(ctx, -1)));
      duk_pop(ctx);
      duk_del_prop_string(ctx, -1, JAVA_METHOD_PROP_NAME);
    }

    // Iterate over all of the properties, deleting all the JavaMethod objects we attached.
    duk_enum(ctx, -1, DUK_ENUM_OWN_PROPERTIES_ONLY);
    while (duk_next(ctx, -1, (duk_bool_t) true)) {
      if (!duk_get_prop_string(ctx, -1, JAVA_METHOD_PROP_NAME)) {
        duk_pop_2(ctx);
        continue;
      }
      delete static_cast<JavaMethod *>(duk_require_pointer(ctx, -1));
      duk_pop_3(ctx);
    }

    // Pop the enum
    duk_pop(ctx);
    return 0;
  }

  // Called by Duktape when JS invokes a bound Java function
  extern "C"
  duk_ret_t javaLambdaHandler(duk_context *ctx) {
    CHECK_STACK(ctx);

    JsBridgeContext *duktapeContext = JsBridgeContext::getInstance(ctx);
    assert(duktapeContext != nullptr);

    JniContext *jniContext = duktapeContext->jniContext();
    JNIEnv *env = jniContext->getJNIEnv();
    assert(env != nullptr);

    duk_push_current_function(ctx);

    // Get JavaMethod instance bound to the function itself
    duk_get_prop_string(ctx, -1, JAVA_METHOD_PROP_NAME);
    auto method = static_cast<JavaMethod *>(duk_require_pointer(ctx, -1));
    duk_pop(ctx);  // Java method

    // Java this is a property of the JS method
    duk_get_prop_string(ctx, -1, JAVA_THIS_PROP_NAME);
    auto thisObjectRaw = reinterpret_cast<jobject>(duk_require_pointer(ctx, -1));
    JniLocalRef<jobject> thisObject(jniContext, env->NewLocalRef(thisObjectRaw));
    duk_pop_2(ctx);  // Java this + current function

    CHECK_STACK_NOW();
    return method->invoke(duktapeContext, thisObject);
  }

  // Called by Duktape to handle finalization of bound Java lambdas
  extern "C"
  duk_ret_t javaLambdaFinalizer(duk_context *ctx) {
    CHECK_STACK(ctx);

    JsBridgeContext *duktapeContext = JsBridgeContext::getInstance(ctx);
    assert(duktapeContext != nullptr);

    JniContext *jniContext = duktapeContext->jniContext();

    if (duk_get_prop_string(ctx, -1, JAVA_THIS_PROP_NAME)) {
      JniGlobalRef<jobject>::deleteRawGlobalRef(jniContext, static_cast<jobject>(duk_require_pointer(ctx, -1)));
      duk_pop(ctx);
    }

    if (duk_get_prop_string(ctx, -1, JAVA_METHOD_PROP_NAME)) {
      delete static_cast<JavaMethod *>(duk_require_pointer(ctx, -1));
      duk_pop(ctx);
    }

    return 0;
  }
}

// static
duk_ret_t JavaObject::push(const JsBridgeContext *jsBridgeContext, const std::string &strName, const JniLocalRef<jobject> &object, const JObjectArrayLocalRef &methods) {
  duk_context *ctx = jsBridgeContext->getCContext();
  const JniContext *jniContext = jsBridgeContext->jniContext();

  CHECK_STACK_OFFSET(ctx, 1);

  const duk_idx_t objIndex = duk_push_object(ctx);

  // Hook up a finalizer to decrement the refcount and clean up our JavaMethods.
  duk_push_c_function(ctx, javaObjectFinalizer, 1);
  duk_set_finalizer(ctx, objIndex);

  const jsize numMethods = methods.getLength();
  std::string qualifiedMethodPrefix = strName + "::";

  for (jsize i = 0; i < numMethods; ++i) {
    JniLocalRef<jsBridgeMethod> method = methods.getElement<jsBridgeMethod>(i);
    JniCache::MethodInterface methodInterface = jsBridgeContext->getJniCache()->methodInterface(method);

    std::string strMethodName = methodInterface.getName().str();
    std::string qualifiedMethodName = qualifiedMethodPrefix + strMethodName;

    std::unique_ptr<JavaMethod> javaMethod;
    try {
      javaMethod = std::make_unique<JavaMethod>(jsBridgeContext, method, qualifiedMethodName, false /*isLambda*/);
    } catch (const std::invalid_argument &e) {
      jsBridgeContext->queueIllegalArgumentException(std::string() + "In bound method \"" + qualifiedMethodName + "\": " + e.what());
      duk_pop(ctx);  // object being bound
      duk_push_undefined(ctx);
      return DUK_RET_ERROR;
    }

    // Use VARARGS here to allow us to manually validate that the proper number of arguments are
    // given in the call. If we specify the actual number of arguments needed, Duktape will try to
    // be helpful by discarding extra or providing missing arguments. That's not quite what we want.
    // See http://duktape.org/api.html#duk_push_c_function for details.
    const duk_idx_t func = duk_push_c_function(ctx, javaMethodHandler, DUK_VARARGS);
    duk_push_pointer(ctx, javaMethod.release());
    duk_put_prop_string(ctx, func, JAVA_METHOD_PROP_NAME);

    // Add this method to the bound object.
    duk_put_prop_string(ctx, objIndex, strMethodName.c_str());
  }

  JNIEnv *env = jniContext->getJNIEnv();
  assert(env != nullptr);

  // Keep a reference in JavaScript to the object being bound.
  duk_push_pointer(ctx, object.toNewRawGlobalRef());  // JNI global ref will be deleted via JS finalizer
  duk_put_prop_string(ctx, objIndex, JAVA_THIS_PROP_NAME);

  return 1;
}

// static
duk_ret_t JavaObject::pushLambda(const JsBridgeContext *jsBridgeContext, const std::string &strName, const JniLocalRef<jobject> &object, const JniLocalRef<jsBridgeMethod> &method) {
  duk_context *ctx = jsBridgeContext->getCContext();
  const JniContext *jniContext = jsBridgeContext->jniContext();

  CHECK_STACK_OFFSET(ctx, 1);

  JniCache::MethodInterface methodInterface = jsBridgeContext->getJniCache()->methodInterface(method);

  std::string strMethodName = methodInterface.getName().str();
  std::string qualifiedMethodName = strName + "::" + strMethodName;

  std::unique_ptr<JavaMethod> javaMethod;
  try {
    javaMethod = std::make_unique<JavaMethod>(jsBridgeContext, method, qualifiedMethodName, true /*isLambda*/);
  } catch (const std::invalid_argument &e) {
    jsBridgeContext->queueIllegalArgumentException(std::string() + "In bound method \"" + qualifiedMethodName + "\": " + e.what());
    // Pop the object being bound
    duk_pop(ctx);
    return 1;
  }

  // Use VARARGS here to allow us to manually validate that the proper number of arguments are
  // given in the call. If we specify the actual number of arguments needed, Duktape will try to
  // be helpful by discarding extra or providing missing arguments. That's not quite what we want.
  // See http://duktape.org/api.html#duk_push_c_function for details.
  const duk_idx_t funcIndex = duk_push_c_function(ctx, javaLambdaHandler, DUK_VARARGS);

  duk_push_pointer(ctx, javaMethod.release());
  duk_put_prop_string(ctx, funcIndex, JAVA_METHOD_PROP_NAME);

  // Keep a reference in JavaScript to the lambda being bound.
  duk_push_pointer(ctx, object.toNewRawGlobalRef());  // JNI global ref will be deleted via JS finalizer
  duk_put_prop_string(ctx, funcIndex, JAVA_THIS_PROP_NAME);

  // Set a finalizer
  duk_push_c_function(ctx, javaLambdaFinalizer, 1);
  duk_set_finalizer(ctx, funcIndex);

  return 1;
}

#elif defined(QUICKJS)

namespace {
  // Called by QuickJS when JS invokes a method on our bound Java object
  JSValue javaMethodHandler(JSContext *ctx, JSValueConst /*this_val*/, int argc, JSValueConst *argv, int /*magic*/, JSValueConst *datav) {
    JsBridgeContext *quickJsContext = JsBridgeContext::getInstance(ctx);
    assert(quickJsContext != nullptr);

    // Get JavaMethod instance bound to the function itself
    auto javaMethod = QuickJsUtils::getCppPtr<JavaMethod>(datav[0]);

    // Java this is a property of the JS method
    JniLocalRef<jobject> javaThis = quickJsContext->getUtils()->getJavaRef<jobject>(datav[1]);

    JSValue ret = javaMethod->invoke(quickJsContext, javaThis, argc, argv);

    // Manually check for pending exceptions
    JSValue pendingException = JS_GetException(ctx);
    if (!JS_IsNull(pendingException)) {
      JS_Throw(ctx, pendingException);
      return JS_EXCEPTION;
    }

    return ret;
  }
}

// static
JSValue JavaObject::create(const JsBridgeContext *jsBridgeContext, const std::string &strName, const JniLocalRef<jobject> &object, const JObjectArrayLocalRef &methods) {
  JSContext *ctx = jsBridgeContext->getCContext();
  const JniContext *jniContext = jsBridgeContext->jniContext();
  const QuickJsUtils *utils = jsBridgeContext->getUtils();

  JSValue javaObjectValue = JS_NewObject(ctx);

  const jsize numMethods = methods.getLength();
  std::string qualifiedMethodPrefix = strName + "::";

  for (jsize i = 0; i < numMethods; ++i) {
    JniLocalRef<jsBridgeMethod> method = methods.getElement<jsBridgeMethod>(i);
    JniCache::MethodInterface methodInterface = jsBridgeContext->getJniCache()->methodInterface(method);

    std::string strMethodName = methodInterface.getName().str();
    std::string qualifiedMethodName = qualifiedMethodPrefix + strMethodName;

    std::unique_ptr<JavaMethod> javaMethod;
    try {
      javaMethod = std::make_unique<JavaMethod>(jsBridgeContext, method, qualifiedMethodName, false /*isLambda*/);
    } catch (const std::invalid_argument &e) {
      jsBridgeContext->queueIllegalArgumentException(std::string() + "In bound method \"" + qualifiedMethodName + "\": " + e.what());
      JS_FreeValue(ctx, javaObjectValue);
      return JS_UNDEFINED;
    }

    JSValue javaMethodValue = utils->createCppPtrValue(javaMethod.release(), true);
    JSValue javaThisValue = utils->createJavaRefValue(JniGlobalRef<jobject>(object));
    JSValueConst javaMethodHandlerData[2];
    javaMethodHandlerData[0] = javaMethodValue;
    javaMethodHandlerData[1] = javaThisValue;
    JSValue javaMethodHandlerValue = JS_NewCFunctionData(ctx, javaMethodHandler, 1 /*length*/, 0 /*magic*/, 2, javaMethodHandlerData);

    // Free data values (they are duplicated by JS_NewCFunctionData)
    JS_FreeValue(ctx, javaMethodValue);
    JS_FreeValue(ctx, javaThisValue);

    // Add this method to the bound object
    JS_SetPropertyStr(ctx, javaObjectValue, strMethodName.c_str(), javaMethodHandlerValue);
    // No JS_FreeValue(m_ctx, javaMethodHandlerValue) after JS_SetPropertyStr()
  }

  // Keep a reference in JavaScript to the object being bound
  // (which is properly released when the JSValue gets finalized)
  auto javaThisValue = utils->createJavaRefValue<jobject>(object);
  JS_SetPropertyStr(ctx, javaObjectValue, JAVA_THIS_PROP_NAME, javaThisValue);
  // No JS_FreeValue(m_ctx, javaThisValue) after JS_SetPropertyStr()

  return javaObjectValue;
}

// static
JSValue JavaObject::createLambda(const JsBridgeContext *jsBridgeContext, const std::string &strName, const JniLocalRef<jobject> &object, const JniLocalRef<jsBridgeMethod> &method) {
  JSContext *ctx = jsBridgeContext->getCContext();
  const JniContext *jniContext = jsBridgeContext->jniContext();
  const QuickJsUtils *utils = jsBridgeContext->getUtils();

  JniCache::MethodInterface methodInterface = jsBridgeContext->getJniCache()->methodInterface(method);

  std::string strMethodName = methodInterface.getName().str();
  std::string qualifiedMethodName = strName + "::" + strMethodName;

  std::unique_ptr<JavaMethod> javaMethod;
  try {
    javaMethod = std::make_unique<JavaMethod>(jsBridgeContext, method, qualifiedMethodName, true /*isLambda*/);
  } catch (const std::invalid_argument &e) {
    jsBridgeContext->queueIllegalArgumentException(
        std::string() + "In bound method \"" + qualifiedMethodName + "\": " + e.what());
    return JS_UNDEFINED;
  }

  JSValue javaLambdaValue = utils->createCppPtrValue(javaMethod.release(), true /*deleteOnFinalize*/);  // wrap the C++ instance
  JSValue javaThisValue = utils->createJavaRefValue(JniGlobalRef<jobject>(object));  // wrap the JNI ref
  JSValueConst javaLambdaHandlerData[2];
  javaLambdaHandlerData[0] = javaLambdaValue;
  javaLambdaHandlerData[1] = javaThisValue;
  JSValue javaLambdaHandlerValue = JS_NewCFunctionData(ctx, javaMethodHandler, 1 /*length*/, 0 /*magic*/, 2, javaLambdaHandlerData);

  // Free data values (they are duplicated by JS_NewCFunctionData)
  JS_FreeValue(ctx, javaLambdaValue);
  JS_FreeValue(ctx, javaThisValue);

  return javaLambdaHandlerValue;
}

#endif

