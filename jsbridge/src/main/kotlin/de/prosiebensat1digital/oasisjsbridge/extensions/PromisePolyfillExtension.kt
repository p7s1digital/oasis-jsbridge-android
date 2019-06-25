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
        onUnhandledPromiseRejectedJsValue = JsValue.fromNativeFunction1(jsBridge) { reason: String ->
            val e = JsBridgeError.UnhandledJsPromiseError(reason, Throwable("Unhandled promise error"))
            jsBridge.notifyErrorListeners(e)
        }

        jsBridge.evaluateLocalFile("js/promise.js")

        // Detect unhandled Promise rejections
        jsBridge.evaluateNoRetVal("""
            Promise.unhandledRejection = function (args) {
                if (args.event === 'reject') {
                    $onUnhandledPromiseRejectedJsValue(args.reason);
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

    fun processQueue(jniJsContext: Long) {
        if (!isReady) {
            return
        }

        try {
            jsBridge.callJsLambdaUnsafe(processQueueJsValue, arrayOf(), false)
        } catch (t: Throwable) {
            Timber.e("Error while processing promise queue: $t")
            jsBridge.notifyErrorListeners(JsBridgeError.JsPromiseError(t))
        }
    }
}

