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

import androidx.test.platform.app.InstrumentationRegistry
import de.prosiebensat1digital.oasisjsbridge.JsBridgeError.*
import io.mockk.*
import kotlin.test.*
import kotlinx.coroutines.*
import okhttp3.*
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.fail
import org.junit.After
import org.junit.BeforeClass
import org.junit.Ignore
import org.junit.Test
import timber.log.Timber
import java.lang.IllegalArgumentException
import java.lang.NullPointerException

interface TestNativeApiInterface : JsToNativeInterface {
    fun nativeMethodReturningTrue(): Boolean
    fun nativeMethodReturningFalse(): Boolean
    fun nativeMethodReturningString(): String
    fun nativeMethodReturningJsonObject(): JsonObjectWrapper
    fun nativeMethodReturningResolvedDeferred(): Deferred<String>
    fun nativeMethodReturningRejectedDeferred(): Deferred<String>
    fun nativeMethodWithBool(b: Boolean?)
    fun nativeMethodWithString(msg: String?)
    fun nativeMethodWithJsonObjects(jsonObject1: JsonObjectWrapper?, jsonObject2: JsonObjectWrapper?)
    fun nativeMethodWithCallback(cb: (string: String, obj: JsonObjectWrapper, bool1: Boolean, bool2: Boolean, int: Int, double: Double, optionalInt1: Int?, optionalInt2: Int?) -> Unit)
    fun nativeMethodThrowingException()
}

interface TestJsApiInterface : NativeToJsInterface {
    fun jsMethodWithString(msg: String)
    fun jsMethodWithJsonObjects(jsonObject1: JsonObjectWrapper, jsonObject2: JsonObjectWrapper)
    fun jsMethodWithJsValue(jsValue: JsValue)
    fun jsMethodWithCallback(cb: (msg: String, jsonObject1: JsonObjectWrapper, jsonObject2: JsonObjectWrapper, bool1: Boolean, bool2: Boolean, int: Int, double: Double, optionalInt1: Int?, optionalInt2: Int?) -> String)
    fun jsMethodWithUnitCallback(cb: () -> Unit)
    fun jsMethodReturningFulfilledPromise(str: String): Deferred<String>
    fun jsMethodReturningRejectedPromise(d: Double): Deferred<Double>
    fun jsMethodReturningJsValue(str: String): Deferred<JsValue>
}

interface JsExpectationsNativeApi : JsToNativeInterface {
    fun addExpectation(name: String, value: JsValue)
}

class JsBridgeTest {
    private var jsBridge: JsBridge? = null
    private val httpInterceptor = TestHttpInterceptor()
    private val okHttpClient = OkHttpClient.Builder().addInterceptor(httpInterceptor).build()
    private val jsToNativeFunctionMock = mockk<(p: Any) -> Unit>(relaxed = true)
    private val errorListener = object: JsBridge.ErrorListener(Dispatchers.Main) {
        override fun onError(error: JsBridgeError) {
            handleJsBridgeError(error)
        }
    }
    private var errors = mutableListOf<JsBridgeError>()
    private var unhandledPromiseErrors = mutableListOf<UnhandledJsPromiseError>()

    companion object {
        const val ITERATION_COUNT = 1000  // for miniBenchmark

        @BeforeClass
        @JvmStatic
        fun setUpClass() {
            Timber.plant(Timber.DebugTree())
        }
    }

    @After
    fun cleanUp() {
        printErrors()
        errors.clear()
        unhandledPromiseErrors.clear()

        jsBridge?.let {
            runBlocking {
                waitForDone(it)
                jsBridge?.release()
                waitForDone(it)
            }
        }
    }

    @Test
    fun testStart() {
        // WHEN
        val subject = JsBridge(
            InstrumentationRegistry.getInstrumentation().context
        )
        subject.registerErrorListener(errorListener)
        subject.start()

        runBlocking { waitForDone(subject) }

        // THEN
        assertTrue(errors.isEmpty())
    }

    @Test
    fun testEvaluateStringWithoutRetVal() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN

        runBlocking { waitForDone(subject) }

