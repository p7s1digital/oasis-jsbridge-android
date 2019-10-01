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
#include "JavaTypeId.h"
#include "log.h"
#include <unordered_map>
#include <algorithm>
#include <string>

// map<javaName, JavaTypeId>
// where javaName: Java name as returned by Java::class.getName(), e.g.: "java.lang.Integer"
static thread_local std::unordered_map<std::string_view, JavaTypeId> sJavaNameToID = {
  { "", JavaTypeId::Unknown },

  { "V", JavaTypeId::Void },
  { "java.lang.Void", JavaTypeId::BoxedVoid },
  { "kotlin.Unit", JavaTypeId::Unit },

  { "boolean", JavaTypeId::Boolean },
  { "int", JavaTypeId::Int },
  { "long", JavaTypeId::Long },
  { "float", JavaTypeId::Float },
  { "double", JavaTypeId::Double },

  { "java.lang.Boolean", JavaTypeId::BoxedBoolean },
  { "java.lang.Integer", JavaTypeId::BoxedInt },
  { "java.lang.Long", JavaTypeId::BoxedLong },
  { "java.lang.Float", JavaTypeId::BoxedFloat },
  { "java.lang.Double", JavaTypeId::BoxedDouble },

  { "java.lang.String", JavaTypeId::String },
  { "java.lang.Object", JavaTypeId::Object },

  { "[Ljava.lang.Object;",JavaTypeId::ObjectArray },

  { "[Z", JavaTypeId::BooleanArray },
  { "[I", JavaTypeId::IntArray },
  { "[J", JavaTypeId::LongArray },
  { "[F", JavaTypeId::FloatArray },
  { "[D", JavaTypeId::DoubleArray },

  { "kotlin.jvm.functions.Function0", JavaTypeId::FunctionX },
  { "kotlin.jvm.functions.Function1", JavaTypeId::FunctionX },
  { "kotlin.jvm.functions.Function2", JavaTypeId::FunctionX },
  { "kotlin.jvm.functions.Function3", JavaTypeId::FunctionX },
  { "kotlin.jvm.functions.Function4", JavaTypeId::FunctionX },
  { "kotlin.jvm.functions.Function5", JavaTypeId::FunctionX },
  { "kotlin.jvm.functions.Function6", JavaTypeId::FunctionX },
  { "kotlin.jvm.functions.Function7", JavaTypeId::FunctionX },
  { "kotlin.jvm.functions.Function8", JavaTypeId::FunctionX },
  { "kotlin.jvm.functions.Function9", JavaTypeId::FunctionX },

  { "de.prosiebensat1digital.oasisjsbridge.JsValue", JavaTypeId::JsValue },
  { "de.prosiebensat1digital.oasisjsbridge.JsonObjectWrapper", JavaTypeId::JsonObjectWrapper },

  { "kotlinx.coroutines.Deferred", JavaTypeId::Deferred }
};

// map<javaName, JavaTypeId> => map<JavaTypeId, jniClassName>
// where:
// - javaName = string_view: Java name as returned by Java::class.getName(), e.g.: "java.lang.Integer"
// - jniClassName = string: JNI class name as needed by JNIenv::findClass(...), e.g.: "java/lang/Integer"
static std::unordered_map<JavaTypeId, std::string> createIdToJniClassName() {
  std::unordered_map<JavaTypeId, std::string> idTojniClassName;

  for (const auto &p : sJavaNameToID) {
    std::string_view javaName = p.first;
    JavaTypeId id = p.second;

    size_t length = javaName.length();
    std::string jniClassName;
    jniClassName.reserve(length);

    for (char c : javaName) {
      if (c == '.') c = '/';
      jniClassName += c;
    }

    idTojniClassName[id] = jniClassName;
  }

  return idTojniClassName;
}

static std::unordered_map<JavaTypeId, std::string> sIdToJavaName = createIdToJniClassName();

// Get the id from the Java name returned by Java::class.getName(), e.g.: "java.lang.Integer"
JavaTypeId getJavaTypeIdByJavaName(std::string_view javaName) {
  auto itFind = sJavaNameToID.find(javaName);
  if (itFind != sJavaNameToID.end()) {
    return itFind->second;
  }

  if (javaName.empty()) {
    return JavaTypeId::Unknown;
  }

  // ObjectArray
  if (javaName[0] == '[') {
    sJavaNameToID[javaName] = JavaTypeId::ObjectArray;
    return JavaTypeId::ObjectArray;
  }

  return JavaTypeId::Unknown;
}

// Returns the JNI class name needed by JNIenv::findClass(...), e.g.: "java/lang/Integer"
const std::string &getJniClassNameByJavaTypeId(JavaTypeId id) {
  return sIdToJavaName[id];
}
