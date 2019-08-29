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
#include "JavaTypeMap.h"
#include "JavaType.h"
#include "java-types/Array.h"
#include "java-types/Boolean.h"
#include "java-types/BoxedPrimitive.h"
#include "java-types/Deferred.h"
#include "java-types/Double.h"
#include "java-types/Float.h"
#include "java-types/FunctionX.h"
#include "java-types/Integer.h"
#include "java-types/Long.h"
#include "java-types/JsValue.h"
#include "java-types/JsonObjectWrapper.h"
#include "java-types/Object.h"
#include "java-types/Primitive.h"
#include "java-types/String.h"
#include "java-types/Void.h"
#include "jni-helpers/JArrayLocalRef.h"
#include "jni-helpers/JObjectArrayLocalRef.h"
#include "jni-helpers/JStringLocalRef.h"
#include "jni-helpers/JValue.h"
#include "jni-helpers/JniLocalRef.h"
#include <string>
#include <vector>

using namespace JavaTypes;

namespace {
  std::string getName(const JsBridgeContext *jsBridgeContext, const JniRef<jclass> &javaClass) {
    const JniContext *jniContext = jsBridgeContext->jniContext();

    JniLocalRef<jclass> objectClass = jniContext->getObjectClass(javaClass);
    jmethodID method = jniContext->getMethodID(objectClass, "getName", "()Ljava/lang/String;");
    return JStringLocalRef(jniContext->callObjectMethod<jstring>(javaClass, method)).str();
  }

  /**
   * Loads the (primitive) TYPE member of {@code boxedClassName}.
   * For example, given java/lang/Integer, this function will return int.class.
   */
  JniLocalRef<jclass> getPrimitiveType(JniContext *jniContext, const JniLocalRef<jclass> &boxedClass) {
    jfieldID typeField = jniContext->getStaticFieldID(boxedClass, "TYPE", "Ljava/lang/Class;");
    return jniContext->getStaticObjectField(boxedClass, typeField);
  }

  /**
   * Adds type adapters for primitive and boxed versions of {@code name}, as well as arrays of those
   * types.
   */
  template<typename JavaTypeT>
  JavaType *addTypeAdapters(const JsBridgeContext *jsBridgeContext,
                            std::map<std::string, const JavaType *> &types,
                            const char *name, const char* primitiveSignature) {
    JniContext *jniContext = jsBridgeContext->jniContext();

    JniLocalRef<jclass> theClass = jniContext->findClass(name);
    JniLocalRef<jclass> primitiveClass = getPrimitiveType(jniContext, theClass);
    const auto javaType = new JavaTypeT(jsBridgeContext, JniGlobalRef<jclass>(primitiveClass), JniGlobalRef<jclass>(theClass));
    types.emplace(getName(jsBridgeContext, primitiveClass), javaType);
    types.emplace(primitiveSignature, new JavaTypeT(*javaType));

    const auto boxedType = new BoxedPrimitive(jsBridgeContext, *javaType);
    types.emplace(getName(jsBridgeContext, theClass), boxedType);

    return boxedType;
  }

  /** convert Ljava.lang.String; -> java.lang.String */
  std::string dropLandSemicolon(const std::string& s) {
    if (s.empty() || s[0] != 'L') {
      return s;
    }
    return s.substr(1, s.length() - 2);
  }

} // anonymous namespace

JavaTypeMap::~JavaTypeMap() {
  for (const auto &entry : m_types) {
    delete entry.second;
  }
}

const JavaType* JavaTypeMap::get(const JsBridgeContext *jsBridgeContext, const JniRef<jclass> &javaClass, bool boxed) const {
  const JavaType *javaType = find(jsBridgeContext, getName(jsBridgeContext, javaClass));

  if (!boxed  || !javaType->isPrimitive()) {
    return javaType;
  }

  auto primitive = (const Primitive *) javaType;
  return get(jsBridgeContext, primitive->boxedClass());
}

const JavaType *JavaTypeMap::getObjectType(const JsBridgeContext *jsBridgeContext) const {
  return find(jsBridgeContext, "java.lang.Object");
}

const JavaTypes::Deferred *JavaTypeMap::getDeferredType(const JsBridgeContext *jsBridgeContext) const {
  return static_cast<const Deferred *>(find(jsBridgeContext, "kotlinx.coroutines.Deferred"));
}

