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
#ifndef _JSBRIDGE_JAVATYPEID_H
#define _JSBRIDGE_JAVATYPEID_H

#include <string>

enum class JavaTypeId {
  Unknown = 0,

  Void = 1,
  BoxedVoid = 2,
  Unit = 3,

  Boolean = 10,
  Byte = 11,
  Int = 12,
  Long = 13,
  Float = 14,
  Double = 15,

  BoxedBoolean = 20,
  BoxedByte = 21,
  BoxedInt = 22,
  BoxedLong = 23,
  BoxedFloat = 24,
  BoxedDouble = 25,

  String = 30,
  Object = 40,

  ObjectArray = 50,
  List = 51,

  BooleanArray = 60,
  ByteArray = 61,
  IntArray = 62,
  LongArray = 63,
  FloatArray = 64,
  DoubleArray = 65,

  DebugString = 90,
  FunctionX = 100,
  JsValue = 101,
  JsonObjectWrapper = 102,
  Deferred = 103,
  JavaObjectWrapper = 104,
  JsToJavaProxy = 105,
};

JavaTypeId getJavaTypeIdByJavaName(std::u16string_view javaName);
const std::string &getJniClassNameByJavaTypeId(JavaTypeId id);

#endif
