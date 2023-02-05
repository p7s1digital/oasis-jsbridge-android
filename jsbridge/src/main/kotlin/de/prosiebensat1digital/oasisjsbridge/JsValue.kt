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

import de.prosiebensat1digital.oasisjsbridge.JsBridgeError.*
import kotlinx.coroutines.*
import java.lang.ref.WeakReference
import java.util.concurrent.atomic.AtomicInteger
import kotlin.reflect.typeOf
import kotlin.coroutines.CoroutineContext
import kotlin.coroutines.EmptyCoroutineContext
import kotlin.reflect.KType
import kotlin.reflect.full.createType

/**
 * A simple wrapper around a JS value stored as a global JS variable and deleted
 * when the Java object is finalized.
 *
 * This is useful for JS values which need to be transfered to the Java/Kotlin world
 * but do not need to be converted to a JVM type or needs to be evaluated at a later stage.
 * The corresponding JS variable named "associatedJsName" is only valid during the lifetime of the Java
 * object and you should never explicitly refence it in your JS code.
 *
 * Note: the initial JS code must be successfully evaluated via JS eval(). It implies that an object
 * or an anonymous function must be surrounded by brackets, e.g.:
 * - `val jsObject = JsValue(jsBridge, "({ a: 1, b: 'two' })")
 * - `val jsFunction = JsValue(jsBridge, "(function(a, b) { return a + b; })")
 */
