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
#include <stdexcept>
#include <string>
#include <assert.h>

// map<javaName, JavaTypeId>
// where javaName: Java name (UTF16) as returned by Java::class.getName(), e.g.: "java.lang.Integer"
static thread_local std::unordered_map<std::u16string_view, JavaTypeId> sJavaNameToID = {
  { u"", JavaTypeId::Unknown },

  { u"V", JavaTypeId::Void },
  { u"java.lang.Void", JavaTypeId::BoxedVoid },
  { u"kotlin.Unit", JavaTypeId::Unit },

  { u"boolean", JavaTypeId::Boolean },
  { u"byte", JavaTypeId::Byte },
  { u"int", JavaTypeId::Int },
  { u"long", JavaTypeId::Long },
  { u"float", JavaTypeId::Float },
  { u"double", JavaTypeId::Double },
  { u"void", JavaTypeId::Void },

  { u"java.lang.Boolean", JavaTypeId::BoxedBoolean },
  { u"java.lang.Byte", JavaTypeId::BoxedByte },
  { u"java.lang.Integer", JavaTypeId::BoxedInt },
  { u"java.lang.Long", JavaTypeId::BoxedLong },
  { u"java.lang.Float", JavaTypeId::BoxedFloat },
  { u"java.lang.Double", JavaTypeId::BoxedDouble },

  { u"java.lang.String", JavaTypeId::String },
  { u"java.lang.Object", JavaTypeId::Object },

  { u"[Ljava.lang.Object;", JavaTypeId::ObjectArray },
  { u"java.util.List", JavaTypeId::List },

  { u"[Z", JavaTypeId::BooleanArray },
  { u"[B", JavaTypeId::ByteArray },
  { u"[I", JavaTypeId::IntArray },
  { u"[J", JavaTypeId::LongArray },
  { u"[F", JavaTypeId::FloatArray },
  { u"[D", JavaTypeId::DoubleArray },

  { u"kotlin.jvm.functions.Function0", JavaTypeId::FunctionX },
  { u"kotlin.jvm.functions.Function1", JavaTypeId::FunctionX },
  { u"kotlin.jvm.functions.Function2", JavaTypeId::FunctionX },
  { u"kotlin.jvm.functions.Function3", JavaTypeId::FunctionX },
  { u"kotlin.jvm.functions.Function4", JavaTypeId::FunctionX },
  { u"kotlin.jvm.functions.Function5", JavaTypeId::FunctionX },
  { u"kotlin.jvm.functions.Function6", JavaTypeId::FunctionX },
  { u"kotlin.jvm.functions.Function7", JavaTypeId::FunctionX },
  { u"kotlin.jvm.functions.Function8", JavaTypeId::FunctionX },
  { u"kotlin.jvm.functions.Function9", JavaTypeId::FunctionX },

  { u"de.prosiebensat1digital.oasisjsbridge.DebugString", JavaTypeId::DebugString },
  { u"de.prosiebensat1digital.oasisjsbridge.JsValue", JavaTypeId::JsValue },
  { u"de.prosiebensat1digital.oasisjsbridge.JsonObjectWrapper", JavaTypeId::JsonObjectWrapper },

  { u"kotlinx.coroutines.Deferred", JavaTypeId::Deferred }
};

// map<javaName, JavaTypeId> => map<JavaTypeId, jniClassName>
// where:
// - javaName = string_view: Java name (UTF16) as returned by Java::class.getName(), e.g.: "java.lang.Integer"
// - jniClassName = string: JNI class name (UTF8) as needed by JNIenv::findClass(...), e.g.: "java/lang/Integer"
static std::unordered_map<JavaTypeId, std::string> createIdToJniClassName() {
  std::unordered_map<JavaTypeId, std::string> idTojniClassName;

  for (const auto &p : sJavaNameToID) {
    std::u16string_view javaName = p.first;
    JavaTypeId id = p.second;

    size_t length = javaName.length();
    std::string strJniClassName;
    strJniClassName.reserve(length);

    // Basic UTF16 -> UTF8 conversion + replace '.' into '/'
    for (char16_t u16char : javaName) {
      assert(u16char <= 0x7F);  // plain ASCII
      auto u8char = static_cast<char>(u16char);
      if (u8char == '.') u8char = '/';
      strJniClassName += u8char;
    }

    idTojniClassName[id] = strJniClassName;
  }

  return idTojniClassName;
}

static std::unordered_map<JavaTypeId, std::string> sIdToJavaName = createIdToJniClassName();

// Get the id from the Java name (UTF16) returned by Java::class.getName(), e.g.: "java.lang.Integer"
JavaTypeId getJavaTypeIdByJavaName(std::u16string_view javaName) {
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

// Returns the JNI class name (UTF8) needed by JNIenv::findClass(...), e.g.: "java/lang/Integer"
const std::string &getJniClassNameByJavaTypeId(JavaTypeId id) {
  auto it = sIdToJavaName.find(id);
  if (it == sIdToJavaName.end()) {
    throw std::invalid_argument(std::string() + "Could not get Java name for JavaTypeId " + std::to_string(static_cast<int>(id)) + "!");
  }

  return it->second;
}
