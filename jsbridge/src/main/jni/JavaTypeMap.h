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
#ifndef _JSBRIDGE_JAVATYPEMAP_H
#define _JSBRIDGE_JAVATYPEMAP_H

#include "jni-helpers/JniGlobalRef.h"
#include <jni-helpers/JniTypes.h>
#include <jni.h>
#include <map>

class JsBridgeContext;
class JavaType;

namespace JavaTypes {
  class Deferred;
}

// Manages the JavaType instances for a particular JsBridgeContext.
class JavaTypeMap {
public:
  JavaTypeMap() = default;
  JavaTypeMap(const JavaTypeMap &) = delete;
  JavaTypeMap &operator = (const JavaTypeMap &) = delete;
  ~JavaTypeMap();

  /** Get the JavaType to use to marshal instances of {@code javaClass}. */
  const JavaType *get(const JsBridgeContext *, const JniRef<jclass> &javaClass) const;
  /** Get the JavaType to use to marshal instances of {@code javaClass}, force boxed primitives. */
  const JavaType *getBoxed(const JsBridgeContext *, const JniRef<jclass> &javaClass) const;
  /** Get the JavaType that represents Object. */
  const JavaType *getObjectType(const JsBridgeContext *) const;
  /** Get the Deferred type */
  const JavaTypes::Deferred *getDeferredType(const JsBridgeContext *) const;

private:
  const JavaType *find(const JsBridgeContext *, const std::string &) const;

  mutable std::map<std::string, const JavaType *> m_types;
};

#endif
