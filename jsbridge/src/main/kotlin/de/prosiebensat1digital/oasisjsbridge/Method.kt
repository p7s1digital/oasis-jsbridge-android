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
import kotlin.reflect.full.createType
import java.lang.reflect.Method as JavaMethod
import kotlin.reflect.full.valueParameters
import kotlin.reflect.jvm.javaMethod

// Represents a (reflected) method based on:
// - (ideally) Kotlin KFunction which has the (full) reflection info
// - Java Method which has the reflection info without generic type info
//
// It provides access to the reflected method parameters (Parameter) for both
// arguments and return value.
internal class Method {
    val name: String?

    val javaMethod: JavaMethod?

    @Suppress("UNUSED")  // Called from JNI
    var returnParameter: Parameter
        internal set

    @Suppress("UNUSED")  // Called from JNI
    val parameters: Array<Parameter>

    @Suppress("UNUSED")  // Called from JNI
    val isVarArgs: Boolean

    // KFunction with full reflection information
    constructor(kotlinFunction: KFunction<*>, isLambda: Boolean) {
        this.javaMethod = kotlinFunction.javaMethod ?: throw Throwable("Given kotlin Function does not have any Java method")
        this.name = kotlinFunction.name

        // The return type of Unit functions must be "Unit" for lambdas but "Void" for methods
        val returnType = if (!isLambda && kotlinFunction.returnType.classifier == Unit::class) Void::class.createType() else kotlinFunction.returnType
        this.returnParameter = Parameter(this, returnType)

        this.parameters = kotlinFunction.valueParameters.map { Parameter(this, it) }.toTypedArray()
        this.isVarArgs = kotlinFunction.valueParameters.lastOrNull()?.isVararg == true
    }

    // Java-only reflection
    constructor(javaMethod: JavaMethod) {
        this.name = javaMethod.name
        this.javaMethod = javaMethod
        this.returnParameter = Parameter(this, javaMethod.returnType)
        this.parameters = javaMethod.parameterTypes.map { Parameter(this, it) }.toTypedArray()
        this.isVarArgs = javaMethod.isVarArgs
    }

    // JavaMethod with generic arguments (used for lambda Parameter's)
    //
    // e.g. for (String, Int) -> Double
    // - JavaMethod -> Function2.class.java
    // - genericArguments -> [in String, in Int, out Double]
    //
    // Ideally, the projection is OUT for return values and IN for parameters. The lambda parameters
    // arguments are reflected with the INVARIANT projection, though. In that case the asumption is
    // that the return type is the last argument.
    constructor(javaMethod: JavaMethod, genericArguments: List<KTypeProjection>, isLambda: Boolean) {
        this.javaMethod = javaMethod

        this.name = javaMethod.name
        this.isVarArgs = javaMethod.isVarArgs

        var outParameterType: KType? = null
        val inParameterTypes = mutableListOf<KType>()

        for (kotlinTypeProjection in genericArguments) {
            val genericArgumentType = kotlinTypeProjection.type ?: continue

            when (kotlinTypeProjection.variance) {
                KVariance.OUT -> outParameterType = genericArgumentType
                KVariance.IN -> inParameterTypes.add(genericArgumentType)
                KVariance.INVARIANT -> {
                    if (kotlinTypeProjection === genericArguments.last()) {
                        // Last generic parameter is the return type
                        outParameterType = genericArgumentType
                    } else {
                        inParameterTypes.add(genericArgumentType)
                    }
                }
            }
        }

        // The return type of Unit functions must be "Unit" for lambdas but "Void" for methods
        if (!isLambda && outParameterType?.classifier == Unit::class) outParameterType = Void::class.createType()

        // Create the parameters and always wrap them as Object
        this.returnParameter = outParameterType?.let { Parameter(this, it) }
            ?: Parameter(this, Unit.javaClass)

        this.parameters = inParameterTypes.map { Parameter(this, it) }
            .toTypedArray()
    }

    constructor(parameters: Array<Parameter>, returnParameter: Parameter, isVarArgs: Boolean) {
        this.name = null
        this.isVarArgs = isVarArgs
        this.javaMethod = null
        this.parameters = parameters
        this.returnParameter = returnParameter
    }

