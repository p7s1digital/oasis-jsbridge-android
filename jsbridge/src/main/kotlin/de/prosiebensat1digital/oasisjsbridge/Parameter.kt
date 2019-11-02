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
package de.prosiebensat1digital.oasisjsbridge

import kotlin.reflect.*
import kotlin.reflect.full.memberFunctions

// Represents a (reflected) function parameter (or return value) with its (optional) name based on:
// - (ideally) Kotlin KParameter or KType which has the (full) reflection info
// - Java Class which has the reflection info without generic type info
internal open class Parameter private constructor(
    internal val kotlinType: KType?,
    private val javaClass: Class<*>?,
    val name: String?
) {
    constructor(kotlinType: KType)
        : this(kotlinType, findJavaClass(kotlinType), null)

    constructor(kotlinParameter: KParameter) : this(
        kotlinParameter.type,
        findJavaClass(kotlinParameter.type),
        kotlinParameter.name
    )

    constructor(javaClass: Class<*>) : this(null, javaClass, javaClass.name)

    @Suppress("UNUSED")  // Called from JNI
    fun getJava(): Class<*>? {
        return javaClass
    }

    @Suppress("UNUSED")  // Called from JNI
    fun getJavaName(): String? {
        return javaClass?.name
    }

    @Suppress("UNUSED")  // Called from JNI
    fun isNullable(): Boolean {
        return kotlinType?.isMarkedNullable == true
    }


    // For Lambdas
    // ---

    // Return the "invoke" method of a lambda parameter or null if it is not a lambda
    // (because the lambda parameter is a FunctionX object with an invoke() method)
    //
    @Suppress("UNUSED")  // Called from JNI
    val invokeMethod: Method? by lazy {
        // Note: kotlin-reflect v1.3.31 and v1.3.40 crash with an exception when calling
        // KClass.memberFunctions for a lambda
        try {
            if (kotlinType != null) {
                val kotlinClass = kotlinType.classifier as? KClass<*>
                val kotlinFunction =
                    kotlinClass?.memberFunctions?.firstOrNull { it.name == "invoke" }
                return@lazy kotlinFunction?.let { Method(it, true) }
            }
        } catch (t: Throwable) {}

        val javaMethod = javaClass?.methods?.firstOrNull { it.name == "invoke" } ?: return@lazy null

        if (kotlinType == null) {
            // Java-only reflection
            return@lazy Method(javaMethod)
        }

        // Add the FunctionX generic arguments to create type info for function parameters
        Method(javaMethod, kotlinType.arguments, true)
    }

    // Return the component type of an array (e.g. for varargs parameters)
    //
    // e.g.: if the parameter is vararg Int, return Int
    @Suppress("UNUSED")  // Called from JNI
    fun getComponentType(): Parameter? {
        val javaComponentType = javaClass?.componentType
        if (javaComponentType?.isPrimitive == true) {
            return Parameter(javaComponentType)
        }

        return if (kotlinType == null) {
            if (javaComponentType == null) {
                // No type information => using generic Object type
                Parameter(Any::class.java)
            } else {
                Parameter(javaComponentType)
            }
        } else {
            // Use KType instance to create the (only) generic type
            kotlinType.arguments.firstOrNull()?.type?.let { genericParameterType ->
                Parameter(genericParameterType)
            }
        }
    }

    // For Deferred
    // TODO: replace it into more "generic" method like "getGenericParameters()"
    // ---

    // Return the JavaClass of the first generic parameters
    //
    // e.g.: if the parameter is a Deferred<String>, return String::class.java
    @Suppress("UNUSED")  // Called from JNI
    fun getGenericParameter(): Parameter? {
        return if (kotlinType == null) {
            // No type information => using generic Object type
            Parameter(Any::class.java)
        } else {
            // Use KType instance to create the (only) generic type
            kotlinType.arguments.firstOrNull()?.type?.let { genericParameterType ->
                Parameter(genericParameterType)
            }
        }
    }
}


// Private
// ---

private fun findJavaClass(kotlinType: KType): Class<*>? {
    return when (val kotlinClassifier = kotlinType.classifier) {
        is KType -> {
            findJavaClass(kotlinClassifier)
        }
        is KClass<*> -> {
            if (kotlinType.toString().startsWith("kotlin.Array")) {
                // Workaround for wrong reflection issue where Array<Int> is mapped to Java int[] instead of Integer[]
                val componentClass = kotlinType.arguments.firstOrNull()?.type?.classifier as? KClass<*>
                if (componentClass?.java?.isPrimitive == true) {
                    val componentJvmName = componentClass.javaObjectType.canonicalName ?: "java.lang.Object"
                    Class.forName("[L$componentJvmName;")
                } else {
                    kotlinClassifier.javaObjectType
                }
            } else {
                kotlinClassifier.java
            }
        }
        else -> null
    }
}
