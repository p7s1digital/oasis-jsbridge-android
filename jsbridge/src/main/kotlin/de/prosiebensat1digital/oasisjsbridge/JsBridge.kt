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
package de.prosiebensat1digital.oasisjsbridge

import android.app.Activity
import android.content.Context
import android.os.Looper
import androidx.annotation.VisibleForTesting
import com.android.dx.stock.ProxyBuilder
import de.prosiebensat1digital.oasisjsbridge.JsBridgeError.*
import de.prosiebensat1digital.oasisjsbridge.extensions.*
import java.io.FileNotFoundException
import java.io.InputStream
import java.lang.reflect.Method as JavaMethod
import java.util.concurrent.CopyOnWriteArraySet
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.withLock
import kotlin.coroutines.*
import kotlin.reflect.*
import kotlin.reflect.full.createType
import kotlin.reflect.full.declaredMemberFunctions
import kotlinx.coroutines.*
import timber.log.Timber
import java.lang.reflect.Proxy


/**
 * Execute JS code via Duktape or QuickJS and handle the bi-directional communication between native
 * and JS code via interface registration.
 *
 * All the given types are transferred and converted between Native and JS via reflection.
 *
 * Most basic types are supported, as well as lambda parameters. More complex objects can be
 * transferred using the JsonObjectWrapper class.
 *
 * JS values which do not need to be converted to native values can be encapsulated into a
 * JsValue object.
 *
 * Note: all the public methods are asynchronous and will not block the caller threads. They
 * can be safely called in a "synchronous" way. though, because their executions are guaranteed
 * to be performed sequentially (via an internal queue).
 *
 * @param config JsBridge configuration
 * @param context Context needed for local storage extension
 */