    @Suppress("UNUSED")  // Called from JNI
    fun callNativeLambda(obj: Any, args_: Array<Any?>): Any? {
        val func = obj as? Function<*>
                ?: throw JsBridgeError.InternalError(customMessage = "Cannot call native lambda: the given object is not a Function!")

        val args = if (args_.size < parameters.size) {
            args_.copyOf(parameters.size)
        } else {
            args_
        }

        @Suppress("UNCHECKED_CAST")
        return when (parameters.size) {
            0 -> (func as Function0<*>).invoke()
            1 -> (func as Function1<Any?, *>).invoke(args[0])
            2 -> (func as Function2<Any?, Any?, *>).invoke(args[0], args[1])
            3 -> (func as Function3<Any?, Any?, Any?, *>).invoke(args[0], args[1], args[2])
            4 -> (func as Function4<Any?, Any?, Any?, Any?, *>).invoke(args[0], args[1], args[2], args[3])
            5 -> (func as Function5<Any?, Any?, Any?, Any?, Any?, *>).invoke(args[0], args[1], args[2], args[3], args[4])
            6 -> (func as Function6<Any?, Any?, Any?, Any?, Any?, Any?, *>).invoke(args[0], args[1], args[2], args[3], args[4], args[5])
            7 -> (func as Function7<Any?, Any?, Any?, Any?, Any?, Any?, Any?, *>).invoke(args[0], args[1], args[2], args[3], args[4], args[5], args[6])
            8 -> (func as Function8<Any?, Any?, Any?, Any?, Any?, Any?, Any?, Any?, *>).invoke(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7])
            9 -> (func as Function9<Any?, Any?, Any?, Any?, Any?, Any?, Any?, Any?, Any?, *>).invoke(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8])
            else -> throw JsBridgeError.InternalError(customMessage = "Cannot call native lambda: only functions with up to 9 arguments are supported!")
        }
    }

    // Return a function which triggers the given method with parameters collected into an array.
    //
    // E.g.: if the method is (name: String, age: Int) -> Unit, the returned value is:
    // { name: String, age: Int ->
    //    cb(arrayOf(name, age))
    // }
    @Throws
    fun asFunctionWithArgArray(cb: (args: Array<Any?>) -> Any?): Function<Any?>? {
        return when (parameters.count()) {
            0 -> ({
                cb(arrayOf())
            })
            1 -> ({ p1: Any? ->
                cb(arrayOf(p1))
            })
            2 -> ({ p1: Any?, p2: Any? ->
                cb(arrayOf(p1, p2))
            })
            3 -> ({ p1: Any?, p2: Any?, p3: Any? ->
                cb(arrayOf(p1, p2, p3))
            })
            4 -> ({ p1: Any?, p2: Any?, p3: Any?, p4: Any? ->
                cb(arrayOf(p1, p2, p3, p4))
            })
            5 -> ({ p1: Any?, p2: Any?, p3: Any?, p4: Any?, p5: Any? ->
                cb(arrayOf(p1, p2, p3, p4, p5))
            })
            6 -> ({ p1: Any?, p2: Any?, p3: Any?, p4: Any?, p5: Any?, p6: Any? ->
                cb(arrayOf(p1, p2, p3, p4, p5, p6))
            })
            7 -> ({ p1: Any?, p2: Any?, p3: Any?, p4: Any?, p5: Any?, p6: Any?, p7: Any? ->
                cb(arrayOf(p1, p2, p3, p4, p5, p6, p7))
            })
            8 -> ({ p1: Any?, p2: Any?, p3: Any?, p4: Any?, p5: Any?, p6: Any?, p7: Any?, p8: Any? ->
                cb(arrayOf(p1, p2, p3, p4, p5, p6, p7, p8))
            })
            9 -> ({ p1: Any?, p2: Any?, p3: Any?, p4: Any?, p5: Any?, p6: Any?, p7: Any?, p8: Any?, p9: Any? ->
                cb(arrayOf(p1, p2, p3, p4, p5, p6, p7, p8, p9))
            })
            else -> throw Throwable("Function with X > 9 is not supported!")
        }
    }
}
