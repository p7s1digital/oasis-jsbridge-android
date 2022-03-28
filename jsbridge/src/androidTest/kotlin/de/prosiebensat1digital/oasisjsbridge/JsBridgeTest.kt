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

import android.content.Context
import android.util.Log
import androidx.test.platform.app.InstrumentationRegistry
import io.mockk.Ordering
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import kotlin.test.*
import kotlinx.coroutines.*
import okhttp3.OkHttpClient
import org.junit.After
import org.junit.Before
import org.junit.BeforeClass
import org.junit.Test
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.fail
import timber.log.Timber

interface TestNativeApiInterface : JsToNativeInterface {
    fun nativeMethodReturningTrue(): Boolean
    fun nativeMethodReturningFalse(): Boolean
    fun nativeMethodReturningString(): String
    fun nativeMethodReturningJsonObject(): JsonObjectWrapper
    fun nativeMethodReturningResolvedJsonObjectDeferred(): Deferred<JsonObjectWrapper>
    fun nativeMethodReturningRejectedJsonObjectDeferred(): Deferred<JsonObjectWrapper>
    fun nativeMethodReturningResolvedStringDeferred(): Deferred<String>
    fun nativeMethodReturningRejectedStringDeferred(): Deferred<String>
    fun nativeMethodWithBool(b: Boolean?)
    fun nativeMethodWithString(msg: String?)
    fun nativeMethodWithJsonObjects(jsonObject1: JsonObjectWrapper?, jsonObject2: JsonObjectWrapper?)
    fun nativeMethodWithIntVarArg(b: Boolean, vararg ints: Int)
    fun nativeMethodWithStringVarArg(b: Boolean, vararg strings: String)
    fun nativeMethodWithCallback(cb: (string: String, obj: JsonObjectWrapper, bool1: Boolean, bool2: Boolean, int: Int, double: Double, optionalInt1: Int?, optionalInt2: Int?) -> Unit)
    fun nativeMethodThrowingException()
}

interface TestJsApiInterface : NativeToJsInterface {
    fun jsMethodWithString(msg: String)
    fun jsMethodWithJsonObjects(jsonObject1: JsonObjectWrapper, jsonObject2: JsonObjectWrapper)
    fun jsMethodWithJsValue(jsValue: JsValue)
    fun jsMethodWithIntVarArg(b: Boolean, vararg ints: Int)
    fun jsMethodWithStringVarArg(b: Boolean, vararg strings: String)
    fun jsMethodWithCallback(cb: (msg: String, jsonObject1: JsonObjectWrapper, jsonObject2: JsonObjectWrapper, bool1: Boolean, bool2: Boolean, int: Int, double: Double, optionalInt1: Int?, optionalInt2: Int?) -> String)
    fun jsMethodWithUnitCallback(cb: () -> Unit)
    fun jsMethodReturningFulfilledPromise(str: String): Deferred<String>
    fun jsMethodReturningRejectedPromise(d: Double): Deferred<Double>
    fun jsMethodReturningJsValue(str: String): Deferred<JsValue>
}

interface TestJsApiInterfaceWithSuspend : NativeToJsInterface {
    suspend fun jsMethodWithString(msg: String)
    suspend fun jsMethodReturningJsonObject(): JsonObjectWrapper
    suspend fun jsMethodThrowingException(): String
    suspend fun jsMethodReturningFulfilledPromise(): String
    suspend fun jsMethodReturningRejectedPromise(): String
}

interface JsExpectationsNativeApi : JsToNativeInterface {
    fun addExpectation(name: String, value: JsValue)
}

class JsBridgeTest {
    private var jsBridge: JsBridge? = null
    private val context: Context = InstrumentationRegistry.getInstrumentation().context
    private val httpInterceptor = TestHttpInterceptor()
    private val okHttpClient = OkHttpClient.Builder().addInterceptor(httpInterceptor).build()
    private val jsToNativeFunctionMock = mockk<(p: Any) -> Unit>(relaxed = true)
    private lateinit var errors: MutableList<JsBridgeError>
    private lateinit var unhandledPromiseErrors: MutableList<JsBridgeError.UnhandledJsPromiseError>

    companion object {
        const val ITERATION_COUNT = 1000  // for miniBenchmark

        @BeforeClass
        @JvmStatic
        fun setUpClass() {
            Timber.plant(Timber.DebugTree())
        }
    }

    @Before
    fun setUp() {
        errors = mutableListOf()
        unhandledPromiseErrors = mutableListOf()
    }

    @After
    fun cleanUp() {
        jsBridge?.let {
            runBlocking {
                waitForDone(it)
                jsBridge?.release()
                waitForDone(it)
            }
        }

        printErrors()
    }

    @Test
    fun testEvaluateUnsync() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        val js = """nativeFunctionMock("testString");"""
        subject.evaluateUnsync(js)

        runBlocking { waitForDone(subject) }

