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
import kotlin.test.assertEquals
import kotlinx.coroutines.*
import org.junit.After
import org.junit.Before
import org.junit.BeforeClass
import org.junit.Test
import timber.log.Timber

class ReadmeTest {
    private lateinit var jsBridge: JsBridge

    companion object {
        @BeforeClass
        @JvmStatic
        fun setUpClass() {
            Timber.plant(Timber.DebugTree())
        }
    }

    @Before
    fun setUp() {
        jsBridge = JsBridge(JsBridgeConfig.standardConfig())
    }

    @After
    fun cleanUp() {
        jsBridge.let {
            runBlocking {
                waitForDone(it)
                it.release()
            }
        }
    }

    @Test
    fun testHelloWorld() {
        runBlocking {
            val msg: String = jsBridge.evaluate("'Hello world!'.toUpperCase()")
            println(msg)  // HELLO WORLD!
        }
    }

    @Test
    fun testEvaluation() {
        // Without return value:
        jsBridge.evaluateUnsync("console.log('hello');")
        jsBridge.evaluateFileContentUnsync("console.log('hello')", "js/test.js")

        runBlocking {
            // Without return value:
            jsBridge.evaluate<Unit>("console.log('hello');")
            jsBridge.evaluateFileContent("console.log('hello')", "js/test.js")

            // With return value:
            val sum1: Int = jsBridge.evaluate("1+2")  // suspending call
            val sum2: Int = jsBridge.evaluate("new Promise(function(resolve) { resolve(1+2); })")  // suspending call (JS promise)
            val msg: String = jsBridge.evaluate("'message'.toUpperCase()")  // suspending call
            val obj: JsonObjectWrapper =
                jsBridge.evaluate("({one: 1, two: 'two'})")  // suspending call (wrapped JS object via JSON)

            // Blocking evaluation:
            val result1: Int = jsBridge.evaluateBlocking("1+2")  // generic type inferred
            val result2 = jsBridge.evaluateBlocking<Int>("1+2")  // with explicit generic type

            // Exception handling:
            try {
                jsBridge.evaluate<Unit>("""
                  |function buggy() { throw new Error('wrong') }
                  |buggy();
                """.trimMargin())
            } catch (jse: JsException) {
                // jse.message = "wrong"
                // jse.stackTrace = [JavaScript.buggy(eval:1), JavaScript.<eval>(eval:2), ....JsBridge.jniEvaluateString(Native Method), ...]
            }
        }
    }

    @Test
    fun testJsValue() {
        runBlocking {
            val jsInt1 = JsValue(jsBridge, "123")
            val jsInt2 = JsValue.fromNativeValue(jsBridge, 123)
            val jsString1 = JsValue(jsBridge, "'hello'.toUpperCase()")
            val jsString2 = JsValue.fromNativeValue(jsBridge, "HELLO")
            val jsObject1 = JsValue(jsBridge, "({one: 1, two: 'two'})")
            val jsObject2 = JsValue.fromNativeValue(jsBridge, JsonObjectWrapper("one" to 1, "two" to "two"))
            val calcSumJs1 = JsValue(jsBridge, "(function(a, b) { return a + b; })")
            val calcSumJs2 = JsValue.newFunction(jsBridge, "a", "b", "return a + b;")
            val calcSumJs3 = JsValue.fromNativeFunction2(jsBridge) { a: Int, b: Int -> a + b }

            val sum: Int = jsBridge.evaluate("$calcSumJs1(2, 3)")

            val s1 = jsString1.evaluate<String>()  // suspending function + explicit generic parameter
            val s2: String = jsString1.evaluate()  // suspending function + inferred generic parameter
            val s3: String = jsString1.evaluateBlocking()  // blocking
        }
    }

    interface JsApi1 : NativeToJsInterface {
        fun method1(a: Int, b: String)
        suspend fun method2(c: Double): String
    }

    @Test
    fun testJsObjectsFromKotlin() {
        val jsObject = JsValue(jsBridge, """({
          method1: function(a, b) { /*...*/ },
          method2: function(c) { return "Value: " + c; }
        })""")

        val jsApi1: JsApi1 = jsObject.mapToNativeObject()  // no check
        runBlocking {
            val jsApi2: JsApi1 = jsObject.mapToNativeObject(check = true)  // suspending, optionally check that all methods are defined in the JS object
        }
        val jsApi3: JsApi1 = jsObject.mapToNativeObjectBlocking(check = true)  // blocking (with optional check)

        jsApi1.method1(1, "two")
        runBlocking {
            val s = jsApi1.method2(3.456)  // suspending
        }
    }

    interface NativeApi1 : JsToNativeInterface {
        fun method(a: Int, b: String): Double
    }

    @Test
    fun testKotlinObjectsFromJs() {
        val obj = object : NativeApi1 {
            override fun method(a: Int, b: String): Double { return 123.456 }
        }

        val nativeApi: JsValue = JsValue.fromNativeObject(jsBridge, obj)

        jsBridge.evaluateNoRetVal("globalThis.x = $nativeApi.method(1, 'two');")
    }

    @Test
    fun testJsFunctionFromKotlin() {
        val calcSumJs: suspend (Int, Int) -> Int = JsValue.newFunction(jsBridge, "a", "b", """
          return a + b;
          """.trimIndent()
        ).mapToNativeFunction2()

        println("Sum is $calcSumJs(1, 2)")
    }

    @Test
    fun testKotlinFunctionFromJs() {
        val calcSumNative = JsValue.fromNativeFunction2(jsBridge) { a: Int, b: Int -> a + b }

        jsBridge.evaluateNoRetVal("""
          console.log("Sum is", $calcSumNative(1, 2));
          """.trimIndent())
    }

    interface JsApi : NativeToJsInterface {
        suspend fun createMessage(): String
        suspend fun calcSum(a: Int, b: Int): Int
    }

    interface NativeApi : JsToNativeInterface {
        fun getPlatformName(): String
        fun getTemperatureCelcius(): Deferred<Float>
    }

    @Test
    fun testUsageAdvanced() {
        jsBridge.evaluateLocalFileUnsync(InstrumentationRegistry.getInstrumentation().context, "js/api.js")

        // Implement native API
        val nativeApi = object: NativeApi {
            override fun getPlatformName() = "Android"
            override fun getTemperatureCelcius() = GlobalScope.async {
                // Getting current temperature from sensor or via network service
                37.2f
            }
        }
        val nativeApiJsValue = JsValue.fromNativeObject(jsBridge, nativeApi)

        // Create JS API
        val config = JsonObjectWrapper("debug" to true, "useFahrenheit" to false)  // {debug: true, useFahrenheit: false}
        runBlocking {
            val createJsApi: suspend (JsValue, JsonObjectWrapper) -> JsValue
                    = JsValue(jsBridge, "createApi").mapToNativeFunction2()  // JS: global.createApi(nativeApi, config)
            val jsApi: JsApi = createJsApi(nativeApiJsValue, config).mapToNativeObject()

            // Call JS API methods
            val msg = jsApi.createMessage()  // (suspending) "Hello Android, the temperature is 37.2 degrees C."
            val sum = jsApi.calcSum(3, 2)  // (suspending) 5

            assertEquals(msg, "Hello Android! The temperature is 37.2 degrees C.")
            assertEquals(sum, 5)
        }
    }

    // Wait until the JS queue is empty
    private suspend fun waitForDone(jsBridge: JsBridge) {
        try {
            withContext(jsBridge.coroutineContext) {
                // ensure that triggered coroutines are processed
                yield()
            }
        } catch (e: CancellationException) {
            // Ignore cancellation
        }
    }
}