open class JsValue
internal constructor(
    jsBridge: JsBridge,
    jsCode: String?,
    val associatedJsName: String
) {
    // Create a JsValue without initial value
    internal constructor(jsBridge: JsBridge)
            : this(jsBridge, jsCode = null, associatedJsName = generateJsGlobalName())

    /**
     * Create a JsValue with an initial value (from JS code)
     */
    constructor(jsBridge: JsBridge, jsCode: String)
            : this(jsBridge, jsCode = jsCode, associatedJsName = generateJsGlobalName())

    private var jsBridgeRef = WeakReference(jsBridge)
    val jsBridge: JsBridge? get() = jsBridgeRef.get()

    internal var codeEvaluationDeferred: Deferred<Unit>? = jsCode?.let {
        jsBridge.assignJsValueAsync(this@JsValue, jsCode)
    }

    companion object {
        private var internalCounter = AtomicInteger(0)

        /**
         * Create a JsValue which is a JS function as created with JS "new Function(args..., code)", e.g.:
         * val jsValue = JsValue.newFunction(jsBridge, "a", "b", "return a + b;")
         */
        fun newFunction(jsBridge: JsBridge, vararg args: String): JsValue {
            val jsValue = JsValue(jsBridge)
            jsValue.codeEvaluationDeferred = if (args.isEmpty()) {
                jsBridge.newJsFunctionAsync(jsValue, arrayOf(), "")
            } else {
                jsBridge.newJsFunctionAsync(jsValue, args.take(args.size - 1).toTypedArray(), args.last())
            }

            return jsValue
        }


        // Proxy Java to JS object
        // ---

        /**
         * Create a JsValue which is a JS proxy to a Java object
         *
         * Note: the Java methods will be called in the JS thread!
         */
        inline fun <reified T: JsToJavaInterface> createJsToJavaProxy(jsBridge: JsBridge, javaObject: T): JsToJavaProxy<T> {
            return jsBridge.registerJsToJavaInterface(T::class, javaObject)
        }

        /**
         * Create a JsValue which is a JS proxy to a Java object.
         *
         * Notes:
         * - jsToJavaIinterface must be an interface implementing JsToJavaInterface
         * - the Java object must implement the "jsToNJavaInterface" interface
         * - the Java methods will be called in the JS thread!
         * - from Kotlin, it is recommended to use the method overload with generic parameter,
         * instead!
         */
        @JvmStatic
        fun <T: JsToJavaInterface> createJsToJavaProxy(jsBridge: JsBridge, javaObject: T, jsToJavaInterface: Class<*>): JsToJavaProxy<T> {
            return jsBridge.registerJsToJavaInterface(jsToJavaInterface.kotlin, javaObject)
        }


        // Proxy Java to JS lambda
        // ---

        // Note: as reflection is still not well supported for lambdas, we cannot use typeOf<F>()
        //inline fun <reified F: Function<*>> createJsToJavaProxyFunction(func: F): JsValue {
        //    return jsBridge.registerJsToJavaFunction(func, typeOf<F>())
        //}

        @OptIn(ExperimentalStdlibApi::class)
        inline fun <reified R> createJsToJavaProxyFunction0(jsBridge: JsBridge, noinline func: () -> R): JsValue {
            return jsBridge.registerJsToJavaFunction(func, listOf(typeOf<R>()))
        }

        @OptIn(ExperimentalStdlibApi::class)
        inline fun <reified P1, reified R> createJsToJavaProxyFunction1(jsBridge: JsBridge, noinline func: (p1: P1) -> R): JsValue {
            return jsBridge.registerJsToJavaFunction(func, listOf(typeOf<P1>(), typeOf<R>()))
        }

        @OptIn(ExperimentalStdlibApi::class)
        inline fun <reified P1, reified P2, reified R> createJsToJavaProxyFunction2(jsBridge: JsBridge, noinline func: (p1: P1, p2: P2) -> R): JsValue {
            return jsBridge.registerJsToJavaFunction(func, listOf(typeOf<P1>(), typeOf<P2>(), typeOf<R>()))
        }

        @OptIn(ExperimentalStdlibApi::class)
        inline fun <reified P1, reified P2, reified P3, reified R> createJsToJavaProxyFunction3(jsBridge: JsBridge, noinline func: (p1: P1, p2: P2, p3: P3) -> R): JsValue {
            return jsBridge.registerJsToJavaFunction(func, listOf(typeOf<P1>(), typeOf<P2>(), typeOf<P3>(), typeOf<R>()))
        }

        @OptIn(ExperimentalStdlibApi::class)
        inline fun <reified P1, reified P2, reified P3, reified P4, reified R> createJsToJavaProxyFunction4(jsBridge: JsBridge, noinline func: (p1: P1, p2: P2, p3: P3, p4: P4) -> R): JsValue {
            return jsBridge.registerJsToJavaFunction(func, listOf(typeOf<P1>(), typeOf<P2>(), typeOf<P3>(), typeOf<P4>(), typeOf<R>()))
        }

        @OptIn(ExperimentalStdlibApi::class)
        inline fun <reified P1, reified P2, reified P3, reified P4, reified P5, reified R> createJsToJavaProxyFunction5(jsBridge: JsBridge, noinline func: (p1: P1, p2: P2, p3: P3, p4: P4, p5: P5) -> R): JsValue {
            return jsBridge.registerJsToJavaFunction(func, listOf(typeOf<P1>(), typeOf<P2>(), typeOf<P3>(), typeOf<P4>(), typeOf<P5>(), typeOf<R>()))
        }

        @OptIn(ExperimentalStdlibApi::class)
        inline fun <reified P1, reified P2, reified P3, reified P4, reified P5, reified P6, reified R> createJsToJavaProxyFunction6(jsBridge: JsBridge, noinline func: (p1: P1, p2: P2, p3: P3, p4: P4, p5: P5, p6: P6) -> R): JsValue {
            return jsBridge.registerJsToJavaFunction(func, listOf(typeOf<P1>(), typeOf<P2>(), typeOf<P3>(), typeOf<P4>(), typeOf<P5>(), typeOf<P6>(), typeOf<R>()))
        }

        @OptIn(ExperimentalStdlibApi::class)
        inline fun <reified P1, reified P2, reified P3, reified P4, reified P5, reified P6, reified P7, reified R> createJsToJavaProxyFunction7(jsBridge: JsBridge, noinline func: (p1: P1, p2: P2, p3: P3, p4: P4, p5: P5, p6: P6, p7: P7) -> R): JsValue {
            return jsBridge.registerJsToJavaFunction(func, listOf(typeOf<P1>(), typeOf<P2>(), typeOf<P3>(), typeOf<P4>(), typeOf<P5>(), typeOf<P6>(), typeOf<P7>(), typeOf<R>()))
        }

        @OptIn(ExperimentalStdlibApi::class)
        inline fun <reified P1, reified P2, reified P3, reified P4, reified P5, reified P6, reified P7, reified P8, reified R> createJsToJavaProxyFunction8(jsBridge: JsBridge, noinline func: (p1: P1, p2: P2, p3: P3, p4: P4, p5: P5, p6: P6, p7: P7, p8: P8) -> R): JsValue {
            return jsBridge.registerJsToJavaFunction(func, listOf(typeOf<P1>(), typeOf<P2>(), typeOf<P3>(), typeOf<P4>(), typeOf<P5>(), typeOf<P6>(), typeOf<P7>(), typeOf<P8>(), typeOf<R>()))
        }

        @OptIn(ExperimentalStdlibApi::class)
        inline fun <reified P1, reified P2, reified P3, reified P4, reified P5, reified P6, reified P7, reified P8, reified P9, reified R> createJsToJavaProxyFunction9(jsBridge: JsBridge, noinline func: (p1: P1, p2: P2, p3: P3, p4: P4, p5: P5, p6: P6, p7: P7, p8: P8, p9: P9) -> R): JsValue {
            return jsBridge.registerJsToJavaFunction(func, listOf(typeOf<P1>(), typeOf<P2>(), typeOf<P3>(), typeOf<P4>(), typeOf<P5>(), typeOf<P6>(), typeOf<P7>(), typeOf<P8>(), typeOf<P9>(), typeOf<R>()))
        }


        // Convert Java value to JSValue
        // ---

        @OptIn(ExperimentalStdlibApi::class)
        inline fun <reified T> fromJavaValue(jsBridge: JsBridge, javaValue: T): JsValue {
            return jsBridge.convertJavaValueToJs(javaValue, Parameter(typeOf<T>(), jsBridge.customClassLoader))
        }

        @JvmStatic
        fun fromJavaValue(jsBridge: JsBridge, javaValue: Any, javaClass: Class<*>): JsValue {
            return jsBridge.convertJavaValueToJs(javaValue, Parameter(javaClass, jsBridge.customClassLoader))
        }


        // Private
        // ---

        @JvmStatic
        protected fun generateJsGlobalName(): String {
            val suffix = internalCounter.incrementAndGet()
            return "__jsBridge_jsValue$suffix"
        }
    }

    protected fun finalize() {
        release()
        jsBridgeRef.clear()
    }

    /**
     * Make sure that the instance is retained.
     * This is useful when the JsValue is not stored and you want to prevent it from being
     * garbage-collected (and thus deleting its associated JS value) while evaluating it via
     * its associated JS name (see toString())
     *
     * e.g.:
     * val jsValue = JsValue(jsBridge, "123")
     * ...
     * val ret = jsBridge.evaluate<Int>("$jsValue + 456")
     *
     * // Note: JsValue may be garbage-collected while performing the above evaluation in the JS thread...
     * jsValue.hold()  // ...unless we ensure that the instance is held!
     */
    fun hold() = Unit

    /**
     * Delete a JsValue via deleting the associated (global) JS variable. This can either be
     * called manually or automatically when the JsValue has been garbage-collected
     */
    fun release() {
        val jsBridge = jsBridgeRef.get() ?: run {
            //Timber.v("No need to delete JsValue $associatedJsName because the JS interpreter has been deleted!")
            return
        }

        //Timber.v("Deleting JsValue $associatedJsName")
        jsBridge.deleteJsValue(this)
    }

    /**
     * Return the associated JS name. Please be aware that the variable is only valid as long as
     * the JsValue instance exists. If the string is evaluated after JsValue has been garbage-collected,
     * the JS variable returned by toString() will be deleted before the evaluation!
     * To ensure that a JsValue still exists, you can for example use JsValue.hold()
     */
    override fun toString() = """globalThis["$associatedJsName"]"""

    override fun equals(other: Any?): Boolean {
        if (other !is JsValue) return false
        return associatedJsName == other.associatedJsName
    }

    override fun hashCode(): Int = associatedJsName.hashCode()

    fun copyTo(other: JsValue) {
        jsBridge?.copyJsValue(other.associatedJsName, this@JsValue)
    }

    fun assignToGlobal(globalName: String) {
        jsBridge?.copyJsValue(globalName, this@JsValue)
    }

    @OptIn(ExperimentalStdlibApi::class)
    suspend inline fun <reified T: Any?> evaluate(): T {
        val jsBridge = jsBridge
                ?: throw JsValueEvaluationError(associatedJsName, customMessage = "Cannot evaluate JS value because the JS interpreter has been destroyed")

        return jsBridge.evaluateJsValue(this, typeOf<T>(), true)
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T: Any?> evaluateBlocking(context: CoroutineContext = EmptyCoroutineContext): T {
        val jsBridge = jsBridge
                ?: throw JsValueEvaluationError(associatedJsName, customMessage = "Cannot evaluate JS value because the JS interpreter has been destroyed")

        return runBlocking(context) {
            jsBridge.evaluateJsValue(this@JsValue, typeOf<T>(), true)
        }
    }

    // Notes:
    // - runs in the JS thread and block the caller thread until the result has been evaluated!
    // - from Kotlin, it is recommended to use the method with generic parameter, instead!
    fun evaluateBlocking(javaClass: Class<*>?): Any? {
        val jsBridge = jsBridge
                ?: throw JsValueEvaluationError(associatedJsName, customMessage = "Cannot evaluate JS value because the JS interpreter has been destroyed")

        return runBlocking {
            jsBridge.evaluateJsValue(this@JsValue, javaClass?.kotlin?.createType(), false)
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T: Any> evaluateAsync(): Deferred<T> {
        val jsBridge = jsBridge

        if (jsBridge == null) {
            val rejectedDeferred = CompletableDeferred<T>()
            val error = InternalError("Cannot evaluate JS value because the JS interpreter has been destroyed")
            rejectedDeferred.completeExceptionally(error)
            return rejectedDeferred
        }

        return jsBridge.evaluateJsValueAsync(this, typeOf<T>())
    }

    /**
     * Await a JS promise:
     * - if the JS value is a promise, wait until the promise has been resolved (or rejected) and return a new JsValue
     * - if the JS value is not a promise, return a new JsValue referencing the current one
     */
    suspend inline fun await(): JsValue {
        return evaluate()
    }

    // Same as await() but can be used outside of coroutines and return a Deferred value
    fun awaitAsync(): Deferred<JsValue> {
        return evaluateAsync()
    }


    // Proxy JS to Java object
    // ---

    /**
     * Create a Java proxy to a JS object (without checks)
     *
     * Notes:
     * - the proxy object will be returned even if the JS object is invalid or does not implement
     * all of the methods of the JavaToJsInterface
     * - the non-suspend methods of the returned proxy object will be running in the JS thread and
     * block the caller thread if the return value is not Unit or Deferred
     */
    inline fun <reified T: JavaToJsInterface> createJavaToJsProxy(): T {
        val jsBridge = jsBridge
                ?: throw JavaToJsInterfaceRegistrationError(T::class, customMessage = "Cannot create a Java-to-JS interface proxy because the JS interpreter has been destroyed")

        // Note: as check = false, the method will actually directly return without blocking the
        // current thread
        return jsBridge.registerJavaToJsInterfaceBlocking(this, T::class, false, null)
    }

    /**
     * Create a Java proxy to a JS object
     *
     * Notes:
     * - when check = true, this method will throw an exception if the JS object does not implement
     * all the methods of the JavaToJsInterface
     * - when check = false, the proxy object will be returned even if the JS object is invalid or
     * does not implement all of the methods of the JavaToJsInterface
     * - the non-suspend methods of the returned proxy object will be running in the JS thread and
     * block the caller thread if the return value is not Unit or Deferred
     */
    suspend inline fun <reified T: JavaToJsInterface> createJavaToJsProxy(check: Boolean): T {
        val jsBridge = jsBridge
                ?: throw JavaToJsInterfaceRegistrationError(T::class, customMessage = "Cannot create a Java-to-JS interface proxy because the JS interpreter has been destroyed")

        return jsBridge.registerJavaToJsInterface(this, T::class, check)
    }

    /**
     * Create a Java proxy to a JS object (blocking)
     * -> see JsValue.createJavaToJsProxy(check: Boolean)
     */
    inline fun <reified T: JavaToJsInterface> createJavaToJsProxyBlocking(check: Boolean, context: CoroutineContext? = null): T {
        val jsBridge = jsBridge
                ?: throw JavaToJsInterfaceRegistrationError(T::class, customMessage = "Cannot create a Java-to-JS interface proxy because the JS interpreter has been destroyed")

        return jsBridge.registerJavaToJsInterfaceBlocking(this, T::class, check, context)
    }

    /**
     * Create a Java proxy to a JS object
     *
     * Notes:
     * - the proxy object will be returned even if the JS object is invalid or
     * does not implement/ all of the methods of the JavaToJsInterface
     * - the methods of the returned proxy object will be running in the JS thread and block the
     * caller thread if there is a return value
     * - from Kotlin, it is recommended to use the method with generic parameter, instead!
     */
    fun <T: JavaToJsInterface> createJavaToJsProxy(javaToJsInterface: Class<T>): T {
        val jsBridge = jsBridge
                ?: throw JavaToJsInterfaceRegistrationError(javaToJsInterface.kotlin, customMessage = "Cannot create a Java-to-JS interface proxy because the JS interpreter has been destroyed")

        // Note: as check = false, the method will actually directly return without blocking the
        // current thread
        return jsBridge.registerJavaToJsInterfaceBlocking(this, javaToJsInterface.kotlin, false, null)
    }

    /**
     * Create a Java proxy to a JS object
     *
     * Notes:
     * - this method will block the current thread until the object has been registered
     * - when check = true, this method will block the current thread until the object has been
     * registered or will throw an exception if the JS object does not implement  all the methods of
     * the JavaToJsInterface
     * - when check = false, the proxy object will be returned even if the JS object is invalid or
     * does not implement/ all of the methods of the JavaToJsInterface
     * - the methods of the returned proxy object will be running in the JS thread and block the
     * caller thread if there is a return value
     * - from Kotlin, it is recommended to use the method with generic parameter, instead!
     */
    @JvmOverloads
    fun <T: JavaToJsInterface> createJavaToJsProxyBlocking(javaToJsInterface: Class<T>, check: Boolean, context: CoroutineContext? = null): T {
        val jsBridge = jsBridge
                ?: throw JavaToJsInterfaceRegistrationError(javaToJsInterface.kotlin, customMessage = "Cannot create a Java-to-JS interface proxy because the JS interpreter has been destroyed")

        return jsBridge.registerJavaToJsInterfaceBlocking(this, javaToJsInterface.kotlin, check, context)
    }


    // Proxy JS to Java function
    // ---

    // Unfortunately crashes with latest Kotlin (1.3.40) because typeOf<>() does not work yet for suspending functions,
    // that's why we have to use the registerJsFunctionX() methods below (X = parameter count)
    //
    //inline fun <reified F: Function<*>> createJavaToJsProxyFunction(kFunction = typeOf<F>()): F {
    //  ...
    //}


    // Proxy JS to Java function (0)
    // ---

    @OptIn(ExperimentalStdlibApi::class)
    suspend inline fun <reified R> createJavaToJsProxyFunction0(waitForRegistration: Boolean): suspend () -> R {
        val functionWithParamArray = createJavaToJsProxyFunctionHelper<R>(listOf(typeOf<R>()), waitForRegistration)
        return {
            functionWithParamArray(arrayOf())
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified R> createJavaToJsProxyFunction0(): suspend () -> R {
        val functionWithParamArray = createJavaToJsProxyFunctionAsyncHelper<R>(listOf(typeOf<R>()))
        return {
            functionWithParamArray(arrayOf())
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified R> createJavaToJsBlockingProxyFunction0(context: CoroutineContext? = null): () -> R {
        val types = listOf(typeOf<R>())
        val functionWithParamArray = createJavaToJsBlockingProxyFunctionHelper<R>(types, context)
        return {
            functionWithParamArray(arrayOf())
        }
    }


    // Proxy JS to Java function (1)
    // ---

    @OptIn(ExperimentalStdlibApi::class)
    suspend inline fun <reified T1, reified R> createJavaToJsProxyFunction1(waitForRegistration: Boolean): suspend (T1) -> R {
        val types = listOf(typeOf<T1>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionHelper<R>(types, waitForRegistration)
        return { p1 ->
            functionWithParamArray(arrayOf(p1))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified R> createJavaToJsProxyFunction1(): suspend (T1) -> R {
        val types = listOf(typeOf<T1>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionAsyncHelper<R>(types)
        return { p1 ->
            functionWithParamArray(arrayOf(p1))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified R> createJavaToJsBlockingProxyFunction1(context: CoroutineContext? = null): (T1) -> R {
        val types = listOf(typeOf<T1>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsBlockingProxyFunctionHelper<R>(types, context)
        return { p1 ->
            functionWithParamArray(arrayOf(p1))
        }
    }


    // Proxy JS to Java function (2)
    // ---

    @OptIn(ExperimentalStdlibApi::class)
    suspend inline fun <reified T1, reified T2, reified R> createJavaToJsProxyFunction2(waitForRegistration: Boolean): suspend (T1, T2) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionHelper<R>(types, waitForRegistration)
        return { p1, p2 ->
            functionWithParamArray(arrayOf(p1, p2))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified R> createJavaToJsProxyFunction2(): suspend (T1, T2) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionAsyncHelper<R>(types)
        return { p1, p2 ->
            functionWithParamArray(arrayOf(p1, p2))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified R> createJavaToJsBlockingProxyFunction2(context: CoroutineContext? = null): (T1, T2) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsBlockingProxyFunctionHelper<R>(types, context)
        return { p1, p2 ->
            functionWithParamArray(arrayOf(p1, p2))
        }
    }


    // Proxy JS to Java function (3)
    // ---

    @OptIn(ExperimentalStdlibApi::class)
    suspend inline fun <reified T1, reified T2, reified T3, reified R> createJavaToJsProxyFunction3(waitForRegistration: Boolean): suspend (T1, T2, T3) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionHelper<R>(types, waitForRegistration)
        return { p1, p2, p3 ->
            functionWithParamArray(arrayOf(p1, p2, p3))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified T3, reified R> createJavaToJsProxyFunction3(): suspend (T1, T2, T3) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionAsyncHelper<R>(types)
        return { p1, p2, p3 ->
            functionWithParamArray(arrayOf(p1, p2, p3))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified T3, reified R> createJavaToJsBlockingProxyFunction3(context: CoroutineContext? = null): (T1, T2, T3) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsBlockingProxyFunctionHelper<R>(types, context)
        return { p1, p2, p3 ->
            functionWithParamArray(arrayOf(p1, p2, p3))
        }
    }


    // Proxy JS to Java function (4)
    // ---

    @OptIn(ExperimentalStdlibApi::class)
    suspend inline fun <reified T1, reified T2, reified T3, reified T4, reified R> createJavaToJsProxyFunction4(waitForRegistration: Boolean): suspend (T1, T2, T3, T4) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionHelper<R>(types, waitForRegistration)
        return { p1, p2, p3, p4 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified T3, reified T4, reified R> createJavaToJsProxyFunction4(): suspend (T1, T2, T3, T4) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionAsyncHelper<R>(types)
        return { p1, p2, p3, p4 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified T3, reified T4, reified R> createJavaToJsBlockingProxyFunction4(context: CoroutineContext? = null): (T1, T2, T3, T4) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsBlockingProxyFunctionHelper<R>(types, context)
        return { p1, p2, p3, p4 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4))
        }
    }


    // Proxy JS to Java function (5)
    // ---

    @OptIn(ExperimentalStdlibApi::class)
    suspend inline fun <reified T1, reified T2, reified T3, reified T4, reified T5, reified R> createJavaToJsProxyFunction5(waitForRegistration: Boolean): suspend (T1, T2, T3, T4, T5) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<T5>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionHelper<R>(types, waitForRegistration)
        return { p1, p2, p3, p4, p5 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4, p5))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified T3, reified T4, reified T5, reified R> createJavaToJsProxyFunction5(): suspend (T1, T2, T3, T4, T5) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<T5>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionAsyncHelper<R>(types)
        return { p1, p2, p3, p4, p5 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4, p5))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified T3, reified T4, reified T5, reified R> createJavaToJsBlockingProxyFunction5(context: CoroutineContext? = null): (T1, T2, T3, T4, T5) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<T5>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsBlockingProxyFunctionHelper<R>(types, context)
        return { p1, p2, p3, p4, p5 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4, p5))
        }
    }


    // Proxy JS to Java function (6)
    // ---

    @OptIn(ExperimentalStdlibApi::class)
    suspend inline fun <reified T1, reified T2, reified T3, reified T4, reified T5, reified T6, reified R> createJavaToJsProxyFunction6(waitForRegistration: Boolean): suspend (T1, T2, T3, T4, T5, T6) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<T5>(), typeOf<T6>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionHelper<R>(types, waitForRegistration)
        return { p1, p2, p3, p4, p5, p6 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4, p5, p6))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified T3, reified T4, reified T5, reified T6, reified R> createJavaToJsProxyFunction6(): suspend (T1, T2, T3, T4, T5, T6) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<T5>(), typeOf<T6>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionAsyncHelper<R>(types)
        return { p1, p2, p3, p4, p5, p6 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4, p5, p6))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified T3, reified T4, reified T5, reified T6, reified R> createJavaToJsBlockingProxyFunction6(context: CoroutineContext? = null): (T1, T2, T3, T4, T5, T6) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<T5>(), typeOf<T6>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsBlockingProxyFunctionHelper<R>(types, context)
        return { p1, p2, p3, p4, p5, p6 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4, p5, p6))
        }
    }


    // Proxy JS to Java function (7)
    // ---

    @OptIn(ExperimentalStdlibApi::class)
    suspend inline fun <reified T1, reified T2, reified T3, reified T4, reified T5, reified T6, reified T7, reified R> createJavaToJsProxyFunction7(waitForRegistration: Boolean): suspend (T1, T2, T3, T4, T5, T6, T7) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<T5>(), typeOf<T6>(), typeOf<T7>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionHelper<R>(types, waitForRegistration)
        return { p1, p2, p3, p4, p5, p6, p7 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4, p5, p6, p7))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified T3, reified T4, reified T5, reified T6, reified T7, reified R> createJavaToJsProxyFunction7(): suspend (T1, T2, T3, T4, T5, T6, T7) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<T5>(), typeOf<T6>(), typeOf<T7>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionAsyncHelper<R>(types)
        return { p1, p2, p3, p4, p5, p6, p7 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4, p5, p6, p7))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified T3, reified T4, reified T5, reified T6, reified T7, reified R> createJavaToJsBlockingProxyFunction7(context: CoroutineContext? = null): (T1, T2, T3, T4, T5, T6, T7) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<T5>(), typeOf<T6>(), typeOf<T7>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsBlockingProxyFunctionHelper<R>(types, context)
        return { p1, p2, p3, p4, p5, p6, p7 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4, p5, p6, p7))
        }
    }


    // Proxy JS to Java function (8)
    // ---

    @OptIn(ExperimentalStdlibApi::class)
    suspend inline fun <reified T1, reified T2, reified T3, reified T4, reified T5, reified T6, reified T7, reified T8, reified R> createJavaToJsProxyFunction8(waitForRegistration: Boolean): suspend (T1, T2, T3, T4, T5, T6, T7, T8) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<T5>(), typeOf<T6>(), typeOf<T7>(), typeOf<T8>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionHelper<R>(types, waitForRegistration)
        return { p1, p2, p3, p4, p5, p6, p7, p8 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4, p5, p6, p7, p8))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified T3, reified T4, reified T5, reified T6, reified T7, reified T8, reified R> createJavaToJsProxyFunction8(): suspend (T1, T2, T3, T4, T5, T6, T7, T8) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<T5>(), typeOf<T6>(), typeOf<T7>(), typeOf<T8>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionAsyncHelper<R>(types)
        return { p1, p2, p3, p4, p5, p6, p7, p8 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4, p5, p6, p7, p8))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified T3, reified T4, reified T5, reified T6, reified T7, reified T8, reified R> createJavaToJsBlockingProxyFunction8(context: CoroutineContext? = null): (T1, T2, T3, T4, T5, T6, T7, T8) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<T5>(), typeOf<T6>(), typeOf<T7>(), typeOf<T8>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsBlockingProxyFunctionHelper<R>(types, context)
        return { p1, p2, p3, p4, p5, p6, p7, p8 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4, p5, p6, p7, p8))
        }
    }


    // Proxy JS to Java function (9)
    // ---

    @OptIn(ExperimentalStdlibApi::class)
    suspend inline fun <reified T1, reified T2, reified T3, reified T4, reified T5, reified T6, reified T7, reified T8, reified T9, reified R> createJavaToJsProxyFunction9(waitForRegistration: Boolean): suspend (T1, T2, T3, T4, T5, T6, T7, T8, T9) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<T5>(), typeOf<T6>(), typeOf<T7>(), typeOf<T8>(), typeOf<T9>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionHelper<R>(types, waitForRegistration)
        return { p1, p2, p3, p4, p5, p6, p7, p8, p9 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4, p5, p6, p7, p8, p9))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified T3, reified T4, reified T5, reified T6, reified T7, reified T8, reified T9, reified R> createJavaToJsProxyFunction9(): suspend (T1, T2, T3, T4, T5, T6, T7, T8, T9) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<T5>(), typeOf<T6>(), typeOf<T7>(), typeOf<T8>(), typeOf<T9>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsProxyFunctionAsyncHelper<R>(types)
        return { p1, p2, p3, p4, p5, p6, p7, p8, p9 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4, p5, p6, p7, p8, p9))
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    inline fun <reified T1, reified T2, reified T3, reified T4, reified T5, reified T6, reified T7, reified T8, reified T9, reified R> createJavaToJsBlockingProxyFunction9(context: CoroutineContext? = null): (T1, T2, T3, T4, T5, T6, T7, T8, T9) -> R {
        val types = listOf(typeOf<T1>(), typeOf<T2>(), typeOf<T3>(), typeOf<T4>(), typeOf<T5>(), typeOf<T6>(), typeOf<T7>(), typeOf<T8>(), typeOf<T9>(), typeOf<R>())
        val functionWithParamArray = createJavaToJsBlockingProxyFunctionHelper<R>(types, context)
        return { p1, p2, p3, p4, p5, p6, p7, p8, p9 ->
            functionWithParamArray(arrayOf(p1, p2, p3, p4, p5, p6, p7, p8, p9))
        }
    }


    // Internal
    // ---

    @OptIn(ExperimentalStdlibApi::class)
    @PublishedApi
    internal suspend inline fun <reified R> createJavaToJsProxyFunctionHelper(types: List<KType>, waitForRegistration: Boolean): suspend (Array<Any?>) -> R {
        val awaitJsPromise = types.lastOrNull()?.classifier != Deferred::class
        val lambdaJsValue = jsBridge?.registerJsLambda(this, types, waitForRegistration)
                ?: throw JavaToJsFunctionRegistrationError("<lambda>", customMessage = "Cannot create a Java-to-JS function proxy because the JS interpreter has been destroyed")

        return { params ->
            val jsBridge = jsBridge
                    ?: throw JavaToJsFunctionRegistrationError("lambda", customMessage = "Cannot create a Java-to-JS function proxy because the JS interpreter has been destroyed")

            this.hold()

            @Suppress("UNCHECKED_CAST")
            jsBridge.callJsLambda(lambdaJsValue, params, awaitJsPromise) as R
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    @PublishedApi
    internal inline fun <reified R> createJavaToJsProxyFunctionAsyncHelper(types: List<KType>): suspend (Array<Any?>) -> R {
        val awaitJsPromise = types.lastOrNull()?.classifier != Deferred::class
        val lambdaJsValue = jsBridge?.registerJsLambdaAsync(this, types)
                ?: throw JavaToJsFunctionRegistrationError("<lambda>", customMessage = "Cannot create a Java-to-JS function proxy because the JS interpreter has been destroyed")

        return { params ->
            val jsBridge = jsBridge
                    ?: throw JavaToJsFunctionRegistrationError("<lambda>", customMessage = "Cannot create a Java-to-JS function proxy because the JS interpreter has been destroyed")

            this.hold()

            @Suppress("UNCHECKED_CAST")
            jsBridge.callJsLambda(lambdaJsValue, params, awaitJsPromise) as R
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    @PublishedApi
    internal inline fun <reified R> createJavaToJsBlockingProxyFunctionHelper(types: List<KType>, context: CoroutineContext?): (Array<Any?>) -> R {
        val awaitJsPromise = types.lastOrNull()?.classifier != Deferred::class
        val lambdaJsValue = jsBridge?.registerJsLambdaBlocking(this, types, context)
                ?: throw JavaToJsFunctionRegistrationError("<lambda>", customMessage = "Cannot create a Java-to-JS function proxy because the JS interpreter has been destroyed")

        return { params ->
            val jsBridge = jsBridge
                    ?: throw JavaToJsFunctionRegistrationError("<lambda>", customMessage = "Cannot create a Java-to-JS function proxy because the JS interpreter has been destroyed")

            this.hold()

            runBlocking {
                @Suppress("UNCHECKED_CAST")
                jsBridge.callJsLambda(lambdaJsValue, params, awaitJsPromise) as R
            }
        }
    }
}