        // THEN
        assertTrue(errors.isEmpty())
        verify { jsToNativeFunctionMock(eq("testString")) }
    }

    @Test
    fun testEvaluateUnsyncWithError() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        val js = """
            invalid.javaScript.instruction
            """
        subject.evaluateUnsync(js)

        runBlocking { waitForDone(subject) }

        // THEN
        val stringEvaluationError = errors.singleOrNull() as? JsBridgeError.JsStringEvaluationError
        assertNotNull(stringEvaluationError)
        assertEquals(js, stringEvaluationError.js)
    }

    @Test
    fun testEvaluateBlocking() {
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
    fun testEvaluate() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // THEN
        runBlocking {
            assertNull(subject.evaluate("null"))
            assertNull(subject.evaluate("undefined"))

            assertEquals("1.5+1", subject.evaluate("\"1.5+1\""))  // String
            assertEquals(2.toByte(), subject.evaluate("1.5+1"))  // Byte
            assertEquals(2, subject.evaluate("1.5+1"))  // Int
            assertEquals(2L, subject.evaluate("1.5+1"))  // Long
            assertEquals(2.5, subject.evaluate("1.5+1"))  // Double
            assertEquals(2.5f, subject.evaluate("1.5+1"))  // Float

            // Throwing undefined from JS
            val exceptionFromUndefined: JsException = assertFailsWith { subject.evaluate("throw undefined;") }
            assertEquals("undefined", exceptionFromUndefined.message)
            assertEquals("", exceptionFromUndefined.jsonValue)

            // Throwing null from JS
            val exceptionFromNull: JsException = assertFailsWith { subject.evaluate("throw null;") }
            assertEquals("null", exceptionFromNull.message)
            assertEquals("null", exceptionFromNull.jsonValue)

            // Throwing string from JS
            val exceptionFromString: JsException = assertFailsWith { subject.evaluate("""throw "Error string";""") }
            assertEquals("Error string", exceptionFromString.message)
            assertEquals("\"Error string\"", exceptionFromString.jsonValue)

            // Throwing Error from JS
            val exceptionFromError: JsException = assertFailsWith { subject.evaluate("""throw new Error("Error message");""") }
            assertEquals(true, exceptionFromError.message?.contains("Error message"))
            assertEquals("""{message: "Error message"}""".toPayload(), exceptionFromError.jsonValue?.toPayload())

            // Throwing Object from JS
            val exceptionFromObject: JsException = assertFailsWith { subject.evaluate("""throw {message: "Error object", hint: "Just a test"};""") }
            assertEquals("""{message: "Error object", hint: "Just a test"}""".toPayload(), exceptionFromObject.jsonValue?.toPayload())

            // Pending promise
            val resolveFuncJsValue = JsValue(subject)
            val jsPromise: Deferred<Int> = subject.evaluate("new Promise(function(resolve) { $resolveFuncJsValue = resolve; });")
            assertTrue(jsPromise.isActive)

            // Resolve pending promise
            subject.evaluate<Unit>("$resolveFuncJsValue(69);")
            assertEquals(69, jsPromise.await())

            // Resolved promise
            assertEquals(123, subject.evaluate("new Promise(function(resolve) { resolve(123); });"))
            assertEquals(123, subject.evaluate<Deferred<Int>>("new Promise(function(resolve) { resolve(123); });").await())

            // Rejected promise
            val jsPromiseException: JsException = assertFailsWith {
                subject.evaluate<Int>("new Promise(function(resolve, reject) { reject(new Error('JS test error')); });")
            }
            val jsPromiseErrorPayload = jsPromiseException.jsonValue?.toPayloadObject()
            assertEquals("JS test error", jsPromiseErrorPayload?.getString("message"))

            // Array
            assertArrayEquals(arrayOf("a", "b", "c"), subject.evaluate<Array<String>>("""["a", "b", "c"]"""))  // Array<String>
            assertArrayEquals(booleanArrayOf(true, false, true), subject.evaluate("""[true, false, true]"""))  // BooleanArray
            assertArrayEquals(arrayOf(true, false, true), subject.evaluate<Array<Boolean>>("""[true, false, true]"""))  // Array<Boolean>
            assertArrayEquals(byteArrayOf(1, 2, 3), subject.evaluate("""[1.0, 2.2, 3.8]"""))  // ByteArray
            assertArrayEquals(arrayOf(1.toByte(), 2.toByte(), 3.toByte()), subject.evaluate<Array<Byte>>("""[1.0, 2.2, 3.8]"""))  // Array<Byte>
            assertArrayEquals(intArrayOf(1, 2, 3), subject.evaluate("""[1.0, 2.2, 3.8]"""))  // IntArray
            assertArrayEquals(arrayOf(1, 2, 3), subject.evaluate<Array<Int>>("""[1.0, 2.2, 3.8]"""))  // Array<Int>
            assertArrayEquals(longArrayOf(1L, 2L, 3L), subject.evaluate("""[1.0, 2.2, 3.8]"""))  // LongArray
            assertArrayEquals(arrayOf(1L, 2L, 3L), subject.evaluate<Array<Long>>("""[1.0, 2.2, 3.8]"""))  // Array<Long>
            assertTrue(subject.evaluate<DoubleArray>("""[1.0, 2.2, 3.8]""").contentEquals(doubleArrayOf(1.0, 2.2, 3.8)))  // DoubleArray
            assertArrayEquals(arrayOf(1.0, 2.2, 3.8), subject.evaluate<Array<Double>>("""[1.0, 2.2, 3.8]"""))  // Array<Double>
            assertTrue(subject.evaluate<FloatArray>("""[1.0, 2.2, 3.8]""").contentEquals(floatArrayOf(1.0f, 2.2f, 3.8f)))  // FloatArray
            assertArrayEquals(arrayOf(1.0f, 2.2f, 3.8f), subject.evaluate<Array<Float>>("""[1.0, 2.2, 3.8]"""))  // Array<Float>

            // 2D-Arrays
            assertArrayEquals(arrayOf(arrayOf(1, 2), arrayOf(3, 4)), subject.evaluate<Array<Array<Int>>>("""[[1, 2], [3, 4]]"""))  // Array<Array<Int>>
            assertArrayEquals(arrayOf(arrayOf(1, 2), arrayOf(null, 4)), subject.evaluate<Array<Array<Int?>>>("""[[1, 2], [null, 4]]"""))  // Array<Array<Int?>>
            assertArrayEquals(arrayOf(intArrayOf(1, 2), intArrayOf(3, 4)), subject.evaluate<Array<IntArray>>("""[[1, 2], [3, 4]]"""))  // Array<IntArray>>

            // Array with optionals
            assertArrayEquals(arrayOf("a", null, "c"), subject.evaluate<Array<String?>>("""["a", null, "c"]"""))  // Array<String?>
            assertArrayEquals(arrayOf(false, null, true), subject.evaluate<Array<Boolean?>>("""[false, null, true]"""))  // Array<Boolean?>
            assertArrayEquals(arrayOf(1.toByte(), null, 3.toByte()), subject.evaluate<Array<Byte?>>("""[1.0, null, 3.8]"""))  // Array<Byte?>
            assertArrayEquals(arrayOf(1, null, 3), subject.evaluate<Array<Int?>>("""[1.0, null, 3.8]"""))  // Array<Int?>
            assertArrayEquals(arrayOf(1.0, null, 3.8), subject.evaluate<Array<Double?>>("""[1.0, null, 3.8]"""))  // Array<Double?>
            assertArrayEquals(arrayOf(1.0f, null, 3.8f), subject.evaluate<Array<Float?>>("""[1.0, null, 3.8]"""))  // Array<Float?>

            // Array of (any) objects
            assertArrayEquals(arrayOf(1.0, "hello", null), subject.evaluate<Array<Any>>("""[1.0, "hello", null]"""))  // Array<Any?>

            // JSON
            assertEquals(
                JsonObjectWrapper("key1" to 1, "key2" to "value2").toPayload(),
                subject.evaluate<JsonObjectWrapper>("({key1: 1, key2: \"value2\"})").toPayload()
            )
        }
    }

    @Test
    fun testEvaluateWithError() {
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
        assertEquals(true, jsException1.message?.contains("invalid"))
        assertEquals(jsException2.message, jsException1.message)
        assertTrue(errors.isEmpty())
    }

    @Test
    fun testEvaluateLocalFile() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        runBlocking {
            // androidTestDuktape asset file "test.js"
            // - content:
            // nativeFunctionMock("localFileString");
            subject.evaluateLocalFile(context, "js/test.js")
        }

        // THEN
        assertTrue(errors.isEmpty())
        verify { jsToNativeFunctionMock(eq("localFileString")) }
    }

    @Test
    fun testEvaluateLocalFileNonExisting() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        val t: Throwable? = runBlocking {
            try {
                subject.evaluateLocalFile(context, "non-existing/file.js")
                null
            } catch (t: Throwable) {
                t
            }
        }

        // THEN
        assertEquals(0, errors.size)
        assertTrue(t is JsBridgeError.JsFileEvaluationError)
        assertEquals("non-existing/file.js", t.fileName)
    }

    @Test
    fun testEvaluateLocalFileUnsyncNonExisting() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        subject.evaluateLocalFileUnsync(context, "non-existing/file.js")

        runBlocking {
            waitForDone(subject)
        }

        // THEN
        val fileEvaluationError = errors.firstOrNull() as? JsBridgeError.JsFileEvaluationError
        assertNotNull(fileEvaluationError)
        assertEquals("non-existing/file.js", fileEvaluationError.fileName)
    }

    @Test
    fun testEvaluateLocalFileWithJsError() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        val t: Throwable? = runBlocking {
            try {
                subject.evaluateLocalFile(context, "js/test_with_error.js")
                null
            } catch (t: Throwable) {
                t
            }
        }

        // THEN
        assertEquals(0, errors.size)
        assertTrue(t is JsBridgeError.JsFileEvaluationError)
        val jsException = t.jsException
        assertNotNull(jsException)
        assertTrue(jsException.stackTrace.isNotEmpty())
        jsException.stackTrace[0].let { e ->
            assertEquals("test_with_error.js", e.fileName)
            assertEquals(1, e.lineNumber)
        }
    }

    @Test
    fun testEvaluateLocalFileUnsyncWithJsError() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        subject.evaluateLocalFileUnsync(context, "js/test_with_error.js")

        runBlocking {
            waitForDone(subject)
        }

        // THEN
        val jsException = errors.firstOrNull()?.jsException
        assertNotNull(jsException)
        assertTrue(jsException.stackTrace.isNotEmpty())
        jsException.stackTrace[0].let { e ->
            assertEquals("test_with_error.js", e.fileName)
            assertEquals(1, e.lineNumber)
        }
    }

    @Test
    fun testEvaluateFileContent() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        val content = """nativeFunctionMock("fileContentString");"""

        runBlocking {
            subject.evaluateFileContent(content, "file.js")
        }

        // THEN
        assertTrue(errors.isEmpty())
        verify { jsToNativeFunctionMock(eq("fileContentString")) }
    }

    @Test
    fun testEvaluateFileContentAsModule() {
        if (BuildConfig.FLAVOR == "duktape") {
            // ES6 modules are not supported on Duktape
            return
        }

        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        val jsModule1 = """
            export function helperFunction() { return "testString" };
        """.trimIndent()
        val jsModule2 = """
            import * as module1 from "module1.js"
            globalThis.mainFunction = function() {
                return module1.helperFunction();
            }
        """.trimIndent()

        val ret = runBlocking {
            subject.evaluateFileContent(jsModule1, "module1.js", JsBridge.JsFileEvaluationType.Module)
            subject.evaluateFileContent(jsModule2, "module2.js", JsBridge.JsFileEvaluationType.Module)
            subject.evaluate<String>("globalThis.mainFunction()");
        }

        // THEN
        assertTrue(errors.isEmpty())
        assertEquals("testString", ret)
    }

    @Test
    fun testEvaluateFileContentUnsync() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        val content = """nativeFunctionMock("fileContentString");"""
        subject.evaluateFileContentUnsync(content, "file.js")

        runBlocking {
            waitForDone(subject)
        }

        // THEN
        assertTrue(errors.isEmpty())
        verify { jsToNativeFunctionMock(eq("fileContentString")) }
    }

    @Test
    fun testEvaluateFileContentWithError() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        val t: Throwable? = runBlocking {
            try {
                subject.evaluateFileContent("this will obviously fail", "file.js")
                null
            } catch (t: Throwable) {
                t
            }
        }

        // THEN
        assertEquals(0, errors.size)
        assertTrue(t is JsBridgeError.JsFileEvaluationError)
        val jsException = t.jsException
        assertNotNull(jsException)
        assertTrue(jsException.stackTrace.isNotEmpty())
        jsException.stackTrace[0].let { e ->
            assertEquals("file.js", e.fileName)
            assertEquals(1, e.lineNumber)
        }
    }

    @Test
    fun testEvaluateFileContentUnsyncWithError() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        subject.evaluateFileContentUnsync("this will obviously fail", "file.js")

        runBlocking {
            waitForDone(subject)
        }

        // THEN
        val jsException = errors.firstOrNull()?.jsException
        assertNotNull(jsException)
        assertTrue(jsException.stackTrace.isNotEmpty())
        jsException.stackTrace[0].let { e ->
            assertEquals("file.js", e.fileName)
            assertEquals(1, e.lineNumber)
        }
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

            assertEquals(evaluatedStrValue, strValue.evaluateBlocking())
            assertEquals(evaluatedIntValue, intValue.evaluateBlocking())

            assertEquals("123", strValue.evaluateBlocking())
            assertEquals(123, intValue.evaluateBlocking())

            assertFailsWith<JsException> {
                errorValue.evaluateBlocking<Unit>()
            }
        }

        // THEN (with suspending methods)
        runBlocking {
            val evaluatedStrValue: String = subject.evaluate("$strValue")
            val evaluatedIntValue = subject.evaluate<Int>("$intValue")

            assertEquals(evaluatedStrValue, strValue.evaluate())
            assertEquals(evaluatedIntValue, intValue.evaluate())

            assertEquals("123", strValue.evaluate())
            assertEquals(123, intValue.evaluate())
        }

        // THEN (with async methods)
        val evaluatedStrValueAsync = subject.evaluateAsync<String>("$strValue")
        val evaluatedIntValueAsync = subject.evaluateAsync<Int>("$intValue")

        runBlocking {
            assertEquals(evaluatedStrValueAsync.await(), strValue.evaluateAsync<String>().await())
            assertEquals(evaluatedIntValueAsync.await(), intValue.evaluateAsync<Int>().await())

            assertEquals("123", strValue.evaluateAsync<String>().await())
            assertEquals(123, intValue.evaluateAsync<Int>().await())
        }

        // THEN (evaluate promise)
        runBlocking {
            val promiseValue: Int = resolvedPromiseValue.evaluate()
            assertEquals(123, promiseValue)

            assertEquals(123, resolvedPromiseAwaitValue.await().evaluate())

            val awaitError = assertFailsWith<JsException> {
                rejectedPromiseAwaitValue.await().evaluate()
            }
            assertEquals("wrong", awaitError.jsonValue?.toPayload()?.stringValue())
        }

        // THEN (promise await)
        runBlocking {
            val error = assertFailsWith<JsException> {
                rejectedPromiseValue.evaluate()
            }
            val awaitError = assertFailsWith<JsException> {
                rejectedPromiseValue.await()
            }
            assertEquals("wrong", error.jsonValue?.toPayload()?.stringValue())
            assertEquals("wrong", awaitError.jsonValue?.toPayload()?.stringValue())
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
            assertEquals("null", nonNullableJsonObjectWrapperNull.jsonString)
            assertEquals(JsonObjectWrapper.Undefined, nonNullableJsonObjectWrapperUndefined)

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
        runBlocking {
            subject.evaluate<Unit>("""
                var promise = new Promise(function(resolve, reject) {
                  throw Error("Test unhandled promise rejection");
                });""".trimIndent()
            )

            waitForDone(subject)
        }

        // THEN
        val unhandledJsPromiseError = unhandledPromiseErrors.firstOrNull() as? JsBridgeError.UnhandledJsPromiseError
        assertNotNull(unhandledJsPromiseError?.jsException)
        assertEquals("Test unhandled promise rejection", unhandledJsPromiseError?.jsException?.message)
        assertEquals("Test unhandled promise rejection", unhandledJsPromiseError?.jsException?.jsonValue?.toPayloadObject()?.getString("message"))
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
            assertEquals("JavaScript", e.className)
            assertEquals("testFunction1", e.methodName)
            assertEquals("eval", e.fileName)
            assertEquals(2, e.lineNumber)
        }
        jsException.stackTrace[1].let { e ->
            assertEquals("JavaScript", e.className)
            assertEquals("testFunction2", e.methodName)
            assertEquals("eval", e.fileName)
            assertEquals(6, e.lineNumber)
        }
        jsException.stackTrace[2].let { e ->
            assertEquals("JavaScript", e?.className)
            assertEquals("testFunction3", e?.methodName)
            assertEquals("eval", e?.fileName)
            assertEquals(10, e?.lineNumber)
        }
        jsException.stackTrace[3].let { e ->
            assertEquals("JavaScript", e.className)
            assertEquals(true, """^<?eval>?$""".toRegex().matches(e.methodName))
            assertEquals("eval", e?.fileName)
            assertEquals(13, e?.lineNumber)
        }
        jsException.stackTrace[4].let { e ->
            assert(e.className.endsWith(".JsBridge"))
            assertEquals("jniEvaluateString", e.methodName)
            assertEquals("JsBridge.kt", e.fileName)
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
            assertEquals("JavaScript", e.className)
            assertEquals("testFunction", e.methodName)
            assertEquals("eval", e.fileName)
            assertEquals(2, e.lineNumber)
        }
        jsException.stackTrace[1].let { e ->
            assertEquals("JavaScript", e.className)
            assertEquals(true, """^<?eval>?$""".toRegex().matches(e.methodName))
            assertEquals("eval", e?.fileName)
            assertEquals(5, e?.lineNumber)
        }
        jsException.stackTrace[2].let { e ->
            assert(e.className.endsWith(".JsBridge"))
            assertEquals("jniEvaluateString", e.methodName)
            assertEquals("JsBridge.kt", e.fileName)
        }
        assert(jsException.cause is java.lang.Exception)
        assertEquals("Kotlin exception", jsException.cause?.message)
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
            subject.evaluateBlocking<ByteArray>("new Array($arrayLength);")
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
    fun testFromNativeValueAndBack() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // THEN
        runBlocking {
            val jsTrue = JsValue.fromNativeValue(subject, true)
            val jsFalse = JsValue.fromNativeValue(subject, false)
            assertEquals(true, jsTrue.evaluate())
            assertEquals(false, jsFalse.evaluate())

            val jsString = JsValue.fromNativeValue(subject, "nativeString")
            assertEquals("nativeString", jsString.evaluate())

            val jsByte = JsValue.fromNativeValue(subject, 5.toByte())
            val jsNullableByte = JsValue.fromNativeValue<Byte?>(subject, 5)
            val jsNullByte = JsValue.fromNativeValue<Byte?>(subject, null)
            assertEquals(5.toByte(), jsByte.evaluate())
            assertEquals(5.toByte(), jsNullableByte.evaluate())
            assertEquals(null, jsNullByte.evaluate<Byte?>())

            val jsInt = JsValue.fromNativeValue(subject, 123)
            val jsNullableInt = JsValue.fromNativeValue<Int?>(subject, 123)
            val jsNullInt = JsValue.fromNativeValue<Int?>(subject, null)
            assertEquals(123, jsInt.evaluate())
            assertEquals(123, jsNullableInt.evaluate())
            assertEquals(null, jsNullInt.evaluate<Int?>())

            val jsFloat = JsValue.fromNativeValue(subject, 123.456f)
            val jsNullableFloat = JsValue.fromNativeValue<Float?>(subject, 123.456f)
            val jsNullFloat = JsValue.fromNativeValue<Float?>(subject, null)
            assertEquals(123.456f, jsFloat.evaluate())
            assertEquals(123.456f, jsNullableFloat.evaluate())
            assertEquals(null, jsNullFloat.evaluate<Float?>())

            val jsDouble = JsValue.fromNativeValue(subject, 123.456)
            val jsNullableDouble = JsValue.fromNativeValue<Double?>(subject, 123.456)
            val jsNullDouble = JsValue.fromNativeValue<Double?>(subject, null)
            assertEquals(123.456, jsDouble.evaluate())
            assertEquals(123.456, jsNullableDouble.evaluate())
            assertEquals(null, jsNullDouble.evaluate<Double?>())

            val jsArrayBoolean = JsValue.fromNativeValue(subject, booleanArrayOf(true, true, false))
            val jsArrayNullableBoolean = JsValue.fromNativeValue(subject, arrayOf(true, null, false))
            assertArrayEquals(booleanArrayOf(true, true, false), jsArrayBoolean.evaluate())
            assertArrayEquals(arrayOf(true, null, false), jsArrayNullableBoolean.evaluate())

            val jsArrayByte = JsValue.fromNativeValue(subject, byteArrayOf(1, 2, 3))
            val jsArrayNullableByte = JsValue.fromNativeValue(subject, arrayOf(1.toByte(), null, 3.toByte()))
            assertArrayEquals(byteArrayOf(1, 2, 3), jsArrayByte.evaluate())
            assertArrayEquals(arrayOf(1.toByte(), null, 3.toByte()), jsArrayNullableByte.evaluate<Array<Byte?>>())

            val jsArrayInt = JsValue.fromNativeValue(subject, intArrayOf(1, 2, 3))
            val jsArrayNullableInt = JsValue.fromNativeValue(subject, arrayOf(1, null, 3))
            assertArrayEquals(intArrayOf(1, 2, 3), jsArrayInt.evaluate())
            assertArrayEquals(arrayOf(1, null, 3), jsArrayNullableInt.evaluate<Array<Int?>>())

            val jsArrayLong = JsValue.fromNativeValue(subject, longArrayOf(1L, 2L, 3L))
            val jsArrayNullableLong = JsValue.fromNativeValue(subject, arrayOf(1L, null, 3L))
            assertArrayEquals(longArrayOf(1L, 2L, 3L), jsArrayLong.evaluate())
            assertArrayEquals(arrayOf(1L, null, 3L), jsArrayNullableLong.evaluate<Array<Long?>>())

            val jsArrayFloat = JsValue.fromNativeValue(subject, floatArrayOf(1f, 2f, 3f))
            val jsArrayNullableFloat = JsValue.fromNativeValue(subject, arrayOf(1f, null, 3f))
            assertTrue(jsArrayFloat.evaluate<FloatArray>().contentEquals(floatArrayOf(1f, 2f, 3f)))
            assertArrayEquals(arrayOf(1f, null, 3f), jsArrayNullableFloat.evaluate<Array<Float?>>())

            val jsArrayDouble = JsValue.fromNativeValue(subject, doubleArrayOf(1.0, 2.0, 3.0))
            val jsArrayNullableDouble = JsValue.fromNativeValue(subject, arrayOf(1.0, null, 3.0))
            assertTrue(jsArrayDouble.evaluate<DoubleArray>().contentEquals(doubleArrayOf(1.0, 2.0, 3.0)))
            assertArrayEquals(arrayOf(1.0, null, 3.0), jsArrayNullableDouble.evaluate<Array<Double?>>())

            val jsArrayString = JsValue.fromNativeValue(subject, arrayOf("a", "b", "c"))
            val jsArrayNullableString = JsValue.fromNativeValue(subject, arrayOf("a", null, "c"))
            assertArrayEquals(arrayOf("a", "b", "c"), jsArrayString.evaluate())
            assertArrayEquals(arrayOf("a", null, "c"), jsArrayNullableString.evaluate())
        }
    }

    @Test
    fun testConversionErrors() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // THEN
        runBlocking {
            assertFailsWith<IllegalArgumentException> { subject.evaluate<BooleanArray>("1") }
            assertFailsWith<IllegalArgumentException> { subject.evaluate<IntArray>("1") }
            assertFailsWith<IllegalArgumentException> { subject.evaluate<LongArray>("1") }
            assertFailsWith<IllegalArgumentException> { subject.evaluate<FloatArray>("1") }
            assertFailsWith<IllegalArgumentException> { subject.evaluate<DoubleArray>("1") }
            assertFailsWith<IllegalArgumentException> { subject.evaluate<Array<Any>>("1") }

            assertFailsWith<IllegalArgumentException> {
                subject.evaluate<BooleanArray>("[true, false, 'patate']")
            }
            assertFailsWith<IllegalArgumentException> {
                subject.evaluate<ByteArray>("[1, 2, 'patate']")
            }
            assertFailsWith<IllegalArgumentException> {
                subject.evaluate<IntArray>("[1, 2, 'patate']")
            }
            assertFailsWith<IllegalArgumentException> {
                subject.evaluate<LongArray>("[1, 2, 'patate']")
            }
            assertFailsWith<IllegalArgumentException> {
                subject.evaluate<FloatArray>("[1.0, 2.0, 'patate']")
            }
            assertFailsWith<IllegalArgumentException> {
                subject.evaluate<DoubleArray>("[1.0, 2.0, 'patate']")
            }
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
            assertEquals(expectedResult, result)

            Timber.i("Executing calcMagicCallJsFunctionInsideNativeLoop()...")
            result = calcMagicCallJsFunctionInsideNativeLoop()
            Timber.i("-> result is $result")
            assertEquals(expectedResult, result)

            Timber.i("Executing calcMagicCallJsFunctionInsideNativeLoop() in JS thread...")
            withContext(subject.coroutineContext) {
                result = calcMagicCallJsFunctionInsideNativeLoop()
                Timber.i("-> result is $result")
                assertEquals(expectedResult, result)
            }

            Timber.i("Executing calcMagicEvaluateJsInsideNativeLoop()...")
            result = calcMagicEvaluateJsInsideNativeLoop()
            Timber.i("-> result is $result")
            assertEquals(expectedResult, result)

            Timber.i("Executing calcMagicEvaluateJsInsideNativeLoop() in JS thread...")
            withContext(subject.coroutineContext) {
                result = calcMagicEvaluateJsInsideNativeLoop()
                Timber.i("-> result is $result")
                assertEquals(expectedResult, result)
            }

            Timber.i("Executing calcMagicCallNativeFunctionInsideJsLoop()...")
            result = calcMagicCallNativeFunctionInsideJsLoop()
            Timber.i("-> result is $result")
            assertEquals(expectedResult, result)

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

    @Test
    @Ignore("Very long test")
    fun stressTest() {
        for (i in 0..10000) {
            Timber.i("stressTest() - JsBridge instance ${i + 1}")
            stressTestHelper()
        }
    }

    private fun stressTestHelper() {
        val config = JsBridgeConfig.standardConfig().apply {
            xhrConfig.okHttpClient = okHttpClient
        }
        val subject = JsBridge(config)

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
            assertEquals("ok", jsApi.helloAsync().await())
        }

        runBlocking {
            // Note: mockk verify with timeout has some issues on API < 24
            if (android.os.Build.VERSION.SDK_INT >= 24) {
                verify(exactly = 100, timeout = 15000L) { nativeCbMock(any()) }
            }
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
            |  $jsExpectationsJsValue.addExpectation("ex" + i, i);
            |}
            |$nativeApiJsValue.registerCallback(jsCb);
            |$nativeApiJsValue.start();""".trimMargin())

        assertEquals("ok", subject.evaluateBlocking("$nativeApiJsValue.helloAsync()"))

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
        assertEquals(10, calcSumBlocking(3, 7))

        runBlocking {
            // Missing JS function (the evalution of the JS code should throw)
            assertFailsWith<JsException> {
                JsValue(subject, "non_existing_function").mapToNativeFunction0<Unit>(true)
            }

            // Invalid JS function (the registration should throw)
            assertFailsWith<IllegalArgumentException> {
                JsValue(subject, "123").mapToNativeFunction0<Unit>(true)
            }

            // Call JS function calcSum(3, 4)
            val sum = calcSum(3, 4)
            assertEquals(7, sum)

            // Call JS function calcSumFromAnonymousFunction(3, 4)
            val sumFromAnonymousFunction = calcSumFromAnonymousFunction(3, 4)
            assertEquals(7, sumFromAnonymousFunction)

            // Call JS function calcSumWrongSignature(3, "four")
            assertFailsWith<IllegalArgumentException> {
                calcSumWrongSignature(3, "four")
            }

            // Call JS function invalidFunction()
            val invalidFunctionException: JsException = assertFailsWith {
                invalidFunction()
            }
            assertEquals(true, invalidFunctionException.message?.contains("invalid"))

            // Call JS function createJsObject(69, "sixty-nine")
            val jsObject = createJsObject(69, "sixty-nine")
            assertEquals(PayloadObject.fromValues("key1" to 69, "key2" to "sixty-nine"), jsObject.toPayload())

            // Call JS function createCalcSumFunc()(2, 2)
            val calcSum2: suspend (Int, Int) -> Int = createCalcSumFunc().mapToNativeFunction2()
            val sum2 = calcSum2(2, 2)
            assertEquals(4, sum2)

            // Call JS function throwException()
            val jsException: JsException = assertFailsWith {
                throwException()
            }
            assertEquals("JS exception from function", jsException.message)

            // Call JS function getPromise("testPromise")
            val promiseResult = getPromise("testPromise")
            assertEquals("Hello testPromise!", promiseResult)

            // Call JS function getPromiseAsync("testPromise")
            val promiseResultDeferred = getPromiseAsync("testPromise")
            assertEquals("Hello testPromise!", promiseResultDeferred.await())

            // Call JS function getFailedPromise("testPromise")
            val promiseException: JsException = assertFailsWith {
                getFailedPromise("testPromise")
            }
            val promiseErrorPayload = promiseException.jsonValue?.toPayload()
            assertEquals("Oh no!", promiseErrorPayload?.stringValue())
        }

        assertTrue(errors.isEmpty())
    }

    @Test
    fun testMapNativeFunctionToJs() {
        // GIVEN
        val subject = createAndSetUpJsBridge()
        var flag = false

        runBlocking {
            // WHEN
            val setFlag = JsValue.fromNativeFunction0(subject) { flag = true }
            val toUpperCaseNative =
                JsValue.fromNativeFunction1(subject) { s: String -> s.toUpperCase() }
            val calcSumNative = JsValue.fromNativeFunction2(subject) { a: Int, b: Int -> a + b }
            val setCustomTimeout: (() -> Unit, Long) -> Unit = { cb, msecs ->
                GlobalScope.launch(Dispatchers.Main) {
                    delay(msecs)
                    cb()
                }
            }
            JsValue.fromNativeFunction2(subject, setCustomTimeout)
                .assignToGlobal("setCustomTimeout")

            subject.evaluateBlocking<Unit>("$setFlag()")
            assertTrue(flag)

            assertEquals(
                "TEST STRING",
                subject.evaluateBlocking("""$toUpperCaseNative("test string")""")
            )
            assertEquals(15, subject.evaluateBlocking("$calcSumNative(7, 8)"))

            subject.evaluate<Unit>(
                """
                |setCustomTimeout(function() {
                |  nativeFunctionMock(true);
                |}, 200);
            """.trimMargin()
            )
            // Note: mockk verify with timeout has some issues on API < 24
            if (android.os.Build.VERSION.SDK_INT >= 24) {
                verify(timeout = 3000L) { jsToNativeFunctionMock(eq(true)) }
            }

            // AND WHEN
            // Missing parameter (replaced with null)
            val missingParameterException = assertFailsWith<JsException> {
                subject.evaluate<Unit>("$calcSumNative(2)")
            }
            assertTrue(missingParameterException.cause is NullPointerException)

            // Too many parameters
            assertFailsWith<JsException> {
                subject.evaluate<Unit>("$calcSumNative(2, 3, 4)")
            }

            calcSumNative.hold()
        }
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
        var receivedIntVarArg: IntArray? = null
        var receivedStringVarArg: Array<out String>? = null

        val nativeApi = object : TestNativeApiInterface {
            override fun nativeMethodReturningTrue() = true
            override fun nativeMethodReturningFalse() = false
            override fun nativeMethodReturningString() = "Hello JsBridgeTest!"

            override fun nativeMethodReturningJsonObject() = JsonObjectWrapper("""
                {"returnKey1": 1, "returnKey2": "returnValue2"}
            """)

            override fun nativeMethodReturningResolvedJsonObjectDeferred() = GlobalScope.async {
                JsonObjectWrapper("""
                    {"returnKey1": 1, "returnKey2": "returnValue2"}
                """
                )
            }

            override fun nativeMethodReturningRejectedJsonObjectDeferred() = GlobalScope.async {
                throw Exception("returned deferred object error")
            }

            override fun nativeMethodReturningResolvedStringDeferred(): Deferred<String> = GlobalScope.async {
                "returned deferred string"
            }

            override fun nativeMethodReturningRejectedStringDeferred(): Deferred<String> = GlobalScope.async {
                throw Exception("returned deferred string error")
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

            override fun nativeMethodWithIntVarArg(b: Boolean, vararg ints: Int) {
                receivedIntVarArg = ints
            }

            override fun nativeMethodWithStringVarArg(b: Boolean, vararg strings: String) {
                receivedStringVarArg = strings.clone()
            }

            override fun nativeMethodWithCallback(cb: (string: String, obj: JsonObjectWrapper, bool1: Boolean, bool2: Boolean, int: Int, double: Double, optionalInt1: Int?, optionalInt2: Int?) -> Unit) {
                cb("cbString", JsonObjectWrapper("""{"cbKey1": 1, "cbKey2": "cbValue2"}"""), true, false, 69, 16.64, 123, null)
            }

            override fun nativeMethodThrowingException() {
                throw Exception("Test native exception")
            }
        }

        val config = JsBridgeConfig.standardConfig().apply {
            xhrConfig.okHttpClient = okHttpClient
        }
        val subject = JsBridge(config)

        val jsExpectations = JsExpectations()
        val jsExpectationsJsValue = JsValue.fromNativeObject(subject, jsExpectations)

        // WHEN
        val testNativeApi = JsValue.fromNativeObject(subject, nativeApi)
        val js = """
            var retBool1 = $testNativeApi.nativeMethodReturningTrue();
            var retBool2 = $testNativeApi.nativeMethodReturningFalse();
            var retString = $testNativeApi.nativeMethodReturningString();
            var retObject = $testNativeApi.nativeMethodReturningJsonObject();
            var retObjectPromise = $testNativeApi.nativeMethodReturningResolvedJsonObjectDeferred();
            retObjectPromise.then(function(deferredValue) {
              $jsExpectationsJsValue.addExpectation("resolvedDeferredJson", JSON.stringify(deferredValue));
            }).catch(function(e) {
              $jsExpectationsJsValue.addExpectation("unexpected", true);
            });
            retObjectPromise = $testNativeApi.nativeMethodReturningRejectedJsonObjectDeferred();
            retObjectPromise.then(function(deferredValue) {
              $jsExpectationsJsValue.addExpectation("unexpected", true);
            }).catch(function(e) {
              $jsExpectationsJsValue.addExpectation("rejectedDeferredJsonError", e.message);
            });
            var retStringPromise = $testNativeApi.nativeMethodReturningResolvedStringDeferred();
            retStringPromise.then(function(deferredValue) {
              $jsExpectationsJsValue.addExpectation("resolvedDeferredString", deferredValue);
            }).catch(function(e) {
              $jsExpectationsJsValue.addExpectation("unexpected", true);
            });
            retStringPromise = $testNativeApi.nativeMethodReturningRejectedStringDeferred();
            retStringPromise.then(function(deferredValue) {
              $jsExpectationsJsValue.addExpectation("unexpected", true);
            }).catch(function(e) {
              $jsExpectationsJsValue.addExpectation("rejectedDeferredStringError", e.message);
            });
            $testNativeApi.nativeMethodWithBool(retBool1);
            $testNativeApi.nativeMethodWithBool(retBool2);
            $testNativeApi.nativeMethodWithString(retString);
            $testNativeApi.nativeMethodWithJsonObjects(retObject, [1, "two", 3]);
            $testNativeApi.nativeMethodWithIntVarArg(false, 1, 2, 3, 4, 5);
            $testNativeApi.nativeMethodWithStringVarArg(true, "vararg1", "vararg2", "vararg3");
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
        subject.evaluateUnsync(js)

        runBlocking { waitForDone(subject) }

        // THEN
        assertEquals(true, receivedBool1)
        assertEquals(false, receivedBool2)
        assertEquals("Hello JsBridgeTest!", receivedString)
        assertEquals("""{"returnKey1":1,"returnKey2":"returnValue2"}""", receivedJsonObject1?.jsonString)
        assertEquals("""[1,"two",3]""", receivedJsonObject2?.jsonString)
        assertArrayEquals(intArrayOf(1, 2, 3, 4, 5), receivedIntVarArg)
        assertArrayEquals(arrayOf("vararg1", "vararg2", "vararg3"), receivedStringVarArg)

        runBlocking { waitForDone(subject) }

        jsExpectations.checkDoesNotExist("unexpected")
        jsExpectations.checkEquals("resolvedDeferredJson", """{"returnKey1":1,"returnKey2":"returnValue2"}""")
        jsExpectations.checkEquals("rejectedDeferredJsonError", "returned deferred object error")
        jsExpectations.checkEquals("resolvedDeferredString", "returned deferred string")
        jsExpectations.checkEquals("rejectedDeferredStringError", "returned deferred string error")
        jsExpectations.checkEquals("cbString", "cbString")
        jsExpectations.checkBlock("cbObj") { value: JsonObjectWrapper? ->
            assertEquals("""{"cbKey1": 1, "cbKey2": "cbValue2"}""".toPayload(), value?.toPayload())
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
    fun testRegisterNativeToJsInterfaceWithSuspendMethods() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        val jsExpectations = JsExpectations()
        val jsExpectationsJsValue = JsValue.fromNativeObject(subject, jsExpectations)

        val testJsApi = JsValue(subject, """({
            jsMethodWithString: function(msg) {
              $jsExpectationsJsValue.addExpectation("stringArg", msg);
            },
            jsMethodReturningJsonObject: function() {
              return { key: "value" };
            },
            jsMethodThrowingException: function() {
              throw new Error("js exception");
            },
            jsMethodReturningFulfilledPromise: function() {
              return new Promise(function(resolve) {
                resolve("fulfilled promise");
              });
            },
            jsMethodReturningRejectedPromise: function() {
              return new Promise(function(resolve, reject) {
                reject(new Error("rejected promise"));
              });
            }
        })""")


        // WHEN
        val jsApi: TestJsApiInterfaceWithSuspend = testJsApi.mapToNativeObject()
        runBlocking {
            jsApi.jsMethodWithString("Hello JS!")

            val result = jsApi.jsMethodReturningJsonObject()
            assertEquals("value", result.toPayloadObject()?.getString("key"))

            val exception = assertFailsWith<JsException> {
                jsApi.jsMethodThrowingException()
            }
            assertEquals(true, exception.message?.contains("js exception"))

            val promiseResult = jsApi.jsMethodReturningFulfilledPromise()
            assertEquals("fulfilled promise", promiseResult)

            val promiseException = assertFailsWith<JsException> {
                jsApi.jsMethodReturningRejectedPromise()
            }
            assertEquals(true, promiseException.message?.contains("rejected promise"))

            waitForDone(subject)
        }

        // THEN
        jsExpectations.checkEquals("stringArg", "Hello JS!")
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
            jsMethodWithIntVarArg: function(b/*, ...ints*/) {
              var intArray = Array.prototype.slice.call(arguments, 1);
              $jsExpectationsJsValue.addExpectation("intVarArg", intArray);
            },
            jsMethodWithStringVarArg: function(b/*, ...strings*/) {
              var stringArray = Array.prototype.slice.call(arguments, 1);
              $jsExpectationsJsValue.addExpectation("stringVarArg", stringArray);
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
        jsApi.jsMethodWithIntVarArg(true, 1, 2, 3, 4)
        jsApi.jsMethodWithStringVarArg(true, "one", "two", "three", "four")
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
        assertEquals("""{"test1":"value1","test2":2}""", callbackValues!!.jsonObject1?.jsonString)
        assertEquals("[1,2,3]", callbackValues!!.jsonObject2?.jsonString)
        assertEquals(true, callbackValues!!.bool1)
        assertEquals(false, callbackValues!!.bool2)
        assertEquals(69, callbackValues!!.int)
        assertEquals(123.456, callbackValues!!.double)
        assertEquals(123, callbackValues!!.optionalInt1)
        assertEquals(null, callbackValues!!.optionalInt2)
        assertTrue(unitCallbackCalled)
        assert(fulfilledPromise.isCompleted)
        runBlocking {
            assert(fulfilledPromise.isCompleted)
            assertEquals("Promise value", fulfilledPromise.await())
        }
        assert(rejectedPromise.isCancelled)
        runBlocking {
            val promiseException: JsException = assertFailsWith {
                rejectedPromise.await()
            }
            val promiseErrorPayload = promiseException.jsonValue?.toPayload()
            assertEquals(987.0, promiseErrorPayload?.doubleValue())
        }
        runBlocking {
            val jsValueString = jsValueAsync.await().evaluate<String>()
            assertEquals("JsValue value", jsValueString)
        }

        jsExpectations.checkEquals("stringArg", "Hello JS!")
        jsExpectations.checkBlock("jsonObjectArg") { value: JsonObjectWrapper? ->
            assertEquals(objectJson.toPayload(), value?.toPayload())
        }
        jsExpectations.checkBlock("jsonArrayArg") { value: JsonObjectWrapper? ->
            assertEquals(arrayJson.toPayload(), value?.toPayload())
        }
        runBlocking {
            val jsValueArg = jsExpectations.takeExpectation<JsValue>("jsValueArg")
            assertNotNull(jsValueArg)
            assertTrue(subject.evaluateAsync<Boolean>("$jsValueArg == $jsValueParam").await())
        }
        jsExpectations.checkEquals("intVarArg", intArrayOf(1, 2, 3, 4))
        jsExpectations.checkEquals("stringVarArg", arrayOf("one", "two", "three", "four"))
        jsExpectations.checkEquals("cbRet", "CallbackTestRetVal")
        assertTrue(jsExpectations.isEmpty)

        assertTrue(errors.isEmpty())
    }

    @Test
    fun testRegisterNativeToJsInterfaceSuspend() {
        // GIVEN
        val subject = createAndSetUpJsBridge()
        val jsApiNoObjectValue = JsValue(subject, "undefined")
        val jsApiEmptyObjectValue = JsValue(subject, "({})")

        // WHEN
        runBlocking {
            assertFailsWith<IllegalArgumentException> {
                jsApiNoObjectValue.mapToNativeObject<TestJsApiInterface>(false)
            }
            assertFailsWith<IllegalArgumentException> {
                jsApiNoObjectValue.mapToNativeObject<TestJsApiInterface>(true)
            }
            jsApiEmptyObjectValue.mapToNativeObject<TestJsApiInterface>(false)  // do not fail
            assertFailsWith<IllegalArgumentException> {
                jsApiEmptyObjectValue.mapToNativeObject<TestJsApiInterface>(true)
            }
        }
    }

    @Test
    fun testRegisterNativeToJsInterfaceUnchecked() {
        // GIVEN
        val subject = createAndSetUpJsBridge()
        val jsApiValue = JsValue(subject, """({
          jsMethodReturningFulfilledPromise: function(msg) {
            return msg;
          }
        });""")
        val jsApiNoObjectValue = JsValue(subject, "undefined")
        val jsApiEmptyObjectValue = JsValue(subject, "({})")

        // WHEN
        val jsApi: TestJsApiInterface = jsApiValue.mapToNativeObject()
        val jsApiNoObject: TestJsApiInterface = jsApiNoObjectValue.mapToNativeObject()
        val jsApiEmptyObject: TestJsApiInterface = jsApiEmptyObjectValue.mapToNativeObject()

        // THEN
        runBlocking {
            assertEquals("test string", jsApi.jsMethodReturningFulfilledPromise("test string").await())
            assertFailsWith<IllegalArgumentException> {
                jsApiNoObject.jsMethodReturningFulfilledPromise("test string").await()
            }
            assertFailsWith<JsException> {
                jsApiEmptyObject.jsMethodReturningFulfilledPromise("test string").await()
            }
        }
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
                }
              });
            });"""
        )

        // WHEN
        runBlocking {
            val jsApi: TestJsApiInterface = jsApiPromise.await().mapToNativeObject()
            jsApi.jsMethodWithString("Hello JS!")
        }

        // THEN
        // Note: mockk verify with timeout has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(timeout = 1000) { jsToNativeFunctionMock(eq("Hello JS!")) }
        }
        assertTrue(errors.isEmpty())
    }

    @Test
    fun testDestroy() {
        // WHEN
        val subject = createAndSetUpJsBridge()
        val testJsApi = JsValue(subject, """({
            jsMethodWithCallback: function(cb) { cb(); }
        })""")
        val jsApi: TestJsApiInterface = testJsApi.mapToNativeObject()

        var callbackCalled = false

        // WHEN
        subject.release()
        subject.evaluateUnsync("""nativeFunctionMock("Should not be called.");""")
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
    @Feature_SetTimeout
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

        subject.evaluateUnsync(js)

        runBlocking { waitForDone(subject) }

        // THEN
        // no event is sent immediately
        verify(inverse = true) { jsToNativeFunctionMock(any()) }

        // AND THEN
        // events are sent in the right order
        // Note: mockk verify with timeout has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(ordering = Ordering.SEQUENCE, timeout = initialDelay + timeoutCount * 10L * 2) {
                for (i in 1..timeoutCount) {
                    jsToNativeFunctionMock("timeout$i")
                }
            }
        }

        assertTrue(errors.isEmpty())
    }

    @Test
    @Feature_SetTimeout
    fun testSetTimeoutWithArgs() {
        // GIVEN
        val subject = createAndSetUpJsBridge()
        val jsExpectations = JsExpectations()
        val jsExpectationsJsValue = JsValue.fromNativeObject(subject, jsExpectations)

        // WHEN
        val js = """
            setTimeout(function(a, b, c) {
                $jsExpectationsJsValue.addExpectation("a", a);
                $jsExpectationsJsValue.addExpectation("b", b);
                $jsExpectationsJsValue.addExpectation("c", c);
            }, 100, "aString", null, 69);
        """.trimIndent()

        subject.evaluateUnsync(js)

        runBlocking { delay(1000); waitForDone(subject) }

        // THEN
        jsExpectations.checkEquals("a", "aString")
        jsExpectations.checkEquals("b", JsonObjectWrapper("null"))
        jsExpectations.checkEquals("c", 69)

        assertTrue(errors.isEmpty())
    }

    @Test
    @Feature_SetTimeout
    fun testSetTimeoutWithArgsAndNullTimeout() {
        // GIVEN
        val subject = createAndSetUpJsBridge()
        val jsExpectations = JsExpectations()
        val jsExpectationsJsValue = JsValue.fromNativeObject(subject, jsExpectations)

        // WHEN
        val js = """
            setTimeout(function(a, b, c) {
                $jsExpectationsJsValue.addExpectation("a", a);
                $jsExpectationsJsValue.addExpectation("b", b);
                $jsExpectationsJsValue.addExpectation("c", c);
            }, null, "aString", null, 69);
        """.trimIndent()

        subject.evaluateUnsync(js)

        runBlocking { delay(1000); waitForDone(subject) }

        // THEN
        jsExpectations.checkEquals("a", "aString")
        jsExpectations.checkEquals("b", JsonObjectWrapper("null"))
        jsExpectations.checkEquals("c", 69)

        assertTrue(errors.isEmpty())
    }

    @Test
    @Feature_SetTimeout
    fun testSetTimeoutWithArgsAndUndefinedTimeout() {
        // GIVEN
        val subject = createAndSetUpJsBridge()
        val jsExpectations = JsExpectations()
        val jsExpectationsJsValue = JsValue.fromNativeObject(subject, jsExpectations)

        // WHEN
        val js = """
            setTimeout(function(a, b, c) {
                $jsExpectationsJsValue.addExpectation("a", a);
                $jsExpectationsJsValue.addExpectation("b", b);
                $jsExpectationsJsValue.addExpectation("c", c);
            }, undefined, "aString", null, 69);
        """.trimIndent()

        subject.evaluateUnsync(js)

        runBlocking { delay(1000); waitForDone(subject) }

        // THEN
        jsExpectations.checkEquals("a", "aString")
        jsExpectations.checkEquals("b", JsonObjectWrapper("null"))
        jsExpectations.checkEquals("c", 69)

        assertTrue(errors.isEmpty())
    }

    @Test
    @Feature_SetTimeout
    fun testSetTimeoutWithArgsAndStringNumberTimeout() {
        // GIVEN
        val subject = createAndSetUpJsBridge()
        val jsExpectations = JsExpectations()
        val jsExpectationsJsValue = JsValue.fromNativeObject(subject, jsExpectations)

        // WHEN
        val js = """
            setTimeout(function(a, b, c) {
                $jsExpectationsJsValue.addExpectation("a", a);
                $jsExpectationsJsValue.addExpectation("b", b);
                $jsExpectationsJsValue.addExpectation("c", c);
            }, "100", "aString", null, 69);
        """.trimIndent()

        subject.evaluateUnsync(js)

        runBlocking { delay(1000); waitForDone(subject) }

        // THEN
        jsExpectations.checkEquals("a", "aString")
        jsExpectations.checkEquals("b", JsonObjectWrapper("null"))
        jsExpectations.checkEquals("c", 69)

        assertTrue(errors.isEmpty())
    }

    @Test
    @Feature_SetTimeout
    fun testSetTimeoutWithArgsAndWrongTypeTimeout() {
        // GIVEN
        val subject = createAndSetUpJsBridge()
        val jsExpectations = JsExpectations()
        val jsExpectationsJsValue = JsValue.fromNativeObject(subject, jsExpectations)

        // WHEN
        val js = """
            setTimeout(function(a, b, c) {
                $jsExpectationsJsValue.addExpectation("a", a);
                $jsExpectationsJsValue.addExpectation("b", b);
                $jsExpectationsJsValue.addExpectation("c", c);
            }, "not_a_number_is_zero_timeout", "aString", null, 69);
        """.trimIndent()

        subject.evaluateUnsync(js)

        runBlocking { delay(1000); waitForDone(subject) }

        // THEN
        jsExpectations.checkEquals("a", "aString")
        jsExpectations.checkEquals("b", JsonObjectWrapper("null"))
        jsExpectations.checkEquals("c", 69)

        assertTrue(errors.isEmpty())
    }

    @Test
    @Feature_SetTimeout
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

        subject.evaluateUnsync(js)

        // THEN
        runBlocking {
            waitForDone(subject)
            delay(500)
        }
        verify(exactly = 2) { jsToNativeFunctionMock(neq("timeout2")) }

        // timeout1 should be skipped!
        assertEquals(2, events.size)
        assertEquals("timeout1", events[0])
        assertEquals("timeout3", events[1])
        assertTrue(errors.isEmpty())
    }

    @Test
    @Feature_SetTimeout
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

        subject.evaluateUnsync(js)

        runBlocking { waitForDone(subject) }

        // THEN
        // Note: mockk verify with ordering currently has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(ordering = Ordering.SEQUENCE, timeout = 1000) {
                jsToNativeFunctionMock(eq("interval1"))
                jsToNativeFunctionMock(eq("interval2"))
                jsToNativeFunctionMock(eq("interval3"))
            }
        }

        // Stop *after* interval3
        // Note: mockk verify with timeout has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(inverse = true, timeout = 1000) { jsToNativeFunctionMock(eq("interval4")) }
        }

        assertTrue(errors.isEmpty())
    }

    @Test
    @Feature_SetTimeout
    fun testSetAndClearIntervalWithNullTimeout() {
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
            }, null);
        """

        subject.evaluateUnsync(js)

        runBlocking { waitForDone(subject) }

        // THEN
        // Note: mockk verify with ordering currently has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(ordering = Ordering.SEQUENCE, timeout = 1000) {
                jsToNativeFunctionMock(eq("interval1"))
                jsToNativeFunctionMock(eq("interval2"))
                jsToNativeFunctionMock(eq("interval3"))
            }
        }

        // Stop *after* interval3
        // Note: mockk verify with timeout has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(inverse = true, timeout = 1000) { jsToNativeFunctionMock(eq("interval4")) }
        }

        assertTrue(errors.isEmpty())
    }

    @Test
    @Feature_SetTimeout
    fun testSetAndClearIntervalWithUndefinedTimeout() {
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
            }, undefined);
        """

        subject.evaluateUnsync(js)

        runBlocking { waitForDone(subject) }

        // THEN
        // Note: mockk verify with ordering currently has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(ordering = Ordering.SEQUENCE, timeout = 1000) {
                jsToNativeFunctionMock(eq("interval1"))
                jsToNativeFunctionMock(eq("interval2"))
                jsToNativeFunctionMock(eq("interval3"))
            }
        }

        // Stop *after* interval3
        // Note: mockk verify with timeout has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(inverse = true, timeout = 1000) { jsToNativeFunctionMock(eq("interval4")) }
        }

        assertTrue(errors.isEmpty())
    }

    @Test
    @Feature_SetTimeout
    fun testSetAndClearIntervalWithWrongTypeTimeout() {
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
            }, "not_a_number_is_zero_timeout");
        """

        subject.evaluateUnsync(js)

        runBlocking { waitForDone(subject) }

        // THEN
        // Note: mockk verify with ordering currently has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(ordering = Ordering.SEQUENCE, timeout = 1000) {
                jsToNativeFunctionMock(eq("interval1"))
                jsToNativeFunctionMock(eq("interval2"))
                jsToNativeFunctionMock(eq("interval3"))
            }
        }

        // Stop *after* interval3
        // Note: mockk verify with timeout has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(inverse = true, timeout = 1000) { jsToNativeFunctionMock(eq("interval4")) }
        }

        assertTrue(errors.isEmpty())
    }

    @Test
    @Feature_SetTimeout
    fun testSetAndClearIntervalWithStringNumberTimeout() {
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
            }, "100");
        """

        subject.evaluateUnsync(js)

        runBlocking { waitForDone(subject) }

        // THEN
        // Note: mockk verify with ordering currently has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(ordering = Ordering.SEQUENCE, timeout = 1000) {
                jsToNativeFunctionMock(eq("interval1"))
                jsToNativeFunctionMock(eq("interval2"))
                jsToNativeFunctionMock(eq("interval3"))
            }
        }

        // Stop *after* interval3
        // Note: mockk verify with timeout has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(inverse = true, timeout = 1000) { jsToNativeFunctionMock(eq("interval4")) }
        }

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
        subject.evaluateUnsync(js)

        // THEN
        // Note: mock verify with ordering currently has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(timeout = 2000, ordering = Ordering.SEQUENCE) {
                jsToNativeFunctionMock(match { responseJson ->
                    assertNotNull(responseJson)
                    val responseObject = PayloadObject.fromJsonString(responseJson as String)
                    assertNotNull(responseObject)
                    assertEquals(2, responseObject.keyCount)
                    assertEquals("testValue1", responseObject.getString("testKey1"))
                    assertEquals("testValue2", responseObject.getString("testKey2"))
                    true
                })

                jsToNativeFunctionMock(match { headersJson ->
                    val responseHeadersObject = PayloadObject.fromJsonString(headersJson as String)
                    assertNotNull(responseHeadersObject)
                    assertEquals(2, responseHeadersObject.keyCount)
                    assertEquals(
                        "testResponseHeaderValue1",
                        responseHeadersObject.getString("testresponseheaderkey1")
                    )
                    assertEquals(
                        "testResponseHeaderValue2",
                        responseHeadersObject.getString("testresponseheaderkey2")
                    )
                    true
                })
            }
        }
        assertTrue(errors.isEmpty())
    }

    @Test
    fun testXmlHttpEmptyBodyPostRequest() {
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
            |xhr.open("POST", "$url");
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
        subject.evaluateUnsync(js)

        // THEN
        // Note: mock verify with ordering currently has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(timeout = 2000, ordering = Ordering.SEQUENCE) {
                jsToNativeFunctionMock(match { responseJson ->
                    assertNotNull(responseJson)
                    val responseObject = PayloadObject.fromJsonString(responseJson as String)
                    assertNotNull(responseObject)
                    assertEquals(2, responseObject.keyCount)
                    assertEquals("testValue1", responseObject.getString("testKey1"))
                    assertEquals("testValue2", responseObject.getString("testKey2"))
                    true
                })

                jsToNativeFunctionMock(match { headersJson ->
                    val responseHeadersObject = PayloadObject.fromJsonString(headersJson as String)
                    assertNotNull(responseHeadersObject)
                    assertEquals(2, responseHeadersObject.keyCount)
                    assertEquals(
                        "testResponseHeaderValue1",
                        responseHeadersObject.getString("testresponseheaderkey1")
                    )
                    assertEquals(
                        "testResponseHeaderValue2",
                        responseHeadersObject.getString("testresponseheaderkey2")
                    )
                    true
                })
            }
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
        subject.evaluateUnsync(js)

        // THEN
        // Note: mockk verify with timeout has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(timeout = 2000) { jsToNativeFunctionMock(eq("aborted")) }
        }
        verify(inverse = true) { jsToNativeFunctionMock(eq("loaded")) }
        assertTrue(errors.isEmpty())
    }

    @Test
    fun testXmlHttpRequest_error() {
        // GIVEN
        val subject = createAndSetUpJsBridge()

        // WHEN
        val js = """
            var req = new XMLHttpRequest();
            req.open("GET", "invalid url");
            req.send();
            req.onload = function() {
                nativeFunctionMock("loaded");
            }
            req.onerror = function() {
                nativeFunctionMock("XHR error: " + req.responseText);
            }
            """
        subject.evaluateUnsync(js)

        // THEN
        // Note: mockk verify with timeout has some issues on API < 24
        if (android.os.Build.VERSION.SDK_INT >= 24) {
            verify(timeout = 2000) { jsToNativeFunctionMock(eq("XHR error: Cannot parse URL: invalid url")) }
        }
        verify(inverse = true) { jsToNativeFunctionMock(eq("loaded")) }
        assertTrue(errors.isEmpty())
    }

    @Test
    fun testConsole_asString() {
        // GIVEN
        val messages = mutableListOf<Pair<Int, String>>()
        val config = JsBridgeConfig.bareConfig().apply {
            consoleConfig.enabled = true
            consoleConfig.mode = JsBridgeConfig.ConsoleConfig.Mode.AsString
            consoleConfig.appendMessage = { priority, message ->
                messages.add(priority to message)
            }
        }
        val subject = JsBridge(config)
        jsBridge = subject

        // WHEN
        val js = """
            console.log("This is a log with undefined and null:", undefined, "-", null);
            console.info("This is an info with 2 integers:", 1664, "and", 69);
            console.warn("This is a warning with an object:", { one: 1, two: "two" });
            console.error("This is an error:", new Error("completely wrong"));
            console.assert(1 == 1, "should not be displayed");
            console.assert(1 == 2, "should be displayed");
            """
        subject.evaluateUnsync(js)

        runBlocking { waitForDone(subject) }

        // THEN
        assertEquals(listOf(
            Log.DEBUG to "This is a log with undefined and null: undefined - null",
            Log.INFO to "This is an info with 2 integers: 1664 and 69",
            Log.WARN to """This is a warning with an object: [object Object]""",
            Log.ERROR to """This is an error: Error: completely wrong""",
            Log.ASSERT to """Assertion failed: should be displayed"""
        ), messages)
        assertTrue(errors.isEmpty())
    }

    @Test
    fun testConsole_asJson() {
        // GIVEN
        val messages = mutableListOf<Pair<Int, String>>()
        val config = JsBridgeConfig.bareConfig().apply {
            consoleConfig.enabled = true
            consoleConfig.mode = JsBridgeConfig.ConsoleConfig.Mode.AsJson
            consoleConfig.appendMessage = { priority, message ->
                messages.add(priority to message)
            }
        }
        val subject = JsBridge(config)
        jsBridge = subject

        // WHEN
        val js = """
            console.log("This is a log with undefined and null:", undefined, "-", null);
            console.info("This is an info with 2 integers:", 1664, "and", 69);
            console.warn("This is a warning with an object:", { one: 1, two: "two" });
            console.error("This is an error:", new Error("completely wrong"));
            console.assert(1 == 1, "should not be displayed");
            console.assert(1 == 2, "should be displayed");
            """.trimMargin()
        subject.evaluateUnsync(js)

        runBlocking { waitForDone(subject) }

        // THEN
        assertEquals(5, messages.size)
        assertEquals(Log.DEBUG to "This is a log with undefined and null: undefined - null", messages[0])
        assertEquals(Log.INFO to "This is an info with 2 integers: 1664 and 69", messages[1])
        assertEquals(Log.WARN to """This is a warning with an object: {"one":1,"two":"two"}""", messages[2])
        assertEquals(Log.ERROR, messages[3].first)
        assertTrue(messages[3].second.matches("""^This is an error: \{.*"message":"completely wrong".*\}$""".toRegex()))
        assertEquals(Log.ASSERT to "Assertion failed: should be displayed", messages[4])
        assertTrue(errors.isEmpty())
    }

    @Test
    fun testConsole_empty() {
        // GIVEN
        var hasMessage = false
        val config = JsBridgeConfig.bareConfig().apply {
            consoleConfig.enabled = true
            consoleConfig.mode = JsBridgeConfig.ConsoleConfig.Mode.Empty
            consoleConfig.appendMessage = { _, _ ->
                hasMessage = true
            }
        }
        val subject = JsBridge(config)
        jsBridge = subject

        // WHEN
        val js = """
            console.log("This is a log with undefined and null:", undefined, "-", null);
            console.info("This is an info with 2 integers:", 1664, "and", 69);
            console.warn("This is a warning with an object:", { one: 1, two: "two" });
            console.error("This is an error:", new Error("completely wrong"));
            console.assert(1 == 1, "should not be displayed");
            console.assert(1 == 2, "should be displayed");
            """
        subject.evaluateUnsync(js)

        runBlocking { waitForDone(subject) }

        // THEN
        assertFalse(hasMessage)
        assertTrue(errors.isEmpty())
    }

    @Test
    fun testAidl() {
        // GIVEN
        val subject = createAndSetUpJsBridge()
        val kotlinParcelable = TestAidlParcelable().also { p ->
            p.intField = 12
            p.stringField = "kotlinParcelable"
        }
        val kotlinParcelableArray = arrayOf(
            TestAidlParcelable().also { p ->
                p.intField = 100
                p.stringField = "kotlinParcelableArrayItem1"
            },
            TestAidlParcelable().also { p ->
                p.intField = 101
                p.stringField = "kotlinParcelableArrayItem2"
            },
        )
        val kotlinEnum = TestAidlEnum.SECOND
        var jsParcelable: TestAidlParcelable? = null
        var jsParcelableArray: Array<TestAidlParcelable>? = null
        var jsEnum: Byte? = null
        val expectedJsValues = JsValue(subject, "({})")

        // WHEN
        val aidlInstance = object: TestAidlInterface.Default() {
            override fun triggerCallback(o: TestAidlCallback) {
                o.onDone()
                o.onDoneWithParcelable(kotlinParcelable)
                o.onDoneWithParcelableArray(kotlinParcelableArray)
            }
            override fun setParcelable(payload: TestAidlParcelable) {
                jsParcelable = payload
            }
            override fun setParcelableArray(payload: Array<TestAidlParcelable>) {
                jsParcelableArray = payload
            }
            override fun setEnum(payload: Byte) {
                jsEnum = payload
            }
            override fun getParcelable(): TestAidlParcelable {
                return kotlinParcelable
            }
            override fun getEnum(): Byte {
                return kotlinEnum
            }
        }
        val aidlInstanceJsValue = JsValue.fromAidlInterface(subject, aidlInstance)

        val js = """
            // Call AIDL interface method and receive callback
            $aidlInstanceJsValue.triggerCallback({
              onDone: function() {
                $expectedJsValues.onDone = true
              },
              onDoneWithParcelable: function(p) {
                $expectedJsValues.onDoneWithParcelable = p
              },
              onDoneWithParcelableArray: function(pa) {
                $expectedJsValues.onDoneWithParcelableArray = pa;
              },
            });
            
            // Pass parcelable to AIDL interface method
            $aidlInstanceJsValue.setParcelable({intField: 1, stringField: "one"});
            
            // Pass parcelable to AIDL interface method
            $aidlInstanceJsValue.setParcelableArray([
                {intField: 1, stringField: "one"},
                {intField: 2, stringField: "two"},
                {intField: 3, stringField: "three"},
            ]);
            
            // Pass enum to AIDL interface method
            $aidlInstanceJsValue.setEnum(${TestAidlEnum.FIRST});
            
            // Get parcelable as return value of AIDL interface method
            $expectedJsValues.parcelableFromKotlin = $aidlInstanceJsValue.getParcelable();
            
            // Get enum as return value of AIDL interface method
            $expectedJsValues.enumFromKotlin = $aidlInstanceJsValue.getEnum();
            """

        runBlocking {
            subject.evaluate<Unit>(js)
            val expectedJsValuesMap = expectedJsValues.evaluate<JsonObjectWrapper>()
            waitForDone(subject)

            // THEN
            assertTrue(errors.isEmpty())
            assertEquals(1, jsParcelable?.intField)
            assertEquals("one", jsParcelable?.stringField)
            assertEquals(3, jsParcelableArray?.size)
            assertEquals(1, jsParcelableArray!![0].intField)
            assertEquals(2, jsParcelableArray!![1].intField)
            assertEquals(3, jsParcelableArray!![2].intField)
            assertEquals(TestAidlEnum.FIRST, jsEnum)
            assertEquals(payloadObjectOf(
                "onDone" to true,
                "onDoneWithParcelable" to payloadObjectOf("intField" to kotlinParcelable.intField, "stringField" to kotlinParcelable.stringField),
                "onDoneWithParcelableArray" to payloadArrayOf(
                    payloadObjectOf("intField" to kotlinParcelableArray[0].intField, "stringField" to kotlinParcelableArray[0].stringField),
                    payloadObjectOf("intField" to kotlinParcelableArray[1].intField, "stringField" to kotlinParcelableArray[1].stringField),
                ),
                "parcelableFromKotlin" to payloadObjectOf("intField" to kotlinParcelable.intField, "stringField" to kotlinParcelable.stringField),
                "enumFromKotlin" to kotlinEnum.toInt(),
            ), expectedJsValuesMap.toPayloadObject())
        }
    }

    interface TestAidlInterfaceReuseNativeApi : JsToNativeInterface {
        fun registerCallback(cb: TestAidlCallback)
        fun unregisterCallback(cb: TestAidlCallback)
    }

    @Test
    fun testAidlInterfaceReuse() {
        // GIVEN
        val subject = createAndSetUpJsBridge()
        val nativeApi = object : TestAidlInterfaceReuseNativeApi {
            var registeredCallback: TestAidlCallback? = null
            var done = false

            override fun registerCallback(cb: TestAidlCallback) {
                assertNull(registeredCallback)
                registeredCallback = cb
            }

            override fun unregisterCallback(cb: TestAidlCallback) {
                assertNotNull(registeredCallback)
                assertSame(cb, registeredCallback)
                assertFalse(done)
                done = true
            }
        }
        val nativeApiJsValue = JsValue.fromNativeObject(subject, nativeApi)

        // WHEN
        val js = """
            var cb = {};
            $nativeApiJsValue.registerCallback(cb);
            $nativeApiJsValue.unregisterCallback(cb);
        """.trimIndent()
        runBlocking {
            subject.evaluate<Unit>(js)
            assertTrue(nativeApi.done)
        }
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
            when (value) {
                is IntArray -> assertArrayEquals(value, takeExpectation(name))
                is Array<*> -> assertArrayEquals(value, takeExpectation(name))
                else -> assertEquals(value, takeExpectation(name))
            }
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
        val config = JsBridgeConfig.standardConfig().apply {
            xhrConfig.okHttpClient = okHttpClient
        }

        return JsBridge(config).also { jsBridge ->
            this@JsBridgeTest.jsBridge = jsBridge

            jsBridge.registerErrorListener(createErrorListener())

            // Map "jsToNativeFunctionMock" to JS as a global var "NativeFunctionMock"
            JsValue.fromNativeFunction1(jsBridge, jsToNativeFunctionMock).assignToGlobal("nativeFunctionMock")
        }
    }

    private fun createErrorListener(): JsBridge.ErrorListener {
        // ensure we are not messing up the errors instance
        val errors = this.errors
        val unhandledPromiseErrors = unhandledPromiseErrors

        return object: JsBridge.ErrorListener(Dispatchers.Main) {
            override fun onError(error: JsBridgeError) {
                if (error is JsBridgeError.UnhandledJsPromiseError) {
                    unhandledPromiseErrors.add(error)
                } else {
                    errors.add(error)
                }
            }
        }
    }

    @Suppress("UNUSED")  // Only for debugging
    private fun printErrors() {
        if (errors.isNotEmpty()) {
            Timber.e("JsBridge errors:")
            errors.forEachIndexed { index, error ->
                Timber.e("Error ${index + 1}/${errors.count()}:\n")
                Timber.e(error)
            }
        }

        if (unhandledPromiseErrors.isNotEmpty()) {
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
           yield()

           // ensure that triggered coroutines are processed
           withContext(jsBridge.coroutineContext) {
               delay(100)
           }

           yield()
       } catch (e: CancellationException) {
            // Ignore cancellation
        }
    }
}
