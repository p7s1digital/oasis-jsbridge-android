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
    private val context: Context = InstrumentationRegistry.getInstrumentation().context

    companion object {
        @BeforeClass
        @JvmStatic
        fun setUpClass() {
            Timber.plant(Timber.DebugTree())
        }
    }

    @Before
    fun setUp() {
        jsBridge = JsBridge(JsBridgeConfig.standardConfig(), context)
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
                // jse.stackTrace = [JavaScript.buggy(eval:1), JavaScript.<eval>(eval:2), ....JsBridge.jniEvaluateString(Java Method), ...]
            }
        }
    }

    @Test
    fun testJsValue() {
        runBlocking {
            val jsInt1 = JsValue(jsBridge, "123")
            val jsInt2 = JsValue.fromJavaValue(jsBridge, 123)
            val jsString1 = JsValue(jsBridge, "'hello'.toUpperCase()")
            val jsString2 = JsValue.fromJavaValue(jsBridge, "HELLO")
            val jsObject1 = JsValue(jsBridge, "({one: 1, two: 'two'})")
            val jsObject2 = JsValue.fromJavaValue(jsBridge, JsonObjectWrapper("one" to 1, "two" to "two"))
            val calcSumJs1 = JsValue(jsBridge, "(function(a, b) { return a + b; })")
            val calcSumJs2 = JsValue.newFunction(jsBridge, "a", "b", "return a + b;")
            val calcSumJs3 = JsValue.createJsToJavaProxyFunction2(jsBridge) { a: Int, b: Int -> a + b }

            val sum: Int = jsBridge.evaluate("$calcSumJs1(2, 3)")

            val s1 = jsString1.evaluate<String>()  // suspending function + explicit generic parameter
            val s2: String = jsString1.evaluate()  // suspending function + inferred generic parameter
            val s3: String = jsString1.evaluateBlocking()  // blocking
        }
    }

    interface JsApi1 : JavaToJsInterface {
        fun method1(a: Int, b: String)
        suspend fun method2(c: Double): String
    }

    @Test
    fun testJsObjectsFromKotlin() {
        val jsObject = JsValue(jsBridge, """({
          method1: function(a, b) { /*...*/ },
          method2: function(c) { return "Value: " + c; }
        })""")

        val jsApi1: JsApi1 = jsObject.createJavaToJsProxy()  // no check
        runBlocking {
            val jsApi2: JsApi1 = jsObject.createJavaToJsProxy(check = true)  // suspending, optionally check that all methods are defined in the JS object
        }
        val jsApi3: JsApi1 = jsObject.createJavaToJsProxyBlocking(check = true)  // blocking (with optional check)

        jsApi1.method1(1, "two")
        runBlocking {
            val s = jsApi1.method2(3.456)  // suspending
        }
    }

    interface JavaApi1 : JsToJavaInterface {
        fun method(a: Int, b: String): Double
    }

    @Test
    fun testKotlinObjectsFromJs() {
        val obj = object : JavaApi1 {
            override fun method(a: Int, b: String): Double { return 123.456 }
        }

        val javaApi: JsValue = JsValue.createJsToJavaProxy(jsBridge, obj)

        jsBridge.evaluateUnsync("globalThis.x = $javaApi.method(1, 'two');")
    }

    @Test
    fun testJsFunctionFromKotlin() {
        val calcSumJs: suspend (Int, Int) -> Int = JsValue.newFunction(jsBridge, "a", "b", """
          return a + b;
          """.trimIndent()
        ).createJavaToJsProxyFunction2()

        println("Sum is $calcSumJs(1, 2)")
    }

    @Test
    fun testKotlinFunctionFromJs() {
        val calcSumJava = JsValue.createJsToJavaProxyFunction2(jsBridge) { a: Int, b: Int -> a + b }

        jsBridge.evaluateUnsync("""
          console.log("Sum is", $calcSumJava(1, 2));
          """.trimIndent())
    }

    interface JsApi : JavaToJsInterface {
        suspend fun createMessage(): String
        suspend fun calcSum(a: Int, b: Int): Int
    }

    interface JavaApi : JsToJavaInterface {
        fun getPlatformName(): String
        fun getTemperatureCelcius(): Deferred<Float>
    }

    @Test
    fun testUsageAdvanced() {
        jsBridge.evaluateLocalFileUnsync(InstrumentationRegistry.getInstrumentation().context, "js/api.js")

        // Implement Java API
        val javaApi = object: JavaApi {
            override fun getPlatformName() = "Android"
            override fun getTemperatureCelcius() = GlobalScope.async {
                // Getting current temperature from sensor or via network service
                37.2f
            }
        }
        val javaApiJsValue = JsValue.createJsToJavaProxy(jsBridge, javaApi)

        // Create JS API
        val config = JsonObjectWrapper("debug" to true, "useFahrenheit" to false)  // {debug: true, useFahrenheit: false}
        runBlocking {
            val createJsApi: suspend (JsValue, JsonObjectWrapper) -> JsValue
                    = JsValue(jsBridge, "createApi").createJavaToJsProxyFunction2()  // JS: global.createApi(javaApi, config)
            val jsApi: JsApi = createJsApi(javaApiJsValue, config).createJavaToJsProxy()

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
