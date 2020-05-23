package de.prosiebensat1digital.oasisjsbridge

import android.util.Log
import okhttp3.OkHttpClient

class JsBridgeConfig
private constructor() {
    companion object {
        @JvmStatic
        fun bareConfig() = JsBridgeConfig()

        @JvmStatic
        fun standardConfig() = JsBridgeConfig().apply {
            setTimeoutConfig.enabled = true
            xhrConfig.enabled = true
            promiseConfig.enabled = true
            consoleConfig.enabled = true
        }
    }

    data class XMLHttpRequestConfig(
        val enabled: Boolean,
        var okHttpClient: OkHttpClient? = null,
        var userAgent: String? = null
    ) {
        companion object {
            val Default = XMLHttpRequestConfig(true)
        }
    }

    data class PromisePolyfillConfig(
        val enabled: Boolean
    ) {
        companion object {
            val Default = PromisePolyfillConfig(!BuildConfig.HAS_BUILTIN_PROMISE)
        }
    }

    data class ConsoleConfig(
        val enabled: Boolean,
        val mode: Mode,
        val appendMessage: (priority: Int, message: String) -> Unit = { priority, message ->
            Log.println(priority, "JavaScript", message)
        }
    ) {
        enum class Mode {
            AsString, AsJson, Empty
        }

        companion object {
            val Default = ConsoleConfig(true, Mode.AsJson, ::defaultAppendMessage)

            fun defaultAppendMessage(priority: Int, message: String) {
                Log.println(priority, "JavaScript", message)
            }
        }
    }

    data class JsDebuggerConfig(
        val enabled: Boolean,
        val port: Int = 9092
    ) {
        companion object {
            val Default = JsDebuggerConfig(true, 9092)
        }
    }

    companion object {
        val Default = JsBridgeConfig(
            SetTimeoutExtensionConfig.Default,
            XMLHttpRequestConfig.Default,
            PromisePolyfillConfig.Default,
            ConsoleConfig.Default,
            JsDebuggerConfig.Default
        )
    }
}
