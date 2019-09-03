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
#include "Primitive.h"
#include "JniCache.h"
#include "JsBridgeContext.h"
#include "jni-helpers/JniContext.h"

namespace JavaTypes {

Primitive::Primitive(const JsBridgeContext *jsBridgeContext, JavaTypeId primitiveId, JavaTypeId boxedId)
 : JavaType(jsBridgeContext, primitiveId)
 , m_boxedId(boxedId) {
}

const JniRef<jclass> &Primitive::getBoxedJavaClass() const {
  return m_jsBridgeContext->getJniCache()->getJavaClass(m_boxedId);
}

}  // namespace JavaTypes

