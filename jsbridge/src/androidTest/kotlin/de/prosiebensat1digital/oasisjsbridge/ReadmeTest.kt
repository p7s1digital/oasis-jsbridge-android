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
    fun testUsageMinimal() {
        runBlocking {
            val msg: String = jsBridge.evaluate("'Hello world!'.toUpperCase()")
            assertEquals(msg, "HELLO WORLD!")
        }
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
        jsBridge.evaluateLocalFile(InstrumentationRegistry.getInstrumentation().context, "js/api.js")

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

    @Test
    fun testEvaluation() {
        // Without return value:
        jsBridge.evaluateNoRetVal("console.log('hello');")

        // Blocking evaluation
        val result1: Int = jsBridge.evaluateBlocking("1+2")  // generic type inferred
        println("result1 = $result1")
        val result2 = jsBridge.evaluateBlocking<Int>("1+2")  // with explicit generic type
        println("result2 = $result2")

        // Via coroutines:
        GlobalScope.launch {
            val result3: Int = jsBridge.evaluate("1+2")  // suspending call
            println("result3 = $result3")
        }

        // Exception handling
        GlobalScope.launch {
            try {
                jsBridge.evaluate<Unit>("""
                    |function buggy() {
                    |  throw new Error('wrong')
                    |}
                    |buggy();
                """.trimMargin())
            } catch (jse: JsException) {
                println("Exception: $jse")
                println("Stacktrace: ${jse.stackTrace.asList()}")
                // detailedMessage = wrong
                // jsStackTrace = at buggy (eval:2)
                //                at <eval> (eval:4)
            }
        }
    }

    interface JsToNativeApi: JsToNativeInterface

    @Test
    fun testJsValue() {
        val jsValue1 = JsValue(jsBridge)
        val jsValue2 = JsValue(jsBridge, "'hello'.toUpperCase()")
        val calcSumJs = JsValue.newFunction(jsBridge, "a", "b", "return a + b;")
        val nativeFunctionJs = JsValue.fromNativeFunction2(jsBridge) { a: Int, b: Int -> a + b }
        val nativeObjectJs = JsValue.fromNativeObject(jsBridge, object: JsToNativeApi {})

        runBlocking {
            val sum: Int = jsBridge.evaluate("$calcSumJs(2, 3)")
            println("Sum is $sum")

            jsBridge.evaluate<Unit>("globalThis['nativeApi'] = $nativeObjectJs")
        }

        nativeObjectJs.assignToGlobal("nativeApi")
    }

    @Test
    fun testKotlinToJsFunction() {
        val calcSumJs: suspend (Int, Int) -> Int = JsValue.newFunction(jsBridge, "a", "b", """
          return a + b;
          """.trimIndent()
        ).mapToNativeFunction2()

        println("Sum is $calcSumJs(1, 2)")
    }

    @Test
    fun testJsToKotlinFunction() {
        val calcSumNative = JsValue.fromNativeFunction2(jsBridge) { a: Int, b: Int -> a + b }

        jsBridge.evaluateNoRetVal("""
          console.log("Sum is", $calcSumNative(1, 2));
          """.trimIndent())
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
