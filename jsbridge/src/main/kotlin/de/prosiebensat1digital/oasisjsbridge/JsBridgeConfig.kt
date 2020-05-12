package de.prosiebensat1digital.oasisjsbridge

import okhttp3.OkHttpClient

data class JsBridgeConfig(
    val setTimeoutConfig: SetTimeoutExtensionConfig = SetTimeoutExtensionConfig.Default,
    val xmlHttpRequestConfig: XMLHttpRequestConfig = XMLHttpRequestConfig.Default,
    val promisePolyfillConfig: PromisePolyfillConfig = PromisePolyfillConfig.Default,
    val jsDebuggerConfig: JsDebuggerConfig = JsDebuggerConfig.Default
) {
    data class SetTimeoutExtensionConfig(
        val enabled: Boolean
    ) {
        companion object {
            val Default = SetTimeoutExtensionConfig(true)
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

    data class JsDebuggerConfig(
        val enabled: Boolean,
        val port: Int
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
            JsDebuggerConfig.Default
        )
    }
}