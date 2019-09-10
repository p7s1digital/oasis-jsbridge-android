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
#include <unordered_map>
#include <algorithm>
#include <string>

static std::unordered_map<std::string, JavaTypeId> sJavaNameToID = {
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

static std::unordered_map<JavaTypeId, std::string> createInversedMap() {
  std::unordered_map<JavaTypeId, std::string> ret;
  for (const auto &p : sJavaNameToID) {
    std::string s = p.first;
    std::replace(s.begin(), s.end(), '.', '/');
    ret[p.second] = s;
  }
  return ret;
}

static std::unordered_map<JavaTypeId, std::string> sIdToJavaName = createInversedMap();

static std::string dropLandSemicolon(const std::string& s) {
  if (s.empty() || s[0] != 'L') {
    return s;
  }
  return s.substr(1, s.length() - 2);
}


JavaTypeId getJavaTypeIdByJavaName(const std::string &javaName) {
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

const std::string &getJavaNameByJavaTypeId(JavaTypeId id) {
  return sIdToJavaName[id];
}