class JsBridge
constructor(config: JsBridgeConfig, context: Context) : CoroutineScope {

    companion object {
        private var isLibraryLoaded = false
    }

    abstract class ErrorListener(val coroutineContext: CoroutineContext? = null) {
        abstract fun onError(error: JsBridgeError)
    }

    // State
    private enum class State(val intValue: Int) {
        Pending(0), AboutToStart(1), Starting(2), Started(3),
        Releasing(4), Released(5);
    }

    private val startReleaseLock = ReentrantLock()
    private var state = AtomicInteger(State.Pending.intValue)
    private val currentState get() = State.values().firstOrNull { it.intValue == state.get() }

    // JS coroutine dispatcher (single thread/sequential execution)
    private val jsDispatcher = Executors.newSingleThreadExecutor().asCoroutineDispatcher()
    private var jsThreadId: Long? = null  // for checking thread

    // Handle couroutines lifecycle via a Job instance (for structured concurrency)
    private val rootJob = SupervisorJob()
    private val coroutineExceptionHandler = CoroutineExceptionHandler(::handleCoroutineException)
    override val coroutineContext = rootJob + jsDispatcher + coroutineExceptionHandler

    private var jniJsContext: Long? = null
    var customClassLoader: ClassLoader? = null
        private set

    private val errorListeners = CopyOnWriteArraySet<ErrorListener>()

    // Extensions
    private var jsDebuggerExtension: JsDebuggerExtension? = null
    private var promiseExtension: PromiseExtension? = null
    private var setTimeoutExtension: SetTimeoutExtension? = null
    private var consoleExtension: ConsoleExtension? = null
    private var xhrExtension: XMLHttpRequestExtension? = null
    private var localStorageExtension: LocalStorageExtension? = null

    private var internalCounter = AtomicInteger(0)

    // Initialize the JS interpreter
    // - create the JS context via JNI
    // - set up the interpreter with polyfills and helpers (e.g. support for setTimeout)
    init {
        launch {
            jsThreadId = Thread.currentThread().id
        }

        startReleaseLock.withLock {
            Timber.d("Starting JsBridge")

            // Pending -> AboutToStart
            if (!state.compareAndSet(State.Pending.intValue, State.AboutToStart.intValue)) {
                throw StartError(
                    null,
                    "Cannot start the JsBridge because its current state is $currentState"
                )
                    .also(::notifyErrorListeners)
            }

            launch {
                // AboutToStart -> Starting
                // From now on, a release() call is allowed to interrupt the start
                if (!state.compareAndSet(State.AboutToStart.intValue, State.Starting.intValue)) {
                    return@launch  // Start interrupted by a Destroy
                }

                try {
                    val jniJsContext = createJniJsContext()

                    if (this@JsBridge.jniJsContext != null) {
                        throw InternalError("Cannot create a second JNI context!")
                    }
                    this@JsBridge.jniJsContext = jniJsContext
                } catch (t: Throwable) {
                    throw StartError(t)
                }

                if (state.compareAndSet(State.Starting.intValue, State.Started.intValue)) {
                    Timber.d("JsBridge successfully started!")
                }
            }

            // Extensions
            if (config.jsDebuggerConfig.enabled)
                jsDebuggerExtension = JsDebuggerExtension(this, config.jsDebuggerConfig)
            if (config.promiseConfig.enabled)
                promiseExtension = PromiseExtension(
                    context = context,
                    jsBridge = this@JsBridge,
                    config = config.promiseConfig
                )
            if (config.setTimeoutConfig.enabled)
                setTimeoutExtension = SetTimeoutExtension(this@JsBridge)
            if (config.consoleConfig.enabled)
                consoleExtension = ConsoleExtension(this@JsBridge, config.consoleConfig)
            if (config.xhrConfig.enabled)
                xhrExtension = XMLHttpRequestExtension(context, this@JsBridge, config.xhrConfig)
            if (config.localStorageConfig.enabled)
                localStorageExtension = LocalStorageExtension(
                    this@JsBridge,
                    config.localStorageConfig,
                    context.applicationContext
                )
            config.jvmConfig.customClassLoader?.let { customClassLoader = it }
        }
    }

    protected fun finalize() {
        if (state.get() == State.Releasing.intValue || state.get() == State.Released.intValue) {
            return
        }

        release()
    }


    // Public methods
    // ---

    /**
     * Destroy the JsBridge
     *
     * - Cancel (and join) all the active jobs
     * - Destroy the JS context via JNI
     * - Clean up resources
     */
    fun release(): Unit = startReleaseLock.withLock {
        if (state.get() == State.Releasing.intValue || state.get() == State.Released.intValue) {
            Timber.w("JsBridge is already in state $currentState and does not need to be released again!")
            return
        }

        if (!state.compareAndSet(State.AboutToStart.intValue, State.Releasing.intValue) &&
            !state.compareAndSet(State.Starting.intValue, State.Releasing.intValue) &&
            !state.compareAndSet(State.Started.intValue, State.Releasing.intValue)
        ) {
            throw DestroyError(
                null,
                "Cannot destroy the JsBridge because its current state is $currentState}"
            )
                .also(::notifyErrorListeners)
        }

        if (!rootJob.isActive) {
            throw DestroyError(
                null,
                "JsBridge won't be destroyed because the main coroutine Job is not active"
            )
                .also(::notifyErrorListeners)
        }

        Timber.d("Releasing JsBridge")

        rootJob.cancel()

        // As we are at the end of the lifecycle, we use here the GlobalScope
        GlobalScope.launch(jsDispatcher + coroutineExceptionHandler) {
            try {
                rootJob.join()
            } catch (c: CancellationException) {
            }

            try {
                jniJsContext?.let {
                    jniJsContext = null
                    jniDeleteContext(it)
                }
            } catch (t: Throwable) {
                val e = DestroyError(t)
                throw e
            }

            jsDebuggerExtension?.release()
            jsDebuggerExtension = null

            promiseExtension?.release()
            promiseExtension = null

            setTimeoutExtension?.release()
            setTimeoutExtension = null

            xhrExtension?.release()
            xhrExtension = null

            errorListeners.clear()
            jsDispatcher.close()

            // Releasing -> Released
            if (!state.compareAndSet(State.Releasing.intValue, State.Released.intValue)) {
                Timber.w("Unexpected state after releasing JsBridge: $currentState}")
            }
        }
    }

    fun registerErrorListener(listener: ErrorListener) {
        errorListeners.add(listener)
    }

    fun unregisterErrorListener(listener: ErrorListener) {
        errorListeners.remove(listener)
    }

    fun startDebugger(activity: Activity? = null) {
        val jsDebuggerExtension = jsDebuggerExtension ?: run {
            Timber.w("Cannot start JS debugger: Please enable it in JsBridgeConfig.jsDebuggerConfig.")
            return
        }

        jsDebuggerExtension.activity = activity

        launch {
            jniStartDebugger(jniJsContextOrThrow(), jsDebuggerExtension.config.port)
        }
    }

    /**
     * Defines how the JS file will be evaluated.
     *
     * Evaluation types:
     * - global (default): run the JS code in the global context
     * - module: run as an ES6 module (QuickJS only, Duktape not supported!)
     */
    enum class JsFileEvaluationType {
        Global,
        Module,
    }

    /**
     * Set a custom module loader which will return the content of the given module
     */
    private var jsModuleLoaderFunc: ((moduleName: String) -> String)? = null
    fun setJsModuleLoader(func: (moduleName: String) -> String) {
        jsModuleLoaderFunc = func

        launch {
            val jniJsContext = jniJsContextOrThrow()
            jniEnableModuleLoader(jniJsContext)
        }
    }

    /**
     * Evaluate a local JS file which should be bundled as an asset.
     *
     * If the given file has a corresponding .max file, this one will be used in debug mode.
     * e.g.: "myfile.js" / "myfile.max.js"
     */
    suspend fun evaluateLocalFile(
        context: Context,
        filename: String,
        useMaxJs: Boolean = false,
        type: JsFileEvaluationType = JsFileEvaluationType.Global
    ) {
        withContext(coroutineContext) {
            val jniJsContext = jniJsContextOrThrow()

            try {
                val (inputStream, jsFileName) = getInputStream(context, filename, useMaxJs)
                val jsString = inputStream.bufferedReader().use { it.readText() }
                jniEvaluateFileContent(
                    jniJsContext,
                    jsString,
                    jsFileName,
                    type == JsFileEvaluationType.Module
                )
                Timber.d("-> $filename ($jsFileName) has been successfully evaluated!")
            } catch (t: Throwable) {
                throw JsFileEvaluationError(filename, t)
            }

            processPromiseQueue()
        }
    }

    /**
     * Evaluate a local JS file which should be bundled as an asset (blocking version).
     */
    fun evaluateLocalFileUnsync(
        context: Context,
        filename: String,
        useMaxJs: Boolean = false,
        type: JsFileEvaluationType = JsFileEvaluationType.Global
    ) {
        launch {
            evaluateLocalFile(context, filename, useMaxJs, type)
        }
    }

    /**
     * Evaluate a local JS file which should be bundled as an asset (blocking version)
     */
    fun evaluateLocalFileBlocking(
        context: Context,
        filename: String,
        useMaxJs: Boolean = false,
        type: JsFileEvaluationType = JsFileEvaluationType.Global
    ) {
        runBlocking(coroutineContext) {
            evaluateLocalFile(context, filename, useMaxJs, type)
        }
    }

    /*
     * Evaluate the content of a JavaScript file (e.g. fetched from the network).
     */
    suspend fun evaluateFileContent(
        content: String,
        filename: String,
        type: JsFileEvaluationType = JsFileEvaluationType.Global
    ) {
        withContext(coroutineContext) {
            val jniJsContext = jniJsContextOrThrow()

            try {
                jniEvaluateFileContent(
                    jniJsContext,
                    content,
                    filename,
                    type == JsFileEvaluationType.Module
                )
                Timber.d("-> file content ($filename) has been successfully evaluated!")
            } catch (t: Throwable) {
                throw JsFileEvaluationError(filename, t)
            }

            processPromiseQueue()
        }
    }

    /**
     * Evaluate the content of a JavaScript file (e.g. fetched from the network).
     */
    fun evaluateFileContentUnsync(
        content: String,
        filename: String,
        type: JsFileEvaluationType = JsFileEvaluationType.Global
    ) {
        launch {
            evaluateFileContent(content, filename, type)
        }
    }

    /**
     * Evaluate the content of a JavaScript file (e.g. fetched from the network).
     */
    fun evaluateFileContentBlocking(
        content: String,
        filename: String,
        type: JsFileEvaluationType = JsFileEvaluationType.Global
    ) {
        runBlocking(coroutineContext) {
            evaluateFileContent(content, filename, type)
        }
    }

    /**
     * Evaluate the given JS code without return value.
     */
    fun evaluateUnsync(js: String) {
        launch {
            val jniJsContext = jniJsContextOrThrow()

            // Only for debug printing purposes:
            //val shortJs = if (js.length <= 500) js else js.take(500) + "..."
            //Timber.v("evaluateNoRetVal(\"$shortJs\")")

            try {
                jniEvaluateString(jniJsContext, js, null, false)
            } catch (t: Throwable) {
                throw t as? JsStringEvaluationError ?: JsStringEvaluationError(js, t)
            }

            processPromiseQueue()
        }
    }

    /**
     * Evaluate the given JS code and return the value as a Deferred
     */
    fun <T : Any?> evaluateAsync(js: String, type: KType?): Deferred<T> = async {
        evaluate(js, type, false)
    }

    inline fun <reified T : Any?> evaluateAsync(js: String): Deferred<T> {
        return evaluateAsync(js, typeOf<T>())
    }

    suspend inline fun <reified T : Any?> evaluate(js: String): T {
        return evaluate(js, typeOf<T>(), true)
    }

    inline fun <reified T : Any?> evaluateBlocking(
        js: String,
        context: CoroutineContext = EmptyCoroutineContext
    ): T {
        return runBlocking(context) {
            if (isMainThread()) {
                Timber.w("WARNING: evaluating JS code in the main thread! Consider using non-blocking API or evaluating JS code in another thread!")
            }
            evaluate(js, typeOf<T>(), true)
        }
    }

    // Notes:
    // - runs in the JS thread and block the caller thread until the result has been evaluated!
    // - from Kotlin, it is recommended to use the method with generic parameter, instead!
    fun evaluateBlocking(js: String, javaClass: Class<*>?): Any? {
        return runBlocking {
            if (isMainThread()) {
                Timber.w("WARNING: evaluating JS code in the main thread! Consider using non-blocking API or evaluating JS code in another thread!")
            }
            evaluate(js, javaClass?.kotlin?.createType(), false)
        }
    }


    // Internal
    // ---

    @PublishedApi
    internal suspend fun <T : Any?> evaluate(js: String, type: KType?, awaitJsPromise: Boolean): T {
        //val initialStackTrace = Thread.currentThread().stackTrace
        val parameter = type?.let { Parameter(type, customClassLoader) }

        val doAwaitJsPromise = awaitJsPromise && type?.classifier != Deferred::class

        val ret = withContext(coroutineContext) {
            val jniJsContext = jniJsContextOrThrow()

            // Only for debug printing purposes:
            //val shortJs = if (js.length <= 500) js else js.take(500) + "..."
            //Timber.v("evaluate(\"$shortJs\")")

            // Exceptions must be directly caught by the caller
            var ret = jniEvaluateString(jniJsContext, js, parameter, doAwaitJsPromise)

            if (doAwaitJsPromise && ret is Deferred<*>) {
                processPromiseQueue()
                ret = ret.await()
            }

            processPromiseQueue()
            ret
        }

        @Suppress("UNCHECKED_CAST")
        return ret as T
    }

    // Evaluate the given JS value and return the result as a deferred
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    suspend fun <T : Any?> evaluateJsValue(
        jsValue: JsValue,
        type: KType?,
        awaitJsPromise: Boolean
    ): T {
        jsValue.codeEvaluationDeferred?.await()
        return evaluate("$jsValue", type, awaitJsPromise)
    }

    // Evaluate the given JS value and return the result as a deferred
    @PublishedApi
    internal fun <T : Any?> evaluateJsValueAsync(jsValue: JsValue, type: KType?): Deferred<T> =
        async {
            evaluateJsValue(jsValue, type, true)
        }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    fun newJsFunctionAsync(
        jsValue: JsValue,
        functionArgs: Array<String>,
        jsCode: String
    ): Deferred<Unit> {
        if (isJsThread()) {
            val jniJsContext = jniJsContextOrThrow()
            jniNewJsFunction(jniJsContext, jsValue.associatedJsName, functionArgs, jsCode)
            return CompletableDeferred()
        }

        return async {
            val jniJsContext = jniJsContextOrThrow()
            jniNewJsFunction(jniJsContext, jsValue.associatedJsName, functionArgs, jsCode)
        }
    }

    // Register a "JS" interface called by native.
    //
    // All the methods of the given interface must be implemented in JS as:
    //
    // MyJsApi.myMethod(arg1, arg2, ...)
    //
    // If check is set to true, this method will throw an exception when the JS object is invalid or
    // does not implement all the methods of the given NativeToJsInterface
    //
    // Notes:
    // - objects and array must be defined wrapped as JsonObjectWrapper
    // - function parameters are supported (with the same limitations as in registerJsToNativeInterface)
    // - native methods returning a "Kotlin coroutine" Deferred are mapped to a JS Promise
    // - if the JS value is a promise, you need to resolve it first with jsValue.await() or jsValue.awaitAsync()
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    suspend fun <T : NativeToJsInterface> registerNativeToJsInterface(
        jsValue: JsValue,
        type: KClass<T>,
        check: Boolean,
        isAidl: Boolean
    ): T {
        return registerNativeToJsInterfaceHelper(jsValue, type, check, true, isAidl)
    }

    // Register a "JS" interface called by native (blocking)
    //
    // When check = false, the proxy object will be directly returned
    // When check = true, the method will block the current thread until the registration has been
    // done and then return the proxy object
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    fun <T : NativeToJsInterface> registerNativeToJsInterfaceBlocking(
        jsValue: JsValue,
        type: KClass<T>,
        check: Boolean,
        context: CoroutineContext?,
        isAidl: Boolean
    ): T {

        return runBlocking(context ?: Dispatchers.Unconfined) {
            registerNativeToJsInterfaceHelper(
                jsValue,
                type,
                check,
                waitForRegistration = check,
                isAidl
            )
        }
    }

    // Register a "native" interface called by JS.
    //
    // All the native methods of the given interface can be called in JS as follows:
    //
    // MyNativeApi.myMethod(arg1, arg2, ...)
    //
    // Notes:
    // - objects and array must be wrapped in JsonObjectWrapper
    // - function parameters are supported but with only up to 2 parameters and without return value
    //
    // Note: please be aware that the object instance is strongly referenced by the JS
    // variable with the given name!
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    fun registerJsToNativeInterface(kClass: KClass<*>, obj: Any): JsValue {
        val suffix = internalCounter.incrementAndGet()
        val jsObjectName = "__jsBridge_${kClass.simpleName ?: "unnamed_class"}$suffix"
        val jsValue = JsValue(this, null, associatedJsName = jsObjectName)

        launch {
            val jniJsContext = jniJsContextOrThrow()
            registerJsToNativeInterfaceHelper(jniJsContext, jsValue, kClass, obj, false)
        }

        return jsValue
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    fun registerNativeAidlInterface(kClass: KClass<*>, obj: Any): JsValue {
        val suffix = internalCounter.incrementAndGet()
        val jsObjectName = "__jsBridge_aidl_${kClass.simpleName ?: "unnamed_class"}$suffix"
        val jsValue = JsValue(this, null, associatedJsName = jsObjectName)

        launch {
            val jniJsContext = jniJsContextOrThrow()
            registerJsToNativeInterfaceHelper(jniJsContext, jsValue, kClass, obj, true)
        }

        return jsValue
    }

    // Register a JS lambda from a JsValue.
    //
    // Return a new JsValue which can be used for calling the lambda via callJsLambda()
    @PublishedApi
    internal suspend fun registerJsLambda(
        jsValue: JsValue,
        types: List<KType>,
        waitForRegistration: Boolean
    ): JsValue {
        return registerJsLambdaHelper(jsValue, types, waitForRegistration)
    }

    // Register a JS lambda from a JsValue (unchecked).
    //
    // Return a new JsValue which can be used for calling the lambda via callJsLambda()
    // Register a "JS" interface called by native (blocking)
    @PublishedApi
    internal fun registerJsLambdaAsync(jsValue: JsValue, types: List<KType>): JsValue {
        return runBlocking(Dispatchers.Unconfined) {
            registerJsLambdaHelper(jsValue, types, false)
        }
    }

    // Register a JS lambda from a JsValue (blocking).
    //
    // Return a new JsValue which can be used for calling the lambda via callJsLambda()
    // Register a "JS" interface called by native (blocking)
    //
    // When check = false, the proxy function will be directly returned
    // When check = true, the method will block the current thread until the registration has been
    // done and then return the proxy function
    @PublishedApi
    internal fun registerJsLambdaBlocking(
        jsValue: JsValue,
        types: List<KType>,
        context: CoroutineContext?
    ): JsValue {
        return runBlocking(context ?: Dispatchers.Unconfined) {
            registerJsLambdaHelper(jsValue, types, false)
        }
    }

    @PublishedApi
    internal suspend fun registerJsLambdaHelper(
        jsValue: JsValue,
        types: List<KType>,
        waitForRegistration: Boolean
    ): JsValue {
        val parameters = types.map { Parameter(it, customClassLoader) }
        val inputParameters = parameters.take(types.count() - 1)
        val outputParameter = parameters.last()
        val method = Method(inputParameters.toTypedArray(), outputParameter, false)

        // Create a separate JS value just for the call
        val suffix = internalCounter.incrementAndGet()
        val lambdaJsValue = JsValue(this, null, associatedJsName = "__jsBridge_jsLambda$suffix")

        val registerBlock = suspend {
            val jniJsContext = jniJsContextOrThrow()

            jsValue.codeEvaluationDeferred?.await()
            jniCopyJsValue(jniJsContext, lambdaJsValue.associatedJsName, jsValue.associatedJsName)
            jniRegisterJsLambda(jniJsContext, lambdaJsValue.associatedJsName, method)
            Timber.v("Registered JS lambda ${lambdaJsValue.associatedJsName}")
            lambdaJsValue.hold()
        }

        if (waitForRegistration) {
            // Synchronous registration
            withContext(coroutineContext) {
                registerBlock()
            }
        } else {
            // Asynchronous registration
            launch {
                try {
                    registerBlock()
                } catch (t: Throwable) {
                    throw t as? JsException
                        ?: NativeToJsFunctionRegistrationError(jsValue.associatedJsName, t)
                }
            }
        }

        return lambdaJsValue
    }

    // Call a JS lambda registered via registerJsLambda()
    @PublishedApi
    internal suspend fun callJsLambda(
        lambdaJsValue: JsValue,
        args: Array<Any?>,
        awaitJsPromise: Boolean
    ): Any? {
        return withContext(coroutineContext) {
            val jniJsContext = jniJsContextOrThrow()

            lambdaJsValue.codeEvaluationDeferred?.await()

            // Exceptions must be directly caught by the caller
            var ret =
                jniCallJsLambda(jniJsContext, lambdaJsValue.associatedJsName, args, awaitJsPromise)

            if (awaitJsPromise && ret is Deferred<*>) {
                processPromiseQueue()
                ret = ret.await()
            }

            processPromiseQueue()
            ret
        }
    }

    // Directly call a registered JS lambda. It only works when being called from the JS thread!
    internal fun callJsLambdaUnsafe(
        lambdaJsValue: JsValue,
        args: Array<Any?>,
        awaitJsPromise: Boolean
    ): Any? {
        checkJsThread()

        val jniJsContext = jniJsContextOrThrow()

        return jniCallJsLambda(jniJsContext, lambdaJsValue.associatedJsName, args, awaitJsPromise)
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    fun registerJsToNativeFunction(func: Function<*>, types: List<KType>): JsValue {
        val suffix = internalCounter.incrementAndGet()
        val jsFunctionName = "__jsBridge_nativeFunction$suffix"
        val jsFunctionValue = JsValue(this, null, associatedJsName = jsFunctionName)

        launch {
            try {
                val jniJsContext = jniJsContextOrThrow()

                val invokeJavaMethod = func::class.java.methods.firstOrNull { it.name == "invoke" }
                    ?: throw JsToNativeRegistrationError(
                        func::class,
                        null,
                        "Cannot map Java function to JS: the object does not contain any 'invoke' method!"
                    )

                val invokeMethod = Method(invokeJavaMethod, types.mapIndexed { typeIndex, type ->
                    val variance =
                        if (typeIndex == types.count() - 1) KVariance.OUT else KVariance.IN
                    KTypeProjection(variance, type)
                }, true, customClassLoader)

                jniRegisterJavaLambda(
                    jniJsContext,
                    jsFunctionValue.associatedJsName,
                    func,
                    invokeMethod
                )
                jsFunctionValue.hold()
            } catch (e: JsToNativeRegistrationError) {
                throw e
            } catch (t: Throwable) {
                Timber.e(t)
                throw JsToNativeRegistrationError(func::class, t)
            }
        }

        return jsFunctionValue
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    fun assignJsValueAsync(jsValue: JsValue, jsCode: String): Deferred<Unit> {
        return async {
            val jniJsContext = jniJsContextOrThrow()
            jniAssignJsValue(jniJsContext, jsValue.associatedJsName, jsCode)
        }
    }

    internal fun deleteJsValue(jsValue: JsValue) {
        val globalName = jsValue.associatedJsName

        launch {
            jniJsContext?.let { jniDeleteJsValue(it, globalName) }
        }
    }

    internal fun copyJsValue(globalNameTo: String, jsValueFrom: JsValue) {
        launch {
            jsValueFrom.codeEvaluationDeferred?.await()
            jniJsContext?.let { jniCopyJsValue(it, globalNameTo, jsValueFrom.associatedJsName) }
        }
    }

    @PublishedApi
    internal fun convertJavaValueToJs(value: Any?, parameter: Parameter): JsValue {
        val jsValue = JsValue(this)

        jsValue.codeEvaluationDeferred = async {
            val jniJsContext = jniJsContextOrThrow()
            jniConvertJavaValueToJs(jniJsContext, jsValue.associatedJsName, value, parameter)
        }

        return jsValue
    }

    // Simulate a "Promise" tick. Needs to be manually triggered as we don't use an event loop.
    internal fun processPromiseQueue() {
        val promiseExtension = promiseExtension ?: return

        checkJsThread()

        if (promiseExtension.config.needsPolyfill) {
            // Manually process promise queue of the polyfill
            promiseExtension.processPolyfillQueue()
        } else {
            // Run pending jobs of the JS engine with built-in promise support
            jniJsContext?.let { jniProcessPromiseQueue(it) }
        }
    }

    // Called by JsDebuggerExtension
    internal fun cancelDebug() {
        launch {
            val jniJsContext = jniJsContextOrThrow()
            jniCancelDebug(jniJsContext)
        }
    }

    // Notify all registered error listeners
    internal fun notifyErrorListeners(t: Throwable) {
        val e = t as? JsBridgeError ?: InternalError(t)
        //Timber.e("JsBridgeError: $e")

        errorListeners.forEach { errorListener ->
            launch(errorListener.coroutineContext ?: EmptyCoroutineContext) {
                errorListener.onError(e)
            }
        }
    }


    // Private
    // ---

    // Create the JNI/JS context and return a Deferred which is rejected with a
    // JsBridgeError in case of error
    private fun createJniJsContext(): Long {
        checkJsThread()

        if (!isLibraryLoaded) {
            try {
                Timber.d("Loading ${BuildConfig.JNI_LIB_NAME}...")
                System.loadLibrary(BuildConfig.JNI_LIB_NAME)
                Timber.d("${BuildConfig.JNI_LIB_NAME} successfully loaded!")
            } catch (t: Throwable) {
                val e = InternalError(
                    Throwable("Cannot load ${BuildConfig.JNI_LIB_NAME} JNI library!", t)
                )
                throw e
            }
            isLibraryLoaded = true
        }

        return jniCreateContext()
    }

    @Suppress("UNUSED")  // Called from JNI
    private fun callJsModuleLoader(moduleName: String): String {
        return jsModuleLoaderFunc!!(moduleName)
    }

    @Throws
    private fun getInputStream(
        context: Context,
        filename: String,
        useMaxJs: Boolean
    ): Pair<InputStream, String> {
        if (filename.contains("""\.max\.js$""".toRegex())) {
            throw Throwable(".max.js file should not be directly set, use .js and set useMaxJs parameter to true instead!")
        }

        // When debugging, try with a .max.js file first
        if (useMaxJs) {
            try {
                val maxFilename = filename.replace("""\.js$""".toRegex(), ".max.js")
                Timber.v("Checking availability of $maxFilename...")
                val ret = context.assets.open(maxFilename)
                Timber.d("$maxFilename found and will be used instead of $filename")
                return Pair(ret, maxFilename.substringAfterLast("/"))
            } catch (e: FileNotFoundException) {
                // Ignore error
            }
        }

        Timber.d("Reading $filename...")
        return Pair(context.assets.open(filename), filename.substringAfterLast("/"))
    }

    @Throws(JsToNativeRegistrationError::class)
    private fun registerJsToNativeInterfaceHelper(
        jniJsContext: Long,
        jsValue: JsValue,
        type: KClass<*>,
        obj: Any,
        isAidl: Boolean
    ) {
        checkJsThread()

        // Pick up the most "bottom" interface which implements JsToNativeInterface (or android.os.IInterface)
        val apiInterface = findApiInterface(obj::class.java, isAidl)
            ?: throw JsToNativeRegistrationError(
                this::class,
                customMessage = "Cannot map Java object to JS because it does not implement JsToJavaInterface or android.os.IInterface"
            )

        if (!type.isInstance(obj)) {
            throw JsToNativeRegistrationError(
                type,
                Throwable("${obj.javaClass.name} is not an instance of $type")
            )
        }

        val methods = linkedMapOf<String, Method>()

        try {
            // Collect class Methods using Kotlin reflection
            for (kFunction in apiInterface.kotlin.declaredMemberFunctions) {
                // TODO: check that there is no optional!
                val method = Method(kFunction, false, isAidl, customClassLoader)
                if (methods.put(kFunction.name, method) != null) {
                    throw JsToNativeRegistrationError(
                        type,
                        customMessage = ("${kFunction.name} is overloaded in $type")
                    )
                }
            }
        } catch (e: JsToNativeRegistrationError) {
            // Re-throw
            throw e
        } catch (t: Throwable) {
            // Fallback to Java reflection. This is unfortunately still needed (latest check: 1.3.40)
            // because Kotlin throws an exception when reflecting lambdas (function objects)
            Timber.w("Cannot reflect object of type $type (exception: $t) using Kotlin reflection! Falling back to Java reflection...")

            for (javaMethod in type.java.methods) {
                val method = Method(javaMethod, isAidl, customClassLoader)
                if (methods.put(javaMethod.name, method) != null) {
                    throw JsToNativeRegistrationError(
                        type,
                        Throwable("${method.name} is overloaded in $type")
                    )
                }
            }
        }

        try {
            jniRegisterJavaObject(
                jniJsContext,
                jsValue.associatedJsName,
                obj,
                methods.values.toTypedArray()
            )
        } catch (t: Throwable) {
            throw JsToNativeRegistrationError(type, t)
        }
    }

    private fun findApiInterface(clazz: Class<*>, isAidl: Boolean): Class<*>? {
        if (isAidl) {
            // If it implements android.os.IInterface, return it
            clazz.interfaces.singleOrNull { it == android.os.IInterface::class.java }
                ?.let { return clazz }

            // Otherwise, try with the superclass
            clazz.superclass?.let { findApiInterface(it, true) }?.let { return it }

            // Or with the implemented interfaces
            clazz.interfaces.firstNotNullOfOrNull { findApiInterface(it, true) }?.let { return it }

            // Not found
            return null
        } else {
            // If one of the interfaces implements android.os.IInterface (e.g. Stub), return it
            clazz.interfaces.firstOrNull { it == JsToNativeInterface::class.java }
                ?.let { return clazz }

            // Otherwise, try with the interfaces
            clazz.interfaces.firstOrNull { findApiInterface(it, false) != null }?.let { return it }
        }

        return null
    }

    // - If async = true, the proxy object is returned directly (the registration will be
    // done asynchronously)
    // - If async = false, the proxy object is only returned after a successful registration
    @Throws
    private suspend fun <T : Any> registerNativeToJsInterfaceHelper(
        jsValue: JsValue,
        type: KClass<T>,
        check: Boolean,
        waitForRegistration: Boolean,
        isAidl: Boolean
    ): T {
        if (!type.java.isInterface) {
            throw NativeToJsRegistrationError(type, customMessage = "$type must be an interface")
        }

        // Only allow interface directly implementing JsToNativeInterface (or implementing an interface implementing
        // JsToNativeInterface)
        // => Sequence<KClass<*>>
        val interfaces = generateSequence(type as KClass<*>) { previousClassifier ->
            // Single parent interface
            previousClassifier
                // Type -> Sequence<KType>
                .supertypes.asSequence()

                // -> KClass<*>
                .map { it.classifier as? KClass<*> }

                // Ignore "Any" super type
                .filter { it != Any::class }

                // Except only 1 super interface and return it
                // -> KClass<*>
                .singleOrNull()
                .also {
                    it?.java?.isInterface
                        ?: throw NativeToJsRegistrationError(
                            type,
                            customMessage = "$type and its super interfaces must extend exactly 1 interface"
                        )
                }

                // Continue until it is the NativeToJsInterface
                .takeIf { it != NativeToJsInterface::class }
        }

        val methods = interfaces
            // Collect all member functions of all interfaces
            // -> Sequence<KFunction<*>>
            .flatMap { it.declaredMemberFunctions.asSequence() }

            // Group by name
            // -> Map<methodName, List<Method>>
            .groupBy({ it.name }, { Method(it, false, isAidl, customClassLoader) })

            // Map to unique value
            // -> Map<methodName, Method>
            .mapValues { entry ->
                entry.value.singleOrNull()
                    ?: throw NativeToJsRegistrationError(
                        type,
                        Throwable("${entry.key} is overloaded in $type")
                    )
            }

            // => Sequence<Method>
            .values

        if (waitForRegistration) {
            // Synchronous registration
            withContext(coroutineContext) {
                jsValue.codeEvaluationDeferred?.await()
                jniRegisterJsObject(
                    jniJsContextOrThrow(),
                    jsValue.associatedJsName,
                    methods.toTypedArray(),
                    check
                )
            }
        } else {
            // Asynchronous registration
            launch(this@JsBridge.coroutineContext) {
                try {
                    jsValue.codeEvaluationDeferred?.await()
                    jniRegisterJsObject(
                        jniJsContextOrThrow(),
                        jsValue.associatedJsName,
                        methods.toTypedArray(),
                        check
                    )
                } catch (t: Throwable) {
                    throw NativeToJsRegistrationError(type, cause = t)
                }
            }
        }

        // Create proxy listener for the JS object
        val proxyListener = ProxyListener(jsValue, type.java)

        @Suppress("UNCHECKED_CAST")
        val proxy = Proxy.newProxyInstance(
            customClassLoader ?: type.java.classLoader,
            arrayOf(type.java),
            proxyListener
        ) as T
        Timber.v("Created proxy instance for ${type.java.name}, js value name: $jsValue")

        return proxy
    }

    // Call a JS method registered via registerNativeToJsInterface()
    @Throws
    private fun callJsMethod(
        name: String,
        method: JavaMethod,
        args: Array<Any?>,
        awaitJsPromise: Boolean
    ): Any? {
        checkJsThread()

        val jniJsContext = jniJsContextOrThrow()
        val retVal = jniCallJsMethod(jniJsContext, name, method, args, awaitJsPromise)

        processPromiseQueue()
        return retVal
    }

    @Suppress("UNUSED")  // Called from JNI
    private fun onDebuggerPending() {
        checkJsThread()

        jsDebuggerExtension?.onDebuggerPending()
    }

    @Suppress("UNUSED")  // Called from JNI
    private fun onDebuggerReady() {
        checkJsThread()

        jsDebuggerExtension?.onDebuggerReady()
    }

    // Create a native proxy to a JS function defined by a reflected (lambda) Method. The function
    // will be called from Java/Kotlin and will trigger execution of the JS lambda.
    //
    // This is used for example when a JsToNative interface with a lambda parameter is registered,
    // e.g.:
    // interface MyNativeApi {
    //   fun myNativeMethod(cb: (p: Int) -> Unit)
    // }
    //
    // When JS code calls "myNativeMethod()", the cb parameter will be passed to the native after being
    // created via createJsLambdaProxy().
    //
    // Note: as it triggers a JS function running in the JS thread, the lambda needs to be started
    // asynchronously and shall not block the current thread. As a result, only JS lambdas without
    // return value are supported (which is usually fine for callbacks).
    @Suppress("UNUSED")  // Called from JNI
    private fun createJsLambdaProxy(globalObjectName: String, method: Method): Function<Any?> {
        checkJsThread()

        val returnClass = method.returnParameter.getJava()
        if (returnClass != Unit::class.java) {
            throw JsToNativeFunctionRegistrationError(
                "JS lambda ($globalObjectName)",
                customMessage = "Unsupported return ($returnClass). JS lambdas should not return any value!"
            )
        }

        // Wrap the JS object within a JsValue which will be deleted when no longer needed
        // Note: make sure that the function block below "retain" the jsFunctionObject by using it otherwise
        // it will be garbage-collected and the JS function object will be gone before being called!
        val jsFunctionObject = JsValue(this, null, associatedJsName = globalObjectName)

        return method.asFunctionWithArgArray { args ->
            runInJsThread {
                try {
                    val jniJsContext = jniJsContextOrThrow()
                    jniCallJsLambda(jniJsContext, jsFunctionObject.associatedJsName, args, false)
                } catch (t: Throwable) {
                    throw JsToNativeFunctionCallError("JS lambda($globalObjectName)", t)
                }
            }
        }
    }

    // Create a native proxy to a JS function defined by a reflected (lambda) Method. The function
    // will be called from Java/Kotlin and will trigger execution of the JS lambda.
    //
    // This is used for example when a JsToNative interface with a lambda parameter is registered,
    // e.g.:
    // interface MyNativeApi {
    //   fun myNativeMethod(cb: (p: Int) -> Unit)
    // }
    //
    // When JS code calls "myNativeMethod()", the cb parameter will be passed to the native after being
    // created via createJsLambdaProxy().
    //
    // Note: as it triggers a JS function running in the JS thread, the lambda needs to be started
    // asynchronously and shall not block the current thread. As a result, only JS lambdas without
    // return value are supported (which is usually fine for callbacks).
    @Suppress("UNUSED")  // Called from JNI
    private fun createAidlInterfaceProxy(globalObjectName: String, parameter: Parameter): Any? {
        checkJsThread()

        // Take over global JS variable with name "globalObjectName" as JsValue
        val jsValue = JsValue(this, null, globalObjectName)

        // Create proxy listener for the JS object
        val proxyListener = ProxyListener(jsValue, parameter.javaClass as Class<*>)

        // We use the ProxyBuilder from dexmaker instead of the standard Java Proxy because
        // it makes it possible to make the proxy extend another class (AIDL Stub)
        val proxyBuilder = ProxyBuilder
            .forClass(parameter.aidlInterfaceStub!!.javaClass)
            .handler(proxyListener)
        customClassLoader?.let { proxyBuilder.parentClassLoader(it) }
        val proxy = proxyBuilder.build()
        Timber.v("Created proxy instance for ${parameter.javaClass.name}, js value name: $jsValue")

        return proxy
    }

    @Suppress("UNUSED")  // Called from JNI
    private fun createCompletableDeferred(): CompletableDeferred<Any?> {
        checkJsThread()
        return CompletableDeferred()
    }

    @Suppress("UNUSED")  // Called from JNI
    private fun resolveDeferred(deferred: CompletableDeferred<Any?>, value: Any?) {
        checkJsThread()
        deferred.complete(value)
    }

    @Suppress("UNUSED")  // Called from JNI
    private fun rejectDeferred(deferred: CompletableDeferred<Any?>, error: JsException) {
        checkJsThread()
        deferred.completeExceptionally(error)
    }

    @Suppress("UNUSED")  // Called from JNI
    private fun setUpJsPromise(id: String, deferred: Deferred<Any>) {
        launch {
            val jniJsContext = jniJsContextOrThrow()

            var isFulfilled = false

            val promiseValue = try {
                val result = deferred.await()
                isFulfilled = true
                result
            } catch (t: Throwable) {
                // Reject promise
                t
            }

            jniCompleteJsPromise(jniJsContext, id, isFulfilled, promiseValue)
            processPromiseQueue()
        }
    }

    internal fun runInJsThread(cb: () -> Unit) {
        if (isJsThread()) {
            cb()
            return
        }

        launch {
            cb()
        }
    }

    // Also called from JNI
    private fun checkJsThread() {
        if (!BuildConfig.DEBUG) return

        if (!isJsThread()) {
            Timber.e("checkJsThread() - FAILED!")
            throw InternalError(Throwable("Unexpected call: should be in the JsThread but is in ${Thread.currentThread().name}"))
        }
    }

    @PublishedApi
    internal fun isMainThread(): Boolean = (Looper.myLooper() == Looper.getMainLooper())
    private fun isJsThread(): Boolean = (Thread.currentThread().id == jsThreadId)

    private fun jniJsContextOrThrow() =
        jniJsContext ?: throw InternalError("Missing JNI JS context!")


    // JNI functions
    private external fun jniCreateContext(): Long
    private external fun jniStartDebugger(context: Long, port: Int)
    private external fun jniCancelDebug(context: Long)
    private external fun jniDeleteContext(context: Long)
    private external fun jniEnableModuleLoader(context: Long)
    private external fun jniEvaluateString(
        context: Long,
        js: String,
        type: Parameter?,
        awaitJsPromise: Boolean
    ): Any?

    private external fun jniEvaluateFileContent(
        context: Long,
        js: String,
        filename: String,
        asModule: Boolean
    )

    private external fun jniRegisterJavaLambda(context: Long, name: String, obj: Any, method: Any)
    private external fun jniRegisterJavaObject(
        context: Long,
        name: String,
        obj: Any,
        methods: Array<Any>
    )

    private external fun jniRegisterJsObject(
        context: Long,
        name: String,
        methods: Array<out Any>,
        check: Boolean
    )

    private external fun jniRegisterJsLambda(context: Long, name: String, method: Any)
    private external fun jniCallJsMethod(
        context: Long,
        objectName: String,
        javaMethod: JavaMethod,
        args: Array<Any?>
    ): Any?

    private external fun jniCallJsMethod(
        context: Long,
        objectName: String,
        javaMethod: JavaMethod,
        args: Array<Any?>,
        awaitJsPromise: Boolean
    ): Any?

    private external fun jniCallJsLambda(
        context: Long,
        objectName: String,
        args: Array<Any?>,
        awaitJsPromise: Boolean
    ): Any?

    private external fun jniAssignJsValue(context: Long, globalName: String, jsCode: String)
    private external fun jniDeleteJsValue(context: Long, globalName: String)
    private external fun jniCopyJsValue(context: Long, globalNameTo: String, globalNameFrom: String)
    private external fun jniNewJsFunction(
        context: Long,
        globalName: String,
        functionArgs: Array<String>,
        jsCode: String
    )

    private external fun jniConvertJavaValueToJs(
        context: Long,
        globalName: String,
        value: Any?,
        parameter: Parameter
    )

    private external fun jniCompleteJsPromise(
        context: Long,
        id: String,
        isFulfilled: Boolean,
        value: Any
    )

    private external fun jniProcessPromiseQueue(context: Long)

    @Suppress("UNUSED_PARAMETER")
    private fun handleCoroutineException(context: CoroutineContext, t: Throwable) {
        notifyErrorListeners(t)

        // We do not want to cancel the children here as this behaviour must be triggered from
        // outside (via the error listeners)
        //rootJob.cancelChildren()
    }

    private inner class ProxyListener(
        private val jsValue: JsValue,
        private val type: Class<*>,
    ) : java.lang.reflect.InvocationHandler {
        override fun invoke(proxy: Any, method: JavaMethod, args_: Array<Any?>?): Any? {
            val args = args_ ?: arrayOf()
            return when {
                method.name == "hashCode" -> jsValue.hashCode()
                method.name == "equals" -> jsValue.toString() == args.firstOrNull()?.toString()
                method.name == "toString" -> jsValue.toString()

                // AIDL-specific
                method.name == "asBinder" -> ProxyBuilder.callSuper(proxy, method, *args)
                method.name == "onTransact" -> ProxyBuilder.callSuper(proxy, method, *args)

                // Suspending method
                args.lastOrNull() is Continuation<*> -> {
                    @Suppress("UNCHECKED_CAST")
                    val continuation = args.last() as Continuation<Any?>
                    val jsArgs = args.copyOfRange(0, args.size - 1)
                    callJsMethodSuspended(method, jsArgs, continuation)
                }

                // Without return value
                method.returnType == Unit::class.java ||
                        method.returnType == Void::class.java ||
                        method.returnType == Void::class.javaPrimitiveType -> {
                    callJsMethodWithoutRetVal(method, args)
                    CompletableDeferred(Unit)
                }

                // Deferred
                method.returnType.isAssignableFrom(Deferred::class.java) -> callJsMethodAsync(
                    method,
                    args
                )

                else -> callJsMethodBlocking(method, args)
            }
        }

        private fun callJsMethodWithoutRetVal(method: JavaMethod, args: Array<Any?>?) {
            runInJsThread {
                try {
                    Timber.v("Calling (void) JS method ${type.name}::${method.name}()...")
                    callJsMethod(jsValue.associatedJsName, method, args ?: arrayOf(), false)
                } catch (t: Throwable) {
                    throw NativeToJsCallError("${type.name}::${method.name}()", t)
                }
            }
        }

        private fun callJsMethodSuspended(
            method: JavaMethod,
            args: Array<Any?>,
            continuation: Continuation<Any?>
        ): Any {
            launch {
                val retVal = try {
                    Timber.v("Calling (suspend) JS method ${type.name}::${method.name}()...")
                    callJsMethod(jsValue.associatedJsName, method, args, true)
                } catch (t: Throwable) {
                    // Throw JS exception (which must be directly caught by the caller)
                    continuation.resumeWithException(t)
                    return@launch
                }

                if (retVal is Deferred<*>) {
                    try {
                        continuation.resume(retVal.await())
                    } catch (t: Throwable) {
                        // For deferred, don't apply the generic JsBridge exception handling
                        // but directly add the exception into the returned Deferred
                        continuation.resumeWithException(t)
                    }
                } else {
                    continuation.resume(retVal)
                }
            }

            return kotlin.coroutines.intrinsics.COROUTINE_SUSPENDED
        }

        private fun callJsMethodAsync(method: JavaMethod, args: Array<Any?>?): Deferred<Any?> {
            val deferred = CompletableDeferred<Any?>()

            launch {
                val retVal = try {
                    Timber.v("Calling (deferred) JS method ${type.name}::${method.name}()...")
                    callJsMethod(jsValue.associatedJsName, method, args ?: arrayOf(), false)
                } catch (t: Throwable) {
                    // Reject the deferred with the JS exception (which must be directly caught by the caller)
                    deferred.completeExceptionally(t)
                }

                if (retVal is Deferred<*>) {
                    try {
                        deferred.complete(retVal.await())
                    } catch (t: Throwable) {
                        // For deferred, don't apply the generic JsBridge exception handling
                        // but directly add the exception into the returned Deferred
                        deferred.completeExceptionally(t)
                    }
                } else {
                    deferred.complete(retVal)
                }
            }

            return deferred
        }

        private fun callJsMethodBlocking(method: JavaMethod, args: Array<Any?>?): Any? {
            if (isMainThread()) {
                Timber.w("WARNING: executing JS method ${type.name}::${method.name}() in the main thread! Consider using a Deferred or calling the method in another thread!")
            } else {
                Timber.v("Calling (blocking) JS method ${type.name}::${method.name}()...")
            }

            return runBlocking(coroutineContext) {
                // Exceptions must be directly caught by the caller
                callJsMethod(jsValue.associatedJsName, method, args ?: arrayOf(), false)
            }
        }
    }
}
