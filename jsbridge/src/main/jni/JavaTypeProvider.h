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
#ifndef _JSBRIDGE_JAVATYPEPROVIDER_H
#define _JSBRIDGE_JAVATYPEPROVIDER_H

#include "JavaTypeId.h"
#include "jni-helpers/JniGlobalRef.h"
#include "jni-helpers/JniLocalRef.h"
#include "jni-helpers/JniTypes.h"
#include <memory>
#include <unordered_map>

class JsBridgeContext;
class JavaType;

namespace JavaTypes {
  class Deferred;
}

// Manages the JavaType instances for a particular JsBridgeContext.
class JavaTypeProvider {
public:
  JavaTypeProvider() = delete;
  JavaTypeProvider(const JsBridgeContext *);

  JavaTypeProvider(const JavaTypeProvider &) = delete;
  JavaTypeProvider &operator = (const JavaTypeProvider &) = delete;

  const JavaType *newType(const JniRef<jsBridgeParameter> &, bool boxed = false) const;
  std::unique_ptr<const JavaType> makeUniqueType(const JniRef<jsBridgeParameter> &, bool boxed) const;

  const std::unique_ptr<const JavaType> &getObjectType() const;
  std::unique_ptr<const JavaType> getDeferredType(const JniRef<jsBridgeParameter> &) const;

  const JniGlobalRef<jclass> &getJavaClass(JavaTypeId) const;

private:
  const JsBridgeContext *m_jsBridgeContext;
  JavaTypeId getJavaTypeId(const JniRef<jsBridgeParameter> &) const;
  JniLocalRef<jsBridgeParameter> getGenericParameter(const JniRef<jsBridgeParameter> &) const;
  std::unique_ptr<const JavaType> getGenericParameterType(const JniRef<jsBridgeParameter> &) const;
  mutable std::unique_ptr<const JavaType> m_objectType;
  mutable std::unordered_map<JavaTypeId, JniGlobalRef<jclass>> m_javaClasses;
};

#endif
