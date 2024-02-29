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
package de.prosiebensat1digital.oasisjsbridge.extensions

import android.content.Context
import de.prosiebensat1digital.oasisjsbridge.*
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.async
import kotlin.reflect.typeOf

internal class PromiseExtension(
    private val context: Context,
    private val jsBridge: JsBridge,
    val config: JsBridgeConfig.PromiseConfig
) {
    private var processPolyfillQueueJsValue: Deferred<JsValue>? = null
    private var onUnhandledPromiseRejectedJsValue: JsValue? = null

    init {
        if (config.needsPolyfill) {
            setUpPolyfill()
        }
    }

    private fun setUpPolyfill() {
        onUnhandledPromiseRejectedJsValue =
            JsValue.createJsToJavaProxyFunction3(jsBridge) { json: JsonObjectWrapper, message: String, stacktrace: String ->
                val e = JsBridgeError.UnhandledJsPromiseError(
                    JsException(
                        json.jsonString,
                        message,
                        stacktrace,
                        null
                    )
                )
                jsBridge.notifyErrorListeners(e)
            }

        val promiseJsCode = context.assets.open("js/promises.js")
            .bufferedReader()
            .use { it.readText() }

        jsBridge.evaluateUnsync(promiseJsCode)

        // Detect unhandled Promise rejections
        jsBridge.evaluateUnsync(
            """
            Promise.unhandledRejection = function (args) {
              if (args.event === 'reject') {
                var value = args.reason;
                var msg;
                var stack;
                if (value instanceof Error) {
                  msg = value.message;
                  json = Object.getOwnPropertyNames(value).reduce(function(acc, key) {
                    acc[key] = value[key];
                    return acc;
                  }, {});
                  stack = value.stack;
                } else {
                  msg = value;
                  json = JSON.stringify(value);
                }
                $onUnhandledPromiseRejectedJsValue(json, msg, stack);
              } else if (args.event === 'handle') {
                //console.log('Previous unhandled rejection got handled:', args.reason);
              }
            };""".trimIndent()
        )

        val jsValue = JsValue.newFunction(jsBridge, "Promise.runQueue();")
        processPolyfillQueueJsValue = jsBridge.async {
            jsBridge.registerJsLambda(jsValue, listOf(typeOf<Unit>()), true)
        }
    }

    fun release() {
        onUnhandledPromiseRejectedJsValue = null
    }

    @OptIn(ExperimentalCoroutinesApi::class)
    fun processPolyfillQueue() {
        val processQueueJsValue = processPolyfillQueueJsValue ?: return
        if (!processQueueJsValue.isCompleted) {
            return
        }

        try {
            jsBridge.callJsLambdaUnsafe(processQueueJsValue.getCompleted(), arrayOf(), false)
        } catch (t: Throwable) {
            val errorMessage = "Error while processing promise queue: ${t.message}"
            val jsException =
                JsException(detailedMessage = errorMessage, jsStackTrace = null, cause = t)
            jsBridge.notifyErrorListeners(JsBridgeError.UnhandledJsPromiseError(jsException))
        }
    }
}
