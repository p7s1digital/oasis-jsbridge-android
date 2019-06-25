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
#include "java-types/FunctionX.h"

#include "JsBridgeContext.h"
#include "JavaMethod.h"
#include "JavaScriptLambda.h"
#include "JavaScriptObjectMapper.h"
#include "jni-helpers/JniGlobalRef.h"
#include "jni-helpers/JniContext.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include "utils.h"
#include "duktape/duktape.h"
#include <memory>

namespace {
  const char *JS_FUNCTION_GLOBAL_NAME_PREFIX = "javaTypes_functionX_";  // Note: initial "\xff\xff" removed because of JNI string conversion issues
  const char *PAYLOAD_PROP_NAME = "\xff\xffpayload";

  struct CallJavaLambdaPayload {
    JniGlobalRef<jobject> javaThis;
    std::shared_ptr<JavaMethod> javaMethodPtr;
  };

  extern "C" {
    duk_ret_t callJavaLambda(duk_context *ctx) {

      duk_push_current_function(ctx);
      if (!duk_get_prop_string(ctx, -1, PAYLOAD_PROP_NAME)) {
        duk_pop_2(ctx);  // (undefined) javaThis + current function
        return DUK_RET_ERROR;
      }

      auto payload = reinterpret_cast<const CallJavaLambdaPayload *>(duk_require_pointer(ctx, -1));
      duk_pop_2(ctx);  // Payload + current function

      JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
      assert(jsBridgeContext != nullptr);

      return payload->javaMethodPtr->invoke(jsBridgeContext, payload->javaThis);
    }

    duk_ret_t finalizeJavaLambda(duk_context *ctx) {
      JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
      assert(jsBridgeContext != nullptr);

      if (duk_get_prop_string(ctx, -1, PAYLOAD_PROP_NAME)) {
        delete reinterpret_cast<const CallJavaLambdaPayload *>(duk_require_pointer(ctx, -1));
      }

      duk_pop_2(ctx);  // Payload + finalized object
      return 0;
    }
  }
}


namespace JavaTypes {

FunctionX::FunctionX(const JsBridgeContext *jsBridgeContext, const JniGlobalRef<jclass> &classRef)
 : JavaType(jsBridgeContext, classRef) {
}

class FunctionX::AdditionalPopData: public JavaType::AdditionalData {
  public:
    AdditionalPopData() = default;

