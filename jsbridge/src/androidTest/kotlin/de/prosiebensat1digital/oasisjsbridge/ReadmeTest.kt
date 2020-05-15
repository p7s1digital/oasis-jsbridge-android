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
        jsBridge = JsBridge(InstrumentationRegistry.getInstrumentation().context)
        jsBridge.start()
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
        jsBridge.evaluateNoRetVal("console.log('Hello world!');")
    }

    @Test
    fun testUsageAdvanced() {
        // Create a Kotlin proxy to a JS function
        val helloJs: suspend (String) -> Unit = JsValue.newFunction(jsBridge, "s",
            "console.log('Hello ' + s + '!');"
        ).mapToNativeFunction1()

        // Create a JS proxy to a Kotlin function
        val toUpperCaseNative = JsValue.fromNativeFunction1(jsBridge) { s: String -> s.toUpperCase() }

        runBlocking {
            // Evaluate JS promise calling Kotlin function "toUpperCaseNative()"
            // (suspend function call) -> returns "WORLD" after 1s timeout
            val world: String = jsBridge.evaluate("""new Promise(function(resolve) {
              setTimeout(function() {
                resolve($toUpperCaseNative('world'));
              }, 1000);
            })""".trimIndent())

            // Manually release the JsValue after usage (otherwise handled by garbage collector)
            toUpperCaseNative.release()

            // Call JS function "helloJs()" -> displays "Hello WORLD!"
            helloJs(world)
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

            jsBridge.evaluate<Unit>("global['nativeApi'] = $nativeObjectJs")
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

    interface JsApi: NativeToJsInterface {
        suspend fun calcSum1(a: Int, b: Int): Int
        suspend fun calcSum2(a: Int, b: Int): Int
        fun calcSumDeferred(a: Int, b: Int): Deferred<Int>
        fun setCallback(cb: (payload: JsonObjectWrapper) -> Unit)
        fun triggerEvent()
    }

    @Test
    fun testNativeToJsInterface() {
        runBlocking {
            val jsApi: JsApi = JsValue(jsBridge, """({
                calcSum1: function(a, b) { return a + b; },
                calcSum2: function(a, b) { return new Promise(function(resolve) { resolve(a + b); }); },
                calcSumDeferred: function(a, b) { return a + b; },
                setCallback: function(cb) { this._cb = cb; },
                triggerEvent: function() { if (_cb) cb({ value: 12 }); }
            })""".trimIndent()).mapToNativeObject()

            val sum1 = jsApi.calcSum1(1, 2)
            val sum2 = jsApi.calcSum2(1, 2)
            val sumDeferred = jsApi.calcSumDeferred(1, 2).await()

            println("sum1 is = $sum1")
            println("sum2 is = $sum2")
            println("sumDeferred is = $sumDeferred")

            jsApi.setCallback { payload -> println("Got JS event with payload $payload") }
            jsApi.triggerEvent()
        }
    }

    interface NativeApi: JsToNativeInterface {
        fun calcSum(a: Int, b: Int): Int
        fun setCallback(cb: (payload: JsonObjectWrapper) -> Unit)
        fun triggerEvent()
    }

    @Test
    fun testJsToNativeInterface() {
        val nativeApi = object: NativeApi {
            private var cb: ((JsonObjectWrapper) -> Unit)? = null

            override fun calcSum(a: Int, b: Int) = a + b
            override fun setCallback(cb: (payload: JsonObjectWrapper) -> Unit) {
                this.cb = cb
            }
            override fun triggerEvent() {
                val payload = JsonObjectWrapper("value" to 12)
                cb?.invoke(payload)
            }
        }

        val nativeApiJsValue = JsValue.fromNativeObject(jsBridge, nativeApi)

        jsBridge.evaluateNoRetVal("""
          var result = $nativeApiJsValue.calcSum(1, 2)
          $nativeApi.setCallback(function(payload) {
            console.log("Got native event with value=" + payload.value);
          })
          $nativeApi.triggerEvent()
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
