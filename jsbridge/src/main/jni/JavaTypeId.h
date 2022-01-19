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
  Int = 11,
  Long = 12,
  Float = 13,
  Double = 14,

  BoxedBoolean = 20,
  BoxedInt = 21,
  BoxedLong = 22,
  BoxedFloat = 23,
  BoxedDouble = 24,

  String = 30,
  Object = 40,

  ObjectArray = 50,

  BooleanArray = 60,
  IntArray = 61,
  LongArray = 62,
  FloatArray = 63,
  DoubleArray = 64,

  DebugString = 90,
  FunctionX = 100,
  JsValue = 101,
  JsonObjectWrapper = 102,
  Deferred = 103,

  AidlInterface = 201,
  AidlParcelable = 202,
};

JavaTypeId getJavaTypeIdByJavaName(std::u16string_view javaName);
const std::string &getJniClassNameByJavaTypeId(JavaTypeId id);

#endif