    JniGlobalRef<jsBridgeMethod> javaMethod;
};

JavaType::AdditionalData *FunctionX::createAdditionalPopData(const JniRef<jsBridgeParameter> &parameter) const {
  auto data = new AdditionalPopData();

  const JniRef<jclass> &jsBridgeMethodClass = m_jniContext->getJsBridgeMethodClass();
  const JniRef<jclass> &parameterClass = m_jniContext->getJsBridgeParameterClass();

  jmethodID getInvokeMethod = m_jniContext->getMethodID(parameterClass, "getInvokeMethod", "()Lde/prosiebensat1digital/oasisjsbridge/Method;");
  JniLocalRef<jsBridgeMethod> invokeMethod = m_jniContext->callObjectMethod<jsBridgeMethod>(parameter, getInvokeMethod);

#if 1
  // More generic approach to get the invoke method name but probably useless as the name is *always* "invoke"
  jmethodID getName = m_jniContext->getMethodID(m_jniContext->getObjectClass(invokeMethod), "getName", "()Ljava/lang/String;");
  std::string strMethodName = JStringLocalRef(m_jniContext->callObjectMethod<jstring>(invokeMethod, getName)).str();
#else
  static const char *strMethodName = "invoke";
#endif

  data->javaMethod = JniGlobalRef<jsBridgeMethod>(invokeMethod);

  return data;
}

// Pop a JS function, register and create a native wrapper (JavaScriptLambda)
// - C++ -> Java: call createJsLambdaProxy with <functionId> as argument
// - Java -> C++: call callJsLambda (with <functionId> + args parameters)
JValue FunctionX::pop(bool inScript, const AdditionalData *additionalData) const {
  // 1. Get JS brige Method from additional data
  auto additionalPopData = dynamic_cast<const AdditionalPopData *>(additionalData);
  assert(additionalPopData != nullptr);
  const JniRef<jsBridgeMethod> &javaMethod = additionalPopData->javaMethod;

  if (!inScript && !duk_is_function(m_ctx, -1) && !duk_is_null(m_ctx, -1)) {
    const auto message = std::string("Cannot convert return value to FunctionX");
    duk_pop(m_ctx);
    throw std::invalid_argument(message);
  }

  // 1. Get the JS function which needs to be triggered from native
  duk_require_function(m_ctx, -1);
  duk_idx_t jsFuncIdx = duk_normalize_index(m_ctx, -1);

  static int jsFunctionCount = 0;
  std::string jsFunctionGlobalName = JS_FUNCTION_GLOBAL_NAME_PREFIX + std::to_string(++jsFunctionCount);

  // 2. Duplicate it into the global object with prop name <functionId>
  duk_push_global_object(m_ctx);
  duk_dup(m_ctx, jsFuncIdx);
  duk_put_prop_string(m_ctx, -2, jsFunctionGlobalName.c_str());
  duk_pop_2(m_ctx);  // global stash + duplicated JS function

  // 3. Create JavaScriptLambda C++ object and add it to the global mapper
  auto javaScriptLambdaFactory = [&](void *jsHeapPtr) {
    return new JavaScriptLambda(m_jsBridgeContext, javaMethod, jsFunctionGlobalName, jsHeapPtr);
  };
  const JavaScriptObjectMapper &jsObjectMapper = m_jsBridgeContext->getJsObjectMapper();
  jsObjectMapper.add(m_ctx, jsFunctionGlobalName, javaScriptLambdaFactory);

  // 4. Call native createJsLambdaProxy(id, javaMethod)
  JniLocalRef<jobject> javaFunction = m_jniContext->callJsBridgeObjectMethod(
      "createJsLambdaProxy", "(Ljava/lang/String;Lde/prosiebensat1digital/oasisjsbridge/Method;)Lkotlin/Function;",
      JStringLocalRef(m_jniContext, jsFunctionGlobalName.c_str()), javaMethod);
  m_jsBridgeContext->checkRethrowJsError();

  return JValue(javaFunction);
}

class FunctionX::AdditionalPushData: public JavaType::AdditionalData {
public:
  AdditionalPushData() = default;
  std::shared_ptr<JavaMethod> javaMethodPtr;
};

JavaType::AdditionalData *FunctionX::createAdditionalPushData(const JniRef<jsBridgeParameter> &parameter) const {
  const JniRef<jclass> &jsBridgeParameterClass = m_jniContext->getJsBridgeParameterClass();
  const JniRef<jclass> &jsBridgeMethodClass = m_jniContext->getJsBridgeMethodClass();

  jmethodID getInvokeMethod = m_jniContext->getMethodID(jsBridgeParameterClass, "getInvokeMethod", "()Lde/prosiebensat1digital/oasisjsbridge/Method;");
  JniLocalRef<jsBridgeMethod> method = m_jniContext->callObjectMethod<jsBridgeMethod>(parameter, getInvokeMethod);
  if (method.isNull()) {
      alog("WARNING: could not get JsBridge Method instance from parameter!");
      return nullptr;
  }

#ifdef NDEBUG
  static const char *functionXName = "<FunctionX>";
#else
  jmethodID getName = m_jniContext->getMethodID(jsBridgeParameterClass, "getName", "()Ljava/lang/String;");
  jmethodID getMethodName = m_jniContext->getMethodID(jsBridgeParameterClass, "getMethodName", "()Ljava/lang/String;");
  JStringLocalRef paramNameRef(m_jniContext->callObjectMethod<jstring>(parameter, getName));
  JStringLocalRef methodNameRef(m_jniContext->callObjectMethod<jstring>(parameter, getMethodName));
  std::string methodName = methodNameRef.isNull() ? "" : methodNameRef.str();
  std::string paramName = paramNameRef.isNull() ? "_" : paramNameRef.str();
  std::string functionXName = "<FunctionX>/" + methodName + "::" + paramName;
#endif

  auto data = new AdditionalPushData();
  data->javaMethodPtr = std::make_shared<JavaMethod>(m_jsBridgeContext, method, functionXName, true /*isLambda*/);
  return data;
}

// Get a native function, register it and push a JS wrapper
duk_ret_t FunctionX::push(const JValue &value, bool inScript, const AdditionalData *additionalData) const {

  // 1. Get JavaMethod from additional data
  auto additionalPushData = dynamic_cast<const AdditionalPushData *>(additionalData);
  assert(additionalPushData != nullptr);
  auto javaMethodPtr = additionalPushData->javaMethodPtr;

  // 2. C++: get the JValue object which is a Java FunctionX instance
  const JniLocalRef<jobject> &javaFunctionObject = value.getLocalRef();

  if (javaFunctionObject.isNull()) {
    duk_push_null(m_ctx);
    return 1;
  }

  // 3. C++: create and push a JS function which invokes the JavaMethod with the above Java this
  const duk_idx_t funcIdx = duk_push_c_function(m_ctx, callJavaLambda, DUK_VARARGS);

  // Bind Payload
  auto payload = new CallJavaLambdaPayload { JniGlobalRef<jobject>(javaFunctionObject), javaMethodPtr };
  duk_push_pointer(m_ctx, payload);
  duk_put_prop_string(m_ctx, funcIdx, PAYLOAD_PROP_NAME);

  // Finalizer (which releases the JavaMethod instance)
  duk_push_c_function(m_ctx, finalizeJavaLambda, 1);
  duk_set_finalizer(m_ctx, funcIdx);

  return 1;
}

}  // namespace JavaType