        // THEN
        assertTrue(errors.isEmpty())
        verify { jsToNativeFunctionMock(eq("testString")) }
    }

    @Test
    fun testEvaluateStringWithoutRetValWithError() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        val js = """
            invalid.javaScript.instruction
            """
        subject.evaluateNoRetVal(js)

        runBlocking { waitForDone(subject) }

        // THEN
        val stringEvaluationError = errors.singleOrNull() as? JsStringEvaluationError
        assertNotNull(stringEvaluationError)
        assertEquals(stringEvaluationError.js, js)
    }

    @Test
    fun testEvaluateStringUnit() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        val js = """nativeFunctionMock("testString");"""
        subject.evaluateBlocking<Unit>(js)

        // THEN
        assertTrue(errors.isEmpty())
        verify { jsToNativeFunctionMock(eq("testString")) }
    }

    @Test
    fun testEvaluateStringUnitWithError() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        val js = "invalid.javaScript.instruction"
        val jsException1: JsException = assertFailsWith {
            subject.evaluateBlocking<Unit>(js)
        }
        val jsException2 = runBlocking {
            assertFailsWith<JsException> {
                subject.evaluate(js)
            }
        }

        // THEN
        assertEquals(jsException1.message?.contains("invalid"), true)
        assertEquals(jsException1.message, jsException2.message)
        assertTrue(errors.isEmpty())
    }

    @Test
    fun testEvaluateString() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // THEN
        runBlocking {
            assertNull(subject.evaluate("null"))
            assertNull(subject.evaluate("undefined"))

            assertEquals(subject.evaluate("\"1.5+1\""), "1.5+1")  // String
            assertEquals(subject.evaluate("1.5+1"), 2)  // Int
            assertEquals(subject.evaluate("1.5+1"), 2L)  // Long
            assertEquals(subject.evaluate("1.5+1"), 2.5)  // Double
            assertEquals(subject.evaluate("1.5+1"), 2.5f)  // Float

            // Throwing string from JS
            val exceptionFromString: JsException = assertFailsWith { subject.evaluate("""throw "Error string";""") }
            assertEquals(exceptionFromString.message, "Error string")
            assertEquals(exceptionFromString.jsonValue, "\"Error string\"")

            // Throwing Error from JS
            val exceptionFromError: JsException = assertFailsWith { subject.evaluate("""throw new Error("Error message");""") }
            assertEquals(exceptionFromError.message?.contains("Error message"), true)
            assertEquals(exceptionFromError.jsonValue?.toPayload(), """{message: "Error message"}""".toPayload())

            // Throwing Object from JS
            val exceptionFromObject: JsException = assertFailsWith { subject.evaluate("""throw {message: "Error object", hint: "Just a test"};""") }
            assertEquals(exceptionFromObject.jsonValue?.toPayload(), """{message: "Error object", hint: "Just a test"}""".toPayload())

            // Pending promise
            val resolveFuncJsValue = JsValue(subject)
            val jsPromise: Deferred<Int> = subject.evaluate("new Promise(function(resolve) { $resolveFuncJsValue = resolve; });")
            assertTrue(jsPromise.isActive)

            // Resolve pending promise
            subject.evaluateNoRetVal("$resolveFuncJsValue(69);")
            assertEquals(jsPromise.await(), 69)

            // Resolved promise
            assertEquals(subject.evaluate("new Promise(function(resolve) { resolve(123); });"), 123)
            assertEquals(subject.evaluate<Deferred<Int>>("new Promise(function(resolve) { resolve(123); });").await(), 123)

            // Rejected promise
            val jsPromiseException: JsException = assertFailsWith {
                subject.evaluate<Int>("new Promise(function(resolve, reject) { reject(new Error('JS test error')); });")
            }
            val jsPromiseErrorPayload = jsPromiseException.jsonValue?.toPayloadObject()
            assertEquals(jsPromiseErrorPayload?.getString("message"), "JS test error")

            // Array
            assertArrayEquals(subject.evaluate<Array<String>>("""["a", "b", "c"]"""), arrayOf("a", "b", "c"))  // Array<String>
            assertArrayEquals(subject.evaluate("""[true, false, true]"""), booleanArrayOf(true, false, true))  // BooleanArray
            assertArrayEquals(subject.evaluate<Array<Boolean>>("""[true, false, true]"""), arrayOf(true, false, true))  // Array<Boolean>
            assertArrayEquals(subject.evaluate("""[1.0, 2.2, 3.8]"""), intArrayOf(1, 2, 3))  // IntArray
            assertArrayEquals(subject.evaluate<Array<Int>>("""[1.0, 2.2, 3.8]"""), arrayOf(1, 2, 3))  // Array<Int>
            assertArrayEquals(subject.evaluate("""[1.0, 2.2, 3.8]"""), longArrayOf(1L, 2L, 3L))  // LongArray
            assertArrayEquals(subject.evaluate<Array<Long>>("""[1.0, 2.2, 3.8]"""), arrayOf(1L, 2L, 3L))  // Array<Long>
            assertTrue(subject.evaluate<DoubleArray>("""[1.0, 2.2, 3.8]""").contentEquals(doubleArrayOf(1.0, 2.2, 3.8)))  // DoubleArray
            assertArrayEquals(subject.evaluate<Array<Double>>("""[1.0, 2.2, 3.8]"""), arrayOf(1.0, 2.2, 3.8))  // Array<Double>
            assertTrue(subject.evaluate<FloatArray>("""[1.0, 2.2, 3.8]""").contentEquals(floatArrayOf(1.0f, 2.2f, 3.8f)))  // FloatArray
            assertArrayEquals(subject.evaluate<Array<Float>>("""[1.0, 2.2, 3.8]"""), arrayOf(1.0f, 2.2f, 3.8f))  // Array<Float>

            // 2D-Arrays
            assertArrayEquals(subject.evaluate<Array<Array<Int>>>("""[[1, 2], [3, 4]]"""), arrayOf(arrayOf(1, 2), arrayOf(3, 4)))  // Array<Array<Int>>
            assertArrayEquals(subject.evaluate<Array<Array<Int?>>>("""[[1, 2], [null, 4]]"""), arrayOf(arrayOf(1, 2), arrayOf(null, 4)))  // Array<Array<Int?>>
            assertArrayEquals(subject.evaluate<Array<IntArray>>("""[[1, 2], [3, 4]]"""), arrayOf(intArrayOf(1, 2), intArrayOf(3, 4)))  // Array<IntArray>>

            // Array with optionals
            assertArrayEquals(subject.evaluate<Array<String?>>("""["a", null, "c"]"""), arrayOf("a", null, "c"))  // Array<String?>
            assertArrayEquals(subject.evaluate<Array<Boolean?>>("""[false, null, true]"""), arrayOf(false, null, true))  // Array<Boolean?>
            assertArrayEquals(subject.evaluate<Array<Int?>>("""[1.0, null, 3.8]"""), arrayOf(1, null, 3))  // Array<Int?>
            assertArrayEquals(subject.evaluate<Array<Double?>>("""[1.0, null, 3.8]"""), arrayOf(1.0, null, 3.8))  // Array<Double?>
            assertArrayEquals(subject.evaluate<Array<Float?>>("""[1.0, null, 3.8]"""), arrayOf(1.0f, null, 3.8f))  // Array<Float?>

            // Array of (any) objects
            assertArrayEquals(subject.evaluate<Array<Any>>("""[1.0, "hello", null]"""), arrayOf(1.0, "hello", null))  // Array<Any?>

            // JSON
            assertEquals(
                subject.evaluate<JsonObjectWrapper>("({key1: 1, key2: \"value2\"})").toPayload(),
                JsonObjectWrapper("key1" to 1, "key2" to "value2").toPayload()
            )
        }
    }

    @Test
    fun testEvaluateLocalFile() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        // androidTestDuktape asset file "test.js"
        // - content:
        // nativeFunctionMock("localFileString");
        subject.evaluateLocalFile("js/test.js")

        runBlocking { waitForDone(subject) }

        // THEN
        assertTrue(errors.isEmpty())
        verify { jsToNativeFunctionMock(eq("localFileString")) }
    }

    @Test
    fun testEvaluateLocalFileWithError() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        subject.evaluateLocalFile("non-existing/file.js")

        runBlocking { waitForDone(subject) }

        // THEN
        val fileEvaluationError = errors.firstOrNull() as? JsFileEvaluationError
        assertNotNull(fileEvaluationError)
        assertEquals(fileEvaluationError.fileName, "non-existing/file.js")
    }

    @Test
    fun testJsValue() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        val strValue = JsValue(subject, "\"123\"")
        val intValue = JsValue(subject, "123")
        val errorValue = JsValue(subject, "invalid.js.code")
        val resolvedPromiseValue = JsValue(subject,
            "new Promise(function(resolve) { resolve(123); })")
        val rejectedPromiseValue = JsValue(subject,
            "new Promise(function(resolve, reject) { reject(\"wrong\"); })")
        val resolvedPromiseAwaitValue = resolvedPromiseValue.awaitAsync()
        val rejectedPromiseAwaitValue = rejectedPromiseValue.awaitAsync()

        // THEN (with blocking methods)
        run {
            val evaluatedStrValue: String = subject.evaluateBlocking("$strValue")
            val evaluatedIntValue = subject.evaluateBlocking<Int>("$intValue")

            assertEquals(strValue.evaluateBlocking(), evaluatedStrValue)
            assertEquals(intValue.evaluateBlocking(), evaluatedIntValue)

            assertEquals(strValue.evaluateBlocking(), "123")
            assertEquals(intValue.evaluateBlocking(), 123)

            assertFailsWith<JsException> {
                errorValue.evaluateBlocking<Unit>()
            }
        }

        // THEN (with suspending methods)
        runBlocking {
            val evaluatedStrValue: String = subject.evaluate("$strValue")
            val evaluatedIntValue = subject.evaluate<Int>("$intValue")

            assertEquals(strValue.evaluate(), evaluatedStrValue)
            assertEquals(intValue.evaluate(), evaluatedIntValue)

            assertEquals(strValue.evaluate(), "123")
            assertEquals(intValue.evaluate(), 123)
        }

        // THEN (with async methods)
        val evaluatedStrValueAsync = subject.evaluateAsync<String>("$strValue")
        val evaluatedIntValueAsync = subject.evaluateAsync<Int>("$intValue")

        runBlocking {
            assertEquals(strValue.evaluateAsync<String>().await(), evaluatedStrValueAsync.await())
            assertEquals(intValue.evaluateAsync<Int>().await(), evaluatedIntValueAsync.await())

            assertEquals(strValue.evaluateAsync<String>().await(), "123")
            assertEquals(intValue.evaluateAsync<Int>().await(), 123)
        }

        // THEN (evaluate promise)
        runBlocking {
            val promiseValue: Int = resolvedPromiseValue.evaluate()
            assertEquals(promiseValue, 123)

            assertEquals(resolvedPromiseAwaitValue.await().evaluate(), 123)

            val awaitError = assertFailsWith<JsException> {
                rejectedPromiseAwaitValue.await().evaluate()
            }
            assertEquals(awaitError.jsonValue?.toPayload()?.stringValue(), "wrong")
        }

        // THEN (promise await)
        runBlocking {
            val error = assertFailsWith<JsException> {
                rejectedPromiseValue.evaluate()
            }
            val awaitError = assertFailsWith<JsException> {
                rejectedPromiseValue.await()
            }
            assertEquals(error.jsonValue?.toPayload()?.stringValue(), "wrong")
            assertEquals(awaitError.jsonValue?.toPayload()?.stringValue(), "wrong")
        }

        // THEN (null JsValue)
        runBlocking {
            // Non-nullable JsValue with JS var = "null" or "undefined"
            val nonNullableJsValueNull: JsValue = subject.evaluate("null")
            val nonNullableJsValueUndefined: JsValue = subject.evaluate("undefined")
            assertNull(nonNullableJsValueNull.evaluate())
            assertNull(nonNullableJsValueUndefined.evaluate())

            // Nullable JsValue with JS var = "null" or "undefined"
            val nullableJsValueNull: JsValue? = subject.evaluate("null")
            val nullableJsValueUndefined: JsValue? = subject.evaluate("undefined")
            assertNull(nullableJsValueNull)
            assertNull(nullableJsValueUndefined)
        }

        // THEN (null JsonObjectWrapper)
        runBlocking {
            // Non-nullable JsonObjectWrapper with JS var = "null" or "undefined"
            val nonNullableJsonObjectWrapperNull: JsonObjectWrapper = subject.evaluate("null")
            val nonNullableJsonObjectWrapperUndefined: JsonObjectWrapper = subject.evaluate("undefined")
            assertEquals(nonNullableJsonObjectWrapperNull.jsonString, "null")
            assertEquals(nonNullableJsonObjectWrapperUndefined, JsonObjectWrapper.Undefined)

            // Nullable JsonObjectWrapper with JS var = "null" or "undefined"
            val nullableJsonObjectWrapperNull: JsonObjectWrapper? = subject.evaluate("null")
            val nullableJsonObjectWrapperUndefined: JsonObjectWrapper? = subject.evaluate("undefined")
            assertNull(nullableJsonObjectWrapperNull)
            assertNull(nullableJsonObjectWrapperUndefined)
        }

        assertTrue(errors.isEmpty())
    }

    @Test
    fun testUnhandledPromiseRejection() {
        if (BuildConfig.FLAVOR == "quickjs") {
            // Not supported (yet) on QuickJS
            return
        }

        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        subject.evaluateNoRetVal("""
            new Promise(function(resolve, reject) {
              throw Error("Test unhandled promise rejection");
            });
        """.trimIndent())

        runBlocking { waitForDone(subject) }

        // THEN
        val unhandledJsPromiseError = unhandledPromiseErrors.firstOrNull()
        assertNotNull(unhandledJsPromiseError?.jsException)
        assertEquals(unhandledJsPromiseError?.jsException?.message, "Test unhandled promise rejection")
        assertEquals(unhandledJsPromiseError?.jsException?.jsonValue?.toPayloadObject()?.getString("message"), "Test unhandled promise rejection")
    }

    @Test
    fun testJsErrorStack() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        val jsException: JsException = assertFailsWith {
            subject.evaluateBlocking<Unit>("""
                function testFunction1() {
                  throw new Error("function1exception");
                }
                
                function testFunction2() {
                  testFunction1();
                }
                
                function testFunction3() {
                  testFunction2();
                }
                
                testFunction3();
            """.trimIndent()
            )
        }

        // THEN
        assertNotNull(jsException)
        assertEquals(true, jsException.message?.contains("function1exception"))

        // Expected stack trace:
        // - at JavaScript.testFunction1 (eval:2)
        // - at JavaScript.testFunction2 (eval:6)
        // - at JavaScript.testFunction3 (eval:10)
        // - at JavaScript.eval (eval:13)
        // - ...
        assertTrue(jsException.stackTrace.size >= 4)
        jsException.stackTrace[0].let { e ->
            assertEquals(e.className, "JavaScript")
            assertEquals(e.methodName, "testFunction1")
            assertEquals(e.fileName, "eval")
            assertEquals(e.lineNumber, 2)
        }
        jsException.stackTrace[1].let { e ->
            assertEquals(e.className, "JavaScript")
            assertEquals(e.methodName, "testFunction2")
            assertEquals(e.fileName, "eval")
            assertEquals(e.lineNumber, 6)
        }
        jsException.stackTrace[2].let { e ->
            assertEquals(e?.className, "JavaScript")
            assertEquals(e?.methodName, "testFunction3")
            assertEquals(e?.fileName, "eval")
            assertEquals(e?.lineNumber, 10)
        }
        jsException.stackTrace[3].let { e ->
            assertEquals(e.className, "JavaScript")
            assertEquals(true, """^<?eval>?$""".toRegex().matches(e.methodName))
            assertEquals(e?.fileName, "eval")
            assertEquals(e?.lineNumber, 13)
        }
        jsException.stackTrace[4].let { e ->
            assert(e.className.endsWith(".JsBridge"))
            assertEquals(e.methodName, "jniEvaluateString")
            assertEquals(e.fileName, "JsBridge.kt")
        }
    }

    @Test
    fun testJavaExceptionInJs() {
        // GIVEN
        val subject = createAndSetUpJsBridge()
        val createExceptionNative = JsValue.fromNativeFunction0<Unit>(subject) { throw Exception("Kotlin exception") }

        // WHEN
        val jsException: JsException = assertFailsWith {
            subject.evaluateBlocking<Unit>("""
                function testFunction() {
                  $createExceptionNative();
                }
                
                testFunction();
            """.trimIndent())
        }
        createExceptionNative.hold()

        // THEN
        assertNotNull(jsException)
        assertEquals(true, jsException.message?.contains("Kotlin exception"))

        jsException.printStackTrace()

        // Expected stack trace:
        // - at JavaScript.testFunction (eval:2)
        // - at JavaScript.eval (eval:5)
        // - ...
        // cause: Exception: Kotlin exception
        // - ...
        assertTrue(jsException.stackTrace.size >= 2)
        jsException.stackTrace[0].let { e ->
            assertEquals(e.className, "JavaScript")
            assertEquals(e.methodName, "testFunction")
            assertEquals(e.fileName, "eval")
            assertEquals(e.lineNumber, 2)
        }
        jsException.stackTrace[1].let { e ->
            assertEquals(e.className, "JavaScript")
            assertEquals(true, """^<?eval>?$""".toRegex().matches(e.methodName))
            assertEquals(e?.fileName, "eval")
            assertEquals(e?.lineNumber, 5)
        }
        jsException.stackTrace[2].let { e ->
            assert(e.className.endsWith(".JsBridge"))
            assertEquals(e.methodName, "jniEvaluateString")
            assertEquals(e.fileName, "JsBridge.kt")
        }
        assert(jsException.cause is java.lang.Exception)
        assertEquals(jsException.cause?.message, "Kotlin exception")
    }

    @Test
    fun testNullPointerException() {
        // GIVEN
        val subject = createAndSetUpJsBridge()
        val testFunctionNative = JsValue.fromNativeFunction1(subject) { _: Int -> Unit }

        // WHEN
        val jsException: JsException = assertFailsWith {
            subject.evaluateBlocking<Unit>("$testFunctionNative(null);")
        }
        testFunctionNative.hold()

        // THEN
        assertNotNull(jsException.cause is NullPointerException)
    }

    @Test
    fun testTooLargeArray() {
        // GIVEN
        val subject = createAndSetUpJsBridge()
        val arrayLength = Int.MAX_VALUE

        // THEN
        assertFailsWith<OutOfMemoryError> {
            subject.evaluateBlocking<BooleanArray>("new Array($arrayLength);")
        }
        assertFailsWith<OutOfMemoryError> {
            subject.evaluateBlocking<IntArray>("new Array($arrayLength);")
        }
        assertFailsWith<OutOfMemoryError> {
            subject.evaluateBlocking<LongArray>("new Array($arrayLength);")
        }
        assertFailsWith<OutOfMemoryError> {
            subject.evaluateBlocking<FloatArray>("new Array($arrayLength);")
        }
        assertFailsWith<OutOfMemoryError> {
            subject.evaluateBlocking<DoubleArray>("new Array($arrayLength);")
        }
        assertFailsWith<OutOfMemoryError> {
            subject.evaluateBlocking<Array<Any>>("new Array($arrayLength);")
        }
    }

    @Test
    fun miniBenchmark() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // Pure native
        // -> 11ms (Samsung S5, 100000 iterations)
        val calcMagicNativeFunc: suspend () -> Int = {
            withContext(Dispatchers.Default) {
                var m = 0
                for (i in 0 until ITERATION_COUNT) {
                    m += i * 2
                    if (m > 1000000) m = -m
                }
                m
            }
        }

        // Pure JS
        // -> 31 ms (Duktape, Samsung S5, 100000 iterations)
        // -> 84 ms (QuickJS, Samsung S5, 100000 iterations)
        val calcMagicJsFunc: suspend () -> Int = JsValue.newFunction(subject, """
            |var m = 0;
            |for (var i = 0; i < $ITERATION_COUNT; i++) {
            |  m += i * 2;
            |  if (m > 1000000) m = -m;
            |}
            |return m;
            |""".trimMargin()
        ).mapToNativeFunction0()

        // Native loop, calls JS function
        // -> 22.5s (Duktape, Samsung S5, 100000 iterations, from main thread)
        // -> 10.5s (Duktape, Samsung S5, 100000 iterations, from JS thread)
        // -> 17s (QuickJS, Samsung S5, 100000 iterations, from main thread)
        // -> 6s (QuickJS, Samsung S5, 100000 iterations, from JS thread)
        val updateMagicJsFunc: suspend (Int, Int) -> Int = JsValue.newFunction(subject, "a", "i", """
            |var n = a + i * 2;
            |if (n > 1000000) n = -n;
            |return n;
            |""".trimMargin()
        ).mapToNativeFunction2()
        val calcMagicCallJsFunctionInsideNativeLoop: suspend () -> Int = {
            var m = 0
            for (i in 0 until ITERATION_COUNT) {
                m = updateMagicJsFunc(m, i)
            }
            m
        }

        // Native loop, evaluate JS string
        // -> 78s (Duktape, Samsung S5, 100000 iterations, from main thread)
        // -> 53s (Duktape, Samsung S5, 100000 iterations, from JS thread)
        // -> 216s (QuickJS, Samsung S5, 100000 iterations, from main thread)
        // -> 54s (QuickJS, Samsung S5, 100000 iterations, from JS thread)
        val calcMagicEvaluateJsInsideNativeLoop: suspend () -> Int = {
            var m = 0
            for (i in 0 until ITERATION_COUNT) {
                m = subject.evaluate("""
                  |var n = $m + $i * 2;
                  |if (n > 1000000) n = -n;
                  |n;
                """.trimMargin())
            }
            m
        }

        // JS loop, call native functions (191ms for 20000 iterations)
        // -> 3.7s ms (Duktape, Samsung S5, 100000 iterations)
        // -> 5.5s ms (QuickJS, Samsung S5, 100000 iterations)
        val updateMagicNativeFuncJsValue = JsValue.fromNativeFunction2(subject) { a: Int, i: Int ->
            var n = a + i * 2
            if (n > 1000000) n = -n
            n
        }
        val calcMagicCallNativeFunctionInsideJsLoop: suspend () -> Int = JsValue.newFunction(subject, """
            |var m = 0
            |for (var i = 0; i < $ITERATION_COUNT; ++i) {
            |  m = $updateMagicNativeFuncJsValue(m, i)
            |}
            |return m;
            |""".trimMargin()
        ).mapToNativeFunction0()

        runBlocking {
            delay(500)

            Timber.i("Executing calcMagicNativeFunc()...")
            val expectedResult = calcMagicNativeFunc()
            Timber.i("-> result is $expectedResult")

            Timber.i("Executing calcMagicJsFunc()...")
            var result = calcMagicJsFunc()
            Timber.i("-> result is $result")
            assertEquals(result, expectedResult)

            Timber.i("Executing calcMagicCallJsFunctionInsideNativeLoop()...")
            result = calcMagicCallJsFunctionInsideNativeLoop()
            Timber.i("-> result is $result")
            assertEquals(result, expectedResult)

            Timber.i("Executing calcMagicCallJsFunctionInsideNativeLoop() in JS thread...")
            withContext(subject.coroutineContext) {
                result = calcMagicCallJsFunctionInsideNativeLoop()
                Timber.i("-> result is $result")
                assertEquals(result, expectedResult)
            }

            Timber.i("Executing calcMagicEvaluateJsInsideNativeLoop()...")
            result = calcMagicEvaluateJsInsideNativeLoop()
            Timber.i("-> result is $result")
            assertEquals(result, expectedResult)

            Timber.i("Executing calcMagicEvaluateJsInsideNativeLoop() in JS thread...")
            withContext(subject.coroutineContext) {
                result = calcMagicEvaluateJsInsideNativeLoop()
                Timber.i("-> result is $result")
                assertEquals(result, expectedResult)
            }

            Timber.i("Executing calcMagicCallNativeFunctionInsideJsLoop()...")
            result = calcMagicCallNativeFunctionInsideJsLoop()
            Timber.i("-> result is $result")
            assertEquals(result, expectedResult)

            // Make sure that the JS value does not create garbage-collected as calcMagicMixed3Func()
            // access the value by its name!
            updateMagicNativeFuncJsValue.hold()
        }
    }

    interface StressJsApi: NativeToJsInterface {
        fun registerCallback(cb: (Int) -> Unit)
        fun start()
        fun helloAsync(): Deferred<String>
    }

    interface StressNativeApi: JsToNativeInterface {
        fun registerCallback(cb: (Int) -> Unit)
        fun start()
        fun helloAsync(): Deferred<String>
    }

    private fun stressTestHelper() {
        val subject = JsBridge(InstrumentationRegistry.getInstrumentation().context)

        subject.start(okHttpClient = okHttpClient)

        val jsExpectations = JsExpectations()
        val jsExpectationsJsValue = JsValue.fromNativeObject(subject, jsExpectations)


        // Native to JS
        // ---

        val jsApi: StressJsApi = JsValue(subject, """({
            |  cb: null,
            |  registerCallback: function(cb) {
            |    this.cb = cb;
            |  },
            |  start: function() {
            |    for (var i = 0; i < 100; ++i) {
            |      this.cb(i);
            |    }
            |  },
            |  helloAsync: function() {
            |    return new Promise(
            |      function(resolve) {
            |        resolve("ok");
            |      }
            |    );
            |  }
            |})""".trimMargin()
        ).mapToNativeObject()

        val nativeCbMock = mockk<(Int) -> Unit>(relaxed = true)
        jsApi.registerCallback(nativeCbMock)
        jsApi.start()

        runBlocking {
            assertEquals(jsApi.helloAsync().await(), "ok")
        }

        runBlocking {
            verify(exactly = 100, timeout = 15000L) { nativeCbMock(any()) }
        }


        // JS to native
        // ---

        val nativeApi = object: StressNativeApi {
            private var cb: ((Int) -> Unit)? = null

            override fun registerCallback(cb: (Int) -> Unit) {
                this.cb = cb
            }

            override fun start() {
                for (i in 0 until 100) {
                    cb?.invoke(i)
                }
            }

            override fun helloAsync(): Deferred<String> {
                return CompletableDeferred("ok")
            }
        }

        val nativeApiJsValue = JsValue.fromNativeObject(subject, nativeApi)
        subject.evaluateBlocking<Unit>("""
            |function jsCb(i) {
            |  $jsExpectationsJsValue.addExpectation("ex" + i, i);  // TODO: add an error here the crash log will be useless...
            |}
            |$nativeApiJsValue.registerCallback(jsCb);
            |$nativeApiJsValue.start();""".trimMargin())

        assertEquals(
            subject.evaluateBlocking("$nativeApiJsValue.helloAsync()"),
            "ok"
        )

        for (i in 0 until 100) {
            jsExpectations.checkEquals("ex$i", i)
        }

        // Hold the JS values whose associated JS name is used in string evaluation
        jsExpectationsJsValue.hold()
        nativeApiJsValue.hold()

        assertTrue(jsExpectations.isEmpty)
        subject.release()
    }

    @Test
    @Ignore("Very long test")
    fun stressTest() {
        for (i in 0..10000) {
            Timber.i("stressTest() - JsBridge instance ${i + 1}")
            stressTestHelper()
        }
    }

    @Test
    fun testMapJsFunctionToNative() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        val calcSumJsValue = JsValue.newFunction(subject, "a", "b", "return a + b;")
        val calcSumJsValueAnonymousFunction = JsValue(subject, """
            |(function(a, b) {
            |  return a + b;
            |})
            |""".trimMargin()
        )
        val calcSum: suspend (Int, Int) -> Int = calcSumJsValue.mapToNativeFunction2()
        val calcSumFromAnonymousFunction: suspend (Int, Int) -> Int = calcSumJsValueAnonymousFunction.mapToNativeFunction2()
        val calcSumBlocking: (Int, Int) -> Int = calcSumJsValue.mapToNativeBlockingFunction2()
        val calcSumWrongSignature: (Int, String) -> Int = calcSumJsValue.mapToNativeBlockingFunction2()

        val invalidFunction: suspend () -> Unit = JsValue.newFunction(subject, "invalid").mapToNativeFunction0()

        val createJsObject: suspend (Int, String) -> JsonObjectWrapper =
            JsValue.newFunction(subject, "a", "b", "return {key1: a, key2: b};")
                .mapToNativeFunction2()

        val createCalcSumFunc: suspend () -> JsValue = JsValue.newFunction(subject, "return $calcSumJsValue;")
            .mapToNativeFunction0()

        val throwException: suspend () -> Unit = JsValue.newFunction(subject, "", "throw 'JS exception from function';")
            .mapToNativeFunction0()

        val getPromiseJsValue = JsValue.newFunction(subject, "name", """
            |return new Promise(function(resolve) {
            |  resolve("Hello " + name + "!");
            |});
            |""".trimMargin()
        )
        val getPromise: suspend (name: String) -> String = getPromiseJsValue.mapToNativeFunction1()
        val getPromiseAsync: suspend (name: String) -> Deferred<String> = getPromiseJsValue.mapToNativeFunction1()

        val getFailedPromise: suspend (name: String) -> String = JsValue.newFunction(subject, """
            |return new Promise(function(resolve, reject) {
            |  reject("Oh no!");
            |});
            |""".trimMargin()
        ).mapToNativeFunction1()

        // THEN
        assertEquals(calcSumBlocking(3, 7), 10)

        runBlocking {
            // Call JS function calcSum(3, 4)
            val sum = calcSum(3, 4)
            assertEquals(sum, 7)

            // Call JS function calcSumFromAnonymousFunction(3, 4)
            val sumFromAnonymousFunction = calcSumFromAnonymousFunction(3, 4)
            assertEquals(sumFromAnonymousFunction, 7)

            // Call JS function calcSumWrongSignature(3, "four")
            assertFailsWith<IllegalArgumentException> {
                calcSumWrongSignature(3, "four")
            }

            // Call JS function invalidFunction()
            val invalidFunctionException: JsException = assertFailsWith {
                invalidFunction()
            }
            assertEquals(invalidFunctionException.message?.contains("invalid"), true)

            // Call JS function createJsObject(69, "sixty-nine")
            val jsObject = createJsObject(69, "sixty-nine")
            assertEquals(jsObject.toPayload(), PayloadObject.fromValues("key1" to 69, "key2" to "sixty-nine"))

            // Call JS function createCalcSumFunc()(2, 2)
            val calcSum2: suspend (Int, Int) -> Int = createCalcSumFunc().mapToNativeFunction2()
            val sum2 = calcSum2(2, 2)
            assertEquals(sum2, 4)

            // Call JS function throwException()
            val jsException: JsException = assertFailsWith {
                throwException()
            }
            assertEquals(jsException.message, "JS exception from function")

            // Call JS function getPromise("testPromise")
            val promiseResult = getPromise("testPromise")
            assertEquals(promiseResult, "Hello testPromise!")

            // Call JS function getPromiseAsync("testPromise")
            val promiseResultDeferred = getPromiseAsync("testPromise")
            assertEquals(promiseResultDeferred.await(), "Hello testPromise!")

            // Call JS function getFailedPromise("testPromise")
            val promiseException: JsException = assertFailsWith {
                getFailedPromise("testPromise")
            }
            val promiseErrorPayload = promiseException.jsonValue?.toPayload()
            assertEquals(promiseErrorPayload?.stringValue(), "Oh no!")
        }

        assertTrue(errors.isEmpty())
    }

    @Test
    fun testMapNativeFunctionToJs() {
        // GIVEN
        val subject = createAndSetUpJsBridge()
        var flag = false

        // WHEN
        val setFlag = JsValue.fromNativeFunction0(subject) { flag = true }
        val toUpperCaseNative = JsValue.fromNativeFunction1(subject) { s: String -> s.toUpperCase() }
        val calcSumNative = JsValue.fromNativeFunction2(subject) { a: Int, b: Int -> a + b }
        val setCustomTimeout: (() -> Unit, Long) -> Unit = { cb, msecs ->
            GlobalScope.launch(Dispatchers.Main) {
                delay(msecs)
                cb()
            }
        }
        JsValue.fromNativeFunction2(subject, setCustomTimeout).assignToGlobal("setCustomTimeout")

        subject.evaluateBlocking<Unit>("$setFlag()")
        assertTrue(flag)

        assertEquals(subject.evaluateBlocking("""$toUpperCaseNative("test string")"""), "TEST STRING")
        assertEquals(subject.evaluateBlocking("$calcSumNative(7, 8)"), 15)

        subject.evaluateNoRetVal("""
            |setCustomTimeout(function() {
            |  nativeFunctionMock(true);
            |}, 200);
        """.trimMargin())
        verify(timeout = 3000L) { jsToNativeFunctionMock(eq(true)) }
    }

    // Port of similar test in Duktape Android
    @Test
    fun testTwoDimensionalArray() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        val transposeJs: suspend (Array<IntArray>) -> Array<IntArray> = JsValue.newFunction(subject, "matrix", """
            |return matrix[0].map(function(col, i) {
            |  return matrix.map(function(row) {
            |    return row[i];
            |  })
            |});
            |""".trimMargin()
        ).mapToNativeFunction1()

        val matrix = Array(2) { IntArray(2) { 0 } }
        matrix[0][0] = 1
        matrix[0][1] = 2
        matrix[1][0] = 3
        matrix[1][1] = 4

        val expected = Array(2) { IntArray(2) { 0 } }
        expected[0][0] = 1
        expected[1][0] = 2
        expected[0][1] = 3
        expected[1][1] = 4

        runBlocking {
            // WHEN
            val result = transposeJs(matrix)

            // THEN
            assertArrayEquals(expected, result)
        }
    }

    @Test
    fun testRegisterJsToNativeInterface() {
        // GIVEN
        var receivedBool1: Boolean? = null
        var receivedBool2: Boolean? = null
        var receivedString: String? = null
        var receivedJsonObject1: JsonObjectWrapper? = null
        var receivedJsonObject2: JsonObjectWrapper? = null
        
        val nativeApi = object : TestNativeApiInterface {
            override fun nativeMethodReturningTrue() = true
            override fun nativeMethodReturningFalse() = false
            override fun nativeMethodReturningString() = "Hello JsBridgeTest!"

            override fun nativeMethodReturningJsonObject() = JsonObjectWrapper("""
                {"returnKey1": 1, "returnKey2": "returnValue2"}
            """)

            override fun nativeMethodReturningResolvedDeferred(): Deferred<String> {
                val deferred = CompletableDeferred<String>()
                deferred.complete("returned deferred value")
                return deferred
            }

            override fun nativeMethodReturningRejectedDeferred(): Deferred<String> {
                val deferred = CompletableDeferred<String>()
                deferred.completeExceptionally(Exception("returned deferred error"))
                return deferred
            }

            override fun nativeMethodWithBool(b: Boolean?) {
                when {
                    receivedBool1 == null -> receivedBool1 = b
                    receivedBool2 == null -> receivedBool2 = b
                    else -> fail()
                }
            }

            override fun nativeMethodWithString(msg: String?) {
                receivedString = msg
            }

            override fun nativeMethodWithJsonObjects(jsonObject1: JsonObjectWrapper?, jsonObject2: JsonObjectWrapper?) {
                receivedJsonObject1 = jsonObject1
                receivedJsonObject2 = jsonObject2
            }

            override fun nativeMethodWithCallback(cb: (string: String, obj: JsonObjectWrapper, bool1: Boolean, bool2: Boolean, int: Int, double: Double, optionalInt1: Int?, optionalInt2: Int?) -> Unit) {
                cb("cbString", JsonObjectWrapper("""{"cbKey1": 1, "cbKey2": "cbValue2"}"""), true, false, 69, 16.64, 123, null)
            }

            override fun nativeMethodThrowingException() {
                throw Exception("Test native exception")
            }
        }

        val subject = JsBridge(InstrumentationRegistry.getInstrumentation().context)
        subject.start(okHttpClient = okHttpClient)

        val jsExpectations = JsExpectations()
        val jsExpectationsJsValue = JsValue.fromNativeObject(subject, jsExpectations)

        // WHEN
        val testNativeApi = JsValue.fromNativeObject(subject, nativeApi)
        val js = """
            var retBool1 = $testNativeApi.nativeMethodReturningTrue();
            var retBool2 = $testNativeApi.nativeMethodReturningFalse();
            var retString = $testNativeApi.nativeMethodReturningString();
            var retObject = $testNativeApi.nativeMethodReturningJsonObject();
            var retPromise = $testNativeApi.nativeMethodReturningResolvedDeferred();
            retPromise.then(function(deferredValue) {
              $jsExpectationsJsValue.addExpectation("resolvedDeferredValue", deferredValue);
            }).catch(function(e) {
              $jsExpectationsJsValue.addExpectation("unexpected", true);
            });
            retPromise = $testNativeApi.nativeMethodReturningRejectedDeferred();
            retPromise.then(function(deferredValue) {
              $jsExpectationsJsValue.addExpectation("unexpected", true);
            }).catch(function(e) {
              $jsExpectationsJsValue.addExpectation("rejectedDeferredError", e);
            });
            $testNativeApi.nativeMethodWithBool(retBool1);
            $testNativeApi.nativeMethodWithBool(retBool2);
            $testNativeApi.nativeMethodWithString(retString);
            $testNativeApi.nativeMethodWithJsonObjects(retObject, [1, "two", 3]);
            $testNativeApi.nativeMethodWithCallback(function(string, obj, bool1, bool2, int, double, optionalInt1, optionalInt2) {
              $jsExpectationsJsValue.addExpectation("cbString", string);
              $jsExpectationsJsValue.addExpectation("cbObj", obj);
              $jsExpectationsJsValue.addExpectation("cbBool1", bool1);
              $jsExpectationsJsValue.addExpectation("cbBool2", bool2);
              $jsExpectationsJsValue.addExpectation("cbInt", int);
              $jsExpectationsJsValue.addExpectation("cbDouble", double);
              $jsExpectationsJsValue.addExpectation("cbOptionalInt1", 123);
              $jsExpectationsJsValue.addExpectation("cbOptionalInt2", null);
            });
            try {
              $testNativeApi.nativeMethodThrowingException();
            } catch(e) {
              $jsExpectationsJsValue.addExpectation("nativeException", e);
            }
            """
        subject.evaluateNoRetVal(js)

        runBlocking { waitForDone(subject) }

        // THEN
        assertEquals(receivedBool1, true)
        assertEquals(receivedBool2, false)
        assertEquals(receivedString, "Hello JsBridgeTest!")
        assertEquals(receivedJsonObject1?.jsonString, """{"returnKey1":1,"returnKey2":"returnValue2"}""")
        assertEquals(receivedJsonObject2?.jsonString, """[1,"two",3]""")

        runBlocking { waitForDone(subject) }

        jsExpectations.checkDoesNotExist("unexpected")
        jsExpectations.checkEquals("resolvedDeferredValue", "returned deferred value")
        jsExpectations.checkEquals("rejectedDeferredError", "returned deferred error")
        jsExpectations.checkEquals("cbString", "cbString")
        jsExpectations.checkBlock("cbObj") { value: JsonObjectWrapper? ->
            assertEquals(value?.toPayload(), """{"cbKey1": 1, "cbKey2": "cbValue2"}""".toPayload())
        }
        jsExpectations.checkEquals("cbBool1", true)
        jsExpectations.checkEquals("cbBool2", false)
        jsExpectations.checkEquals("cbInt", 69)
        jsExpectations.checkEquals("cbDouble", 16.64)
        assertNotNull(jsExpectations.takeExpectation<Any>("nativeException"))
        assertTrue(jsExpectations.isEmpty)
        assertTrue(errors.isEmpty())
    }

    @Test
    fun testRegisterNativeToJsInterface() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        val jsExpectations = JsExpectations()
        val jsExpectationsJsValue = JsValue.fromNativeObject(subject, jsExpectations)

        data class CallbackValues(
            val string: String?,
            val jsonObject1: JsonObjectWrapper?, val jsonObject2: JsonObjectWrapper?,
            val bool1: Boolean?, val bool2: Boolean?,
            val int: Int?,
            val double: Double?,
            val optionalInt1: Int?,
            val optionalInt2: Int?
        )
        var callbackValues: CallbackValues? = null
        var unitCallbackCalled = false

        val testJsApi = JsValue(subject, """({
            jsMethodWithString: function(msg) {
              $jsExpectationsJsValue.addExpectation("stringArg", msg);
            },
            jsMethodWithJsonObjects: function(obj, arr) {
              $jsExpectationsJsValue.addExpectation("jsonObjectArg", obj);
              $jsExpectationsJsValue.addExpectation("jsonArrayArg", arr);
            },
            jsMethodWithJsValue: function(v) {
              $jsExpectationsJsValue.addExpectation("jsValueArg", v);
            },
            jsMethodWithCallback: function(cb) {
              var cbRet = cb("Callback message", {test1: "value1", test2: 2}, [1, 2, 3], true, false, 69, 123.456, 123, null);
              $jsExpectationsJsValue.addExpectation("cbRet", cbRet);
            },
            jsMethodWithUnitCallback: function(cb) {
              cb();
              $jsExpectationsJsValue.addExpectation("cbUnit", true);
            },
            jsMethodReturningFulfilledPromise: function(str) {
              return new Promise(function(resolve) {
                resolve(str);
              });
            },
            jsMethodReturningRejectedPromise: function(d) {
              return new Promise(function(resolve, reject) {
                reject(d);
              });
            },
            jsMethodReturningJsValue: function(str) {
              return str;
            }
        })""")

        val objectJson = """{"firstKey": "firstValue", "numericValue": 1664}"""
        val obj = JsonObjectWrapper(objectJson)
        assertNotNull(obj)

        val arrayJson = """["one", 2, "three"]""".trimMargin()
        val arr = JsonObjectWrapper(arrayJson)
        assertNotNull(arr)

        // WHEN
        val jsApi: TestJsApiInterface = testJsApi.mapToNativeObject()
        jsApi.jsMethodWithString("Hello JS!")
        jsApi.jsMethodWithJsonObjects(obj, arr)
        val jsValueParam = JsValue(subject, "\"This is a JS value\"")
        jsApi.jsMethodWithJsValue(jsValueParam)
        jsApi.jsMethodWithCallback { string, jsonObject1, jsonObject2, bool1, bool2, int, double, optionalInt1, optionalInt2 ->
            callbackValues = CallbackValues(string, jsonObject1, jsonObject2, bool1, bool2, int, double, optionalInt1, optionalInt2)
            "CallbackTestRetVal"
        }
        jsApi.jsMethodWithUnitCallback {
            unitCallbackCalled = true
        }
        val fulfilledPromise = jsApi.jsMethodReturningFulfilledPromise("Promise value")
        val rejectedPromise = jsApi.jsMethodReturningRejectedPromise(987.0)
        val jsValueAsync = jsApi.jsMethodReturningJsValue("JsValue value")

        runBlocking { waitForDone(subject) }

        // THEN
        assertNotNull(callbackValues)
        assertEquals(callbackValues!!.jsonObject1?.jsonString, """{"test1":"value1","test2":2}""")
        assertEquals(callbackValues!!.jsonObject2?.jsonString, "[1,2,3]")
        assertEquals(callbackValues!!.bool1, true)
        assertEquals(callbackValues!!.bool2, false)
        assertEquals(callbackValues!!.int, 69)
        assertEquals(callbackValues!!.double, 123.456)
        assertEquals(callbackValues!!.optionalInt1, 123)
        assertEquals(callbackValues!!.optionalInt2, null)
        assertTrue(unitCallbackCalled)
        assert(fulfilledPromise.isCompleted)
        runBlocking {
            assert(fulfilledPromise.isCompleted)
            assertEquals(fulfilledPromise.await(), "Promise value")
        }
        assert(rejectedPromise.isCancelled)
        runBlocking {
            val promiseException: JsException = assertFailsWith {
                rejectedPromise.await()
            }
            val promiseErrorPayload = promiseException.jsonValue?.toPayload()
            assertEquals(promiseErrorPayload?.doubleValue(), 987.0)
        }
        runBlocking {
            val jsValueString = jsValueAsync.await().evaluate<String>()
            assertEquals(jsValueString, "JsValue value")
        }

        jsExpectations.checkEquals("stringArg", "Hello JS!")
        jsExpectations.checkBlock("jsonObjectArg") { value: JsonObjectWrapper? ->
            assertEquals(value?.toPayload(), objectJson.toPayload())
        }
        jsExpectations.checkBlock("jsonArrayArg") { value: JsonObjectWrapper? ->
            assertEquals(value?.toPayload(), arrayJson.toPayload())
        }
        runBlocking {
            val jsValueArg = jsExpectations.takeExpectation<JsValue>("jsValueArg")
            assertNotNull(jsValueArg)
            assertTrue(subject.evaluateAsync<Boolean>("$jsValueArg == $jsValueParam").await())
        }
        jsExpectations.checkEquals("cbRet", "CallbackTestRetVal")
        assertTrue(jsExpectations.isEmpty)

        assertTrue(errors.isEmpty())
    }

    @Test
    fun testRegisterNativeToJsInterfaceFromPromise() {
        // GIVEN
        val subject = createAndSetUpJsBridge()
        val jsApiPromise = JsValue(subject, """
            new Promise(function(resolve) {
              resolve({
                jsMethodWithString: function(msg) {
                  nativeFunctionMock(msg);
                },
                jsMethodWithJsonObjects: function() {},  // unused
                jsMethodWithJsValue: function() {},  // unused
                jsMethodWithCallback: function() {},  // unused
                jsMethodWithUnitCallback: function() {},  // unused
                jsMethodReturningFulfilledPromise: function() {},  // unused
                jsMethodReturningRejectedPromise: function() {},  // unused
                jsMethodReturningJsValue: function() {}  // unused
              });
            });"""
        )

        // WHEN
        runBlocking {
            val jsApi: TestJsApiInterface = jsApiPromise.await().mapToNativeObject()
            jsApi.jsMethodWithString("Hello JS!")
        }

        // THEN
        verify(timeout = 1000) { jsToNativeFunctionMock(eq("Hello JS!")) }
        assertTrue(errors.isEmpty())
    }

    @Test
    fun testDestroy() {
        // WHEN
        val subject = createAndSetUpJsBridge()
        val testJsApi = JsValue(subject, """({
            jsMethodWithString: function() {},  // unused
            jsMethodWithJsonObjects: function() {},  // unused
            jsMethodWithJsValue: function() {},  // unused
            jsMethodWithCallback: function(cb) { cb(); },
            jsMethodWithUnitCallback: function(cb) { cb(); },
            jsMethodReturningFulfilledPromise: function() {},  // unused
            jsMethodReturningRejectedPromise: function() {},  // unused
            jsMethodReturningJsValue: function() {}  // unused
        })""")
        val jsApi: TestJsApiInterface = testJsApi.mapToNativeObject()

        var callbackCalled = false

        // WHEN
        subject.release()
        subject.evaluateNoRetVal("""nativeFunctionMock("Should not be called.");""")
        jsApi.jsMethodWithCallback { _, _, _, _, _, _, _, _, _ ->
            callbackCalled = true
            "CallbackRetVal"
        }

        // THEN
        // The JS callback and the native method should not have been triggered because we have
        // destroyed the interpreter
        // (and the app should not crash ;))
        runBlocking {
            waitForDone(subject)
            assertFalse(callbackCalled)
            verify(inverse = true) { jsToNativeFunctionMock(any()) }
        }
        assertTrue(errors.isEmpty())

        jsBridge = null  // avoid another release() in cleanUp()
    }

    @Test
    fun testSetTimeout() {
        // GIVEN
        val initialDelay = 500L
        val timeoutCount = 100
        val subject = createAndSetUpJsBridge()

        // WHEN
        var js = ""
        for (i in 1..timeoutCount) {
            js += """setTimeout(function() { nativeFunctionMock("timeout$i"); }, ${initialDelay + i * 10L});""" + "\n"
        }

        subject.evaluateNoRetVal(js)

        runBlocking { waitForDone(subject) }

        // THEN
        // no event is sent immediately
        verify(inverse = true) { jsToNativeFunctionMock(any()) }

        // AND THEN
        // events are sent in the right order
        verify(ordering = Ordering.SEQUENCE, timeout = initialDelay + timeoutCount * 10L * 2) {
            for (i in 1..timeoutCount) {
                jsToNativeFunctionMock("timeout$i")
            }
        }

        assertTrue(errors.isEmpty())
    }

    @Test
    fun testClearTimeout() {
        val events = mutableListOf<String>()

        every { jsToNativeFunctionMock(any()) } answers {
            events.add(args[0] as String)
        }

        val subject = createAndSetUpJsBridge()

        // WHEN
        val js = """
            |var timeout2Id = null;
            |setTimeout(function() {
            |  nativeFunctionMock("timeout1");
            |  clearTimeout(timeout2Id);
            |}, 0);
            |timeout2Id = setTimeout(function() {
            |  nativeFunctionMock("timeout2");
            |}, 100);
            |setTimeout(function() {
            |  nativeFunctionMock("timeout3");
            |}, 100);
        """.trimMargin()

        subject.evaluateNoRetVal(js)

        // THEN
        runBlocking {
            waitForDone(subject)
            delay(500)
        }
        verify(exactly = 2) { jsToNativeFunctionMock(neq("timeout2")) }

        // timeout1 should be skipped!
        assertEquals(events.size, 2)
        assertEquals(events[0], "timeout1")
        assertEquals(events[1], "timeout3")
        assertTrue(errors.isEmpty())
    }

    @Test
    fun testSetAndClearInterval() {
        val subject = createAndSetUpJsBridge()

        // WHEN
        val js = """
            var i = 1;
            var timeoutId = null;
            timeoutId = setInterval(function() {
              console.log("Inside interval");
              nativeFunctionMock("interval" + i);
              if (i == 3) {
                clearInterval(timeoutId);
              }
              i++;
            }, 50);
        """

        subject.evaluateNoRetVal(js)

        runBlocking { waitForDone(subject) }

        // THEN
        verify(ordering = Ordering.SEQUENCE, timeout = 1000) {
            jsToNativeFunctionMock(eq("interval1"))
            jsToNativeFunctionMock(eq("interval2"))
            jsToNativeFunctionMock(eq("interval3"))
        }

        // Stop *after* interval3
        verify(inverse = true, timeout = 1000) { jsToNativeFunctionMock(eq("interval4")) }

        assertTrue(errors.isEmpty())
    }

    @Test
    fun testXmlHttpRequest() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        val url = "https://test.url/api/request"
        val requestHeaders = hashMapOf("testRequestHeaderKey1" to "testRequestHeaderValue1", "testRequestHeaderKey2" to "testRequestHeaderValue2")
        val responseText = """{"testKey1": "testValue1", "testKey2": "testValue2"}"""
        val responseHeaders = """{"testResponseHeaderKey1": "testResponseHeaderValue1", "testResponseHeaderKey2": "testResponseHeaderValue2"}"""
        httpInterceptor.mockRequest(url, responseText, responseHeaders)

        // WHEN
        val js = """
            |var xhr = new XMLHttpRequest();
            |${requestHeaders.map {
                 """xhr.setRequestHeader("${it.key}", "${it.value}");"""
             }.joinToString("\n")}
            |xhr.responseType = "json";
            |xhr.open("GET", "$url");
            |xhr.send();
            |xhr.onload = function() {
            |  // Convert XHR headers value into JSON (see https://stackoverflow.com/a/37934624/9947065)
            |  var headers = xhr.getAllResponseHeaders().split('\r\n').reduce(function (acc, current, i) {
            |    var parts = current.split(new RegExp(' *: *'));
            |    if (parts.length === 2) acc[parts[0]] = parts[1];
            |    return acc;
            |  }, {});
            |  nativeFunctionMock(JSON.stringify(xhr.response));
            |  nativeFunctionMock(JSON.stringify(headers));
            |}""".trimMargin()
        subject.evaluateNoRetVal(js)

        // THEN
        verify(timeout = 2000, ordering = Ordering.SEQUENCE) {
            jsToNativeFunctionMock(match { responseJson ->
                assertNotNull(responseJson)
                val responseObject = PayloadObject.fromJsonString(responseJson as String)
                assertNotNull(responseObject)
                assertEquals(responseObject.keyCount, 2)
                assertEquals(responseObject.getString("testKey1"), "testValue1")
                assertEquals(responseObject.getString("testKey2"), "testValue2")
                true
            })

            jsToNativeFunctionMock(match { headersJson ->
                val responseHeadersObject = PayloadObject.fromJsonString(headersJson as String)
                assertNotNull(responseHeadersObject)
                assertEquals(responseHeadersObject.keyCount, 2)
                assertEquals(
                    responseHeadersObject.getString("testresponseheaderkey1"),
                    "testResponseHeaderValue1"
                )
                assertEquals(
                    responseHeadersObject.getString("testresponseheaderkey2"),
                    "testResponseHeaderValue2"
                )
                true
            })
        }
        assertTrue(errors.isEmpty())
    }

    @Test
    fun testXmlHttpRequest_abort() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        val url = "https://test.url/api/request"
        val responseText = """{"testKey": "testValue"}"""
        httpInterceptor.mockRequest(url, responseText)

        // WHEN
        val js = """
            var req = new XMLHttpRequest();
            req.open("GET", "$url");
            req.send();
            req.onload = function() {
                nativeFunctionMock("loaded");
            }
            req.onabort = function() {
                nativeFunctionMock("aborted");
            }
            req.abort();
            """
        subject.evaluateNoRetVal(js)

        // THEN
        verify(timeout = 2000) { jsToNativeFunctionMock(eq("aborted"))}
        verify(inverse = true) { jsToNativeFunctionMock(eq("loaded")) }
        assertTrue(errors.isEmpty())
    }


    // JsExpectations
    // ---

    class JsExpectations: JsExpectationsNativeApi {
        val expectations = mutableMapOf<String, JsValue>()
        val isEmpty: Boolean = expectations.isEmpty()

        inline fun <reified T: Any> takeExpectation(name: String): T? {
            val jsValue = expectations.remove(name) ?: return null

            return runBlocking {
                jsValue.evaluate<T>()
            }
        }

        fun checkDoesNotExist(name: String) {
            assertFalse(expectations.contains(name))
        }

        inline fun <reified T: Any> checkEquals(name: String, value: T?) {
            assertEquals(takeExpectation<T>(name), value)
        }

        inline fun <reified T: Any> checkBlock(name: String, block: (T?) -> Unit) {
            val expectedValue = takeExpectation<T>(name)
            block(expectedValue)
        }

        override fun addExpectation(name: String, value: JsValue) {
            expectations[name] = value
        }
    }


    // Private methods
    // ---

    private fun createAndSetUpJsBridge(): JsBridge {
        return JsBridge(InstrumentationRegistry.getInstrumentation().context).also { jsBridge ->
            this@JsBridgeTest.jsBridge = jsBridge

            jsBridge.registerErrorListener(errorListener)
            jsBridge.start(okHttpClient = okHttpClient)

            // Map "jsToNativeFunctionMock" to JS as a global var "NativeFunctionMock"
            JsValue.fromNativeFunction1(jsBridge, jsToNativeFunctionMock).assignToGlobal("nativeFunctionMock")
        }
    }

    private fun handleJsBridgeError(e: JsBridgeError) {
        if (e is UnhandledJsPromiseError) {
            unhandledPromiseErrors.add(e)
        } else {
            errors.add(e)
        }
    }

    @Suppress("UNUSED")  // Only for debugging
    private fun printErrors() {
        if (!errors.isEmpty()) {
            Timber.e("JsBridge errors:")
            errors.forEachIndexed { index, error ->
                Timber.e("Error ${index + 1}/${errors.count()}:\n")
                Timber.e(error)
            }
        }

        if (!unhandledPromiseErrors.isEmpty()) {
            Timber.w("JsBridge unhandled Promise errors:")
            unhandledPromiseErrors.forEachIndexed { index, error ->
                Timber.w("Unhandled Promise error ${index + 1}/${unhandledPromiseErrors.count()}:\n")
                Timber.w(error)
            }
        }
    }

    // Wait until the JS queue is empty
    private suspend fun waitForDone(jsBridge: JsBridge) {
       try {
            withContext(jsBridge.coroutineContext) {
                // ensure that triggered coroutines are processed
                yield()
                yield()
                yield()
            }
        } catch (e: CancellationException) {
            // Ignore cancellation
        }
    }
}
