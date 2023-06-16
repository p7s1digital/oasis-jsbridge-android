package de.prosiebensat1digital.oasisjsbridge

import android.util.Log
import okhttp3.OkHttpClient

class JsBridgeConfig
private constructor() {
    companion object {
        /**
         * Creates an instance of JsBridgeConfig without any extensions.
         */
        @JvmStatic
        fun bareConfig() = JsBridgeConfig()

        /**
         * Creates an instance of JsBridgeConfig and enables all extensions.
         * @param localStorageNamespace arbitrary string for separation of local storage between
         * multiple JsBridge instances. If you use the same namespace for multiple instances of
         * JsBridge there might be collisions if identical keys are used to store values.
         */
        @JvmStatic
        fun standardConfig(localStorageNamespace: String) = JsBridgeConfig().apply {
            setTimeoutConfig.enabled = true
            xhrConfig.enabled = true
            promiseConfig.enabled = true
            consoleConfig.enabled = true
            localStorageConfig.apply {
                enabled = true
                namespace = localStorageNamespace
            }
        }
    }

    val setTimeoutConfig = SetTimeoutExtensionConfig()
    val xhrConfig = XMLHttpRequestConfig()
    val promiseConfig = PromiseConfig()
    val consoleConfig = ConsoleConfig()
    val jsDebuggerConfig = JsDebuggerConfig()
    val localStorageConfig = LocalStorageConfig()
    val jvmConfig = JvmConfig()

    class SetTimeoutExtensionConfig {
        var enabled: Boolean = false
    }

    class XMLHttpRequestConfig {
        var enabled: Boolean = false
        var okHttpClient: OkHttpClient? = null
        var userAgent: String? = null
    }

    class PromiseConfig {
        var enabled: Boolean = false
        val needsPolyfill = !BuildConfig.HAS_BUILTIN_PROMISE
    }

    class ConsoleConfig {
        enum class Mode {
            AsString, AsJson, Empty
        }

        var enabled: Boolean = false
        var mode: Mode = Mode.AsString
        var appendMessage: (priority: Int, message: String) -> Unit = { priority, message ->
            Log.println(priority, "JavaScript", message)
        }
    }

    class JsDebuggerConfig {
        var enabled: Boolean = false
        var port: Int = 9092
    }

    class LocalStorageConfig {
        var enabled: Boolean = false

        var namespace: String = ""
    }

    class JvmConfig {
        var customClassLoader: ClassLoader? = null
    }
}
