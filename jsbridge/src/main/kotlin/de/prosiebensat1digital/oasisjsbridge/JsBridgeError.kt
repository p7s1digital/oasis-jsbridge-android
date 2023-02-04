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

import kotlin.reflect.KClass

sealed class JsBridgeError(message: String? = null, cause: Throwable?): Exception(message, cause) {

    val jsException: JsException? get() {
        var nextCause = this.cause
        while (nextCause != null) {
            if (nextCause is JsException) return nextCause
            nextCause = nextCause.cause
        }

        return null
    }

    class StartError(cause: Throwable? = null, customMessage: String? = null)
        : JsBridgeError(customMessage ?: "Error while starting the JS interpreter", cause)

    class DestroyError(cause: Throwable? = null, customMessage: String? = null)
        : JsBridgeError(customMessage ?: "Error while destroying the JS interpreter", cause)

    class JsFileEvaluationError(val fileName: String, cause: Throwable? = null, customMessage: String? = null)
        : JsBridgeError(customMessage ?: "Error while evaluating file ($fileName)", cause)

    class JsValueEvaluationError(val jsName: String, cause: Throwable? = null, customMessage: String? = null)
        : JsBridgeError(customMessage ?: "Error while evaluating JS value ($jsName)", cause)

    class JsStringEvaluationError(val js: String, cause: Throwable? = null, customMessage: String? = null)
     : JsBridgeError(customMessage ?: "Error during string evaluation", cause) {
        override fun errorString() = super.errorString() + "\nEvaluated JS code: $js\n---"
    }

    class LoadUrlError(val url: String, cause: Throwable? = null, customMessage: String? = null)
        : JsBridgeError(customMessage ?: "Error while loading URL ($url)", cause)

    class JsToNativeRegistrationError(val type: KClass<*>, cause: Throwable? = null, customMessage: String? = null)
        : JsBridgeError(customMessage ?: "Error while registering native interface ($type)", cause)

    class JsToNativeFunctionRegistrationError(val jsFunction: String, cause: Throwable? = null, customMessage: String? = null)
        : JsBridgeError(customMessage ?: "Error while registering native function ($jsFunction)", cause)

    class JsToNativeCallError(val nativeCall: String, cause: Throwable? = null, customMessage: String? = null)
        : JsBridgeError(customMessage ?: "Error while calling native method $nativeCall", cause)

    class JsToNativeFunctionCallError(val nativeCall: String, cause: Throwable? = null, customMessage: String? = null)
        : JsBridgeError(customMessage ?: "Error while calling native function $nativeCall", cause)

    class NativeToJsInterfaceRegistrationError(val type: KClass<*>, cause: Throwable? = null, customMessage: String? = null)
        : JsBridgeError(customMessage ?: "Error while registering JS interface ($type)", cause)

    class NativeToJsFunctionRegistrationError(val jsFunction: String, cause: Throwable? = null, customMessage: String? = null)
        : JsBridgeError(customMessage ?: "Error while registering JS function ($jsFunction)", cause)

    class NativeToJsCallError(val jsCall: String, cause: Throwable? = null, customMessage: String? = null)
        : JsBridgeError(customMessage ?: "Error while calling JS method $jsCall", cause)

    class JsCallbackError(cause: Throwable? = null): JsBridgeError(cause = cause)
    class UnhandledJsPromiseError(jsException: JsException): JsBridgeError("Unhandled Promise error", cause = jsException)
    class XhrError(val query: String, cause: Throwable? = null): JsBridgeError(cause = cause)

    class InternalError(cause: Throwable? = null, customMessage: String? = null)
        : JsBridgeError(customMessage ?: "Internal JS interpreter error", cause)

    // Generic string for JsBridge errors
    //override fun toString() = errorString()

    open fun errorString(): String {
        val stackTraces = mutableListOf<Array<StackTraceElement>>()

        var current: Throwable? = this
        do {
            stackTraces.add(current!!.stackTrace)
            current = current.cause
        } while (current != null)

        val stackTraceString = stackTraces.fold("") { acc, stackTrace ->
            val s = stackTrace.joinToString("\n") { e ->
                "      at ${e.className}::${e.methodName}(${e.fileName}:${e.lineNumber})"
            }

            if (acc.isEmpty()) s else "$acc\nCause:\n$s"
        }

        return "\n---\n$javaClass:\n$message\n---\nStacktrace:\n$stackTraceString\n---"
    }
}