const JavaType *JavaTypeMap::find(const JsBridgeContext *jsBridgeContext, const std::string &name) const {
  if (m_types.empty()) {
    // Load up the map with the types we support
    JniContext *jniContext = jsBridgeContext->jniContext();

    {
      JniLocalRef<jclass> voidClass = jniContext->findClass("java/lang/Void");
      JniLocalRef<jclass> vClass = getPrimitiveType(jniContext, voidClass);
      m_types.emplace(
          getName(jsBridgeContext, vClass),
          new Void(jsBridgeContext, JniGlobalRef<jclass>(vClass), false, false));
      m_types.emplace(
          getName(jsBridgeContext, voidClass),
          new Void(jsBridgeContext, JniGlobalRef<jclass>(voidClass), true, false));
    }

    {
      JniLocalRef<jclass> unitClass = jniContext->findClass("kotlin/Unit");
      m_types.emplace(
          getName(jsBridgeContext, unitClass),
          new Void(jsBridgeContext, JniGlobalRef<jclass>(unitClass), true, true));
    }

    {
      JniLocalRef<jclass> stringClass = jniContext->findClass("java/lang/String");
      m_types.emplace(
          getName(jsBridgeContext, stringClass),
          new String(jsBridgeContext, JniGlobalRef<jclass>(stringClass)));
    }

    const JavaType *jsonObjectWrapperType;
    {
      JniLocalRef<jclass> jsonObjectWrapperClass = jniContext->findClass("de/prosiebensat1digital/oasisjsbridge/JsonObjectWrapper");
      jsonObjectWrapperType = new JsonObjectWrapper(jsBridgeContext, JniGlobalRef<jclass>(jsonObjectWrapperClass));

      m_types.emplace(
          getName(jsBridgeContext, jsonObjectWrapperClass),
          jsonObjectWrapperType);
    }

    {
      addTypeAdapters<Integer>(jsBridgeContext, m_types, "java/lang/Integer", "I");
      addTypeAdapters<Long>(jsBridgeContext, m_types, "java/lang/Long", "J");
    }

    {
      const auto boxedBooleanType = addTypeAdapters<Boolean>(jsBridgeContext, m_types, "java/lang/Boolean", "Z");
      const auto boxedDoubleType = addTypeAdapters<Double>(jsBridgeContext, m_types, "java/lang/Double", "D");
      addTypeAdapters<Float>(jsBridgeContext, m_types, "java/lang/Float", "F");

      JniLocalRef<jclass> objectClass = jniContext->findClass("java/lang/Object");
      auto objectType = new Object(jsBridgeContext, JniGlobalRef<jclass>(objectClass),
                   *boxedBooleanType, *boxedDoubleType, *jsonObjectWrapperType);

      m_types.emplace(
          getName(jsBridgeContext, objectClass),
          objectType);
    }

    {
      JniLocalRef<jclass> deferredClass = jniContext->findClass("kotlinx/coroutines/Deferred");
      m_types.emplace(
          getName(jsBridgeContext, deferredClass),
          new Deferred(jsBridgeContext, JniGlobalRef<jclass>(deferredClass)));
    }

    {
      JniLocalRef<jclass> jsValueClass = jniContext->findClass("de/prosiebensat1digital/oasisjsbridge/JsValue");
      m_types.emplace(
          getName(jsBridgeContext, jsValueClass),
          new JsValue(jsBridgeContext, JniGlobalRef<jclass>(jsValueClass)));
    }


    static const auto functionXname = [](int i) {
      return "kotlin/jvm/functions/Function" + std::to_string(i);
    };

    for (int i = 0; i <= 9; ++i) {
      JniLocalRef<jclass> functionClass = jniContext->findClass(functionXname(i).c_str());
      const auto functionType = new FunctionX(
          jsBridgeContext,
          JniGlobalRef<jclass>(functionClass)
      );
      m_types.emplace(getName(jsBridgeContext, functionClass), functionType);
    }
  }

  const auto itType = m_types.find(name);
  if (itType != m_types.end()) {
    return itType->second;
  }

  if (!name.empty() && name[0] == '[') {
    const auto elementType = find(jsBridgeContext, dropLandSemicolon(name.substr(name.find('[') + 1)));

    JniLocalRef<jclass> arrayClass = elementType->getArrayClass();
    const auto arrayType = new Array(jsBridgeContext, JniGlobalRef<jclass>(arrayClass), *elementType);
    m_types.emplace(getName(jsBridgeContext, arrayClass), arrayType);
    return arrayType;
  }

  throw std::invalid_argument("Unsupported Java type " + name);
}
