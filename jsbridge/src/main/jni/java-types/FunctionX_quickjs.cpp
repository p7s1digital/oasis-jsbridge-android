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
#include "FunctionX.h"

#include "JsBridgeContext.h"
#include "JavaMethod.h"
#include "JavaScriptLambda.h"
#include "QuickJsUtils.h"
#include "jni-helpers/JniContext.h"
#include "jni-helpers/JniGlobalRef.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include "utils.h"
#include <memory>

namespace {
  const char *JS_FUNCTION_GLOBAL_NAME_PREFIX = "javaTypes_functionX_";  // Note: initial "\xff\xff" removed because of JNI string conversion issues
  const char *PAYLOAD_PROP_NAME = "\xff\xffpayload";

  struct CallJavaLambdaPayload {
    JniGlobalRef<jobject> javaThis;
    std::shared_ptr<JavaMethod> javaMethodPtr;
  };

  JSValue callJavaLambda(JSContext *ctx, JSValue, int argc, JSValueConst *argv, int magic, JSValueConst *datav) {
    JsBridgeContext *jsBridgeContext = JsBridgeContext::getInstance(ctx);
    assert(jsBridgeContext != nullptr);

    const QuickJsUtils *utils = jsBridgeContext->getUtils();
    assert(utils != nullptr);

    // Get the C++ CallJavaLambdaPayload from data
    auto payload = QuickJsUtils::getCppPtr<CallJavaLambdaPayload>(*datav);

    return payload->javaMethodPtr->invoke(jsBridgeContext, payload->javaThis, argc, argv);
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

// Get a JS function, register and create a native wrapper (JavaScriptLambda)
// - C++ -> Java: call createJsLambdaProxy with <functionId> as argument
// - Java -> C++: call callJsLambda (with <functionId> + args parameters)
JValue FunctionX::toJava(JSValueConst v, bool inScript, const AdditionalData *additionalData) const {
  const QuickJsUtils *utils = m_jsBridgeContext->getUtils();
  assert(utils != nullptr);

  // 1. Get JS brige Method from additional data
  auto additionalPopData = dynamic_cast<const AdditionalPopData *>(additionalData);
  assert(additionalPopData != nullptr);
  const JniRef<jsBridgeMethod> &javaMethod = additionalPopData->javaMethod;

  if (!inScript && !JS_IsFunction(m_ctx, v) && !JS_IsNull(v)) {
    const auto message = std::string("Cannot convert return value to FunctionX");
    throw std::invalid_argument(message);
  }

  static int jsFunctionCount = 0;
  std::string jsFunctionGlobalName = JS_FUNCTION_GLOBAL_NAME_PREFIX + std::to_string(++jsFunctionCount);

  // 1. Duplicate it into the global object with prop name <functionId>
  JSValue globalObj = JS_GetGlobalObject(m_ctx);
  JS_SetPropertyStr(m_ctx, globalObj, jsFunctionGlobalName.c_str(), JS_DupValue(m_ctx, v));
  JS_FreeValue(m_ctx, globalObj);

  // 2. Create the  C++ JavaScriptLambda instance
  auto javaScriptLambda = new JavaScriptLambda(m_jsBridgeContext, javaMethod, jsFunctionGlobalName, v);

  // 3. Wrap it inside the JS function
  utils->createMappedCppPtrValue<JavaScriptLambda>(javaScriptLambda, JS_DupValue(m_ctx, v), jsFunctionGlobalName.c_str());

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

// Get a native function, register it and return JS wrapper
JSValue FunctionX::fromJava(const JValue &value, bool inScript, const AdditionalData *additionalData) const {
  const QuickJsUtils *utils = m_jsBridgeContext->getUtils();
  assert(utils != nullptr);

  // 1. Get JavaMethod from additional data
  auto additionalPushData = dynamic_cast<const AdditionalPushData *>(additionalData);
  assert(additionalPushData != nullptr);
  auto javaMethodPtr = additionalPushData->javaMethodPtr;

  // 2. C++: get the JValue object which is a Java FunctionX instance
  const JniLocalRef<jobject> &javaFunctionObject = value.getLocalRef();
  if (javaFunctionObject.isNull()) {
    return JS_NULL;
  }

  // 3. C++: create a JS function which invokes the JavaMethod with the above Java this
  auto payload = new CallJavaLambdaPayload { JniGlobalRef<jobject>(javaFunctionObject), javaMethodPtr };
  JSValue payloadValue = utils->createCppPtrValue<CallJavaLambdaPayload>(payload, true);
  JSValue invokeFunctionValue = JS_NewCFunctionData(m_ctx, callJavaLambda, 1, 0, 1, &payloadValue);

  return invokeFunctionValue;
}

}  // namespace JavaTypes
