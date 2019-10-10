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

import de.prosiebensat1digital.oasisjsbridge.*
import kotlinx.coroutines.launch
import timber.log.Timber
import kotlin.reflect.typeOf

@UseExperimental(ExperimentalStdlibApi::class)
class PromisePolyfillExtension(private val jsBridge: JsBridge) {
    private val processQueueJsValue: JsValue
    private var onUnhandledPromiseRejectedJsValue: JsValue?
    private var isReady = false

    init {
        onUnhandledPromiseRejectedJsValue = JsValue.fromNativeFunction3(jsBridge) { json: JsonObjectWrapper, message: String, stacktrace: String ->
            val e = JsBridgeError.UnhandledJsPromiseError(JsException(json.jsonString, message, stacktrace, null))
            jsBridge.notifyErrorListeners(e)
        }

        jsBridge.evaluateLocalFile("js/promise.js")

        // Detect unhandled Promise rejections
        jsBridge.evaluateNoRetVal("""
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
        processQueueJsValue = jsBridge.registerJsLambda(jsValue, listOf(typeOf<Unit>()), false)

        // Ensure that isReady remains false while evaluating Polyfill and registering JS lambda
        jsBridge.launch {
            isReady = true
        }
    }

    fun release() {
        onUnhandledPromiseRejectedJsValue = null
    }

    fun processQueue() {
        if (!isReady) {
            return
        }

        try {
            jsBridge.callJsLambdaUnsafe(processQueueJsValue, arrayOf(), false)
        } catch (t: Throwable) {
            val errorMessage = "Error while processing promise queue: ${t.message}"
            val jsException = JsException(detailedMessage = errorMessage, jsStackTrace = null, cause = t)
            jsBridge.notifyErrorListeners(JsBridgeError.UnhandledJsPromiseError(jsException))
        }
    }
}

