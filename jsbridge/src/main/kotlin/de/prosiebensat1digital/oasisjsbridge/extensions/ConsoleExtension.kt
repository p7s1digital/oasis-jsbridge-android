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

import android.util.Log
import de.prosiebensat1digital.oasisjsbridge.*

// Note: because we use the standard JS->Java conversion for String, there is currently
// no way to distinguish undefined from null. As a result, undefined args will be displayed
// as "null".
// Still missing: trace functionality (currently: alias to debug)
interface StringConsole : JsToNativeInterface {
    fun log(vararg args: String)
    fun debug(vararg args: String)
    fun assert(assertion: Boolean, vararg args: String)
    fun trace(vararg args: String)
    fun info(vararg args: String)
    fun warn(vararg args: String)
    fun err(vararg args: String)
    fun exception(vararg args: String)
}

interface JsonConsole : JsToNativeInterface {
    fun log(vararg args: JsonObjectWrapper)
    fun debug(vararg args: JsonObjectWrapper)
    fun assert(assertion: Boolean, vararg args: JsonObjectWrapper)
    fun trace(vararg args: JsonObjectWrapper)
    fun info(vararg args: JsonObjectWrapper)
    fun warn(vararg args: JsonObjectWrapper)
    fun err(vararg args: JsonObjectWrapper)
    fun exception(vararg args: JsonObjectWrapper)
}

class ConsoleExtension(
    private val jsBridge: JsBridge,
    val config: JsBridgeConfig.ConsoleConfig
) {
    private val consoleJsValue: JsValue

    init {
        consoleJsValue = when (config.mode) {
            JsBridgeConfig.ConsoleConfig.Mode.AsString -> createStringConsole()
            JsBridgeConfig.ConsoleConfig.Mode.AsJson -> createJsonConsole()
        }

        consoleJsValue.assignToGlobal("console")
    }

    private fun createStringConsole(): JsValue {
        val consoleNativeObject = object: StringConsole {
            override fun log(vararg args: String) = message(Log.DEBUG, args)
            override fun debug(vararg args: String) = message(Log.DEBUG, args)
            override fun trace(vararg args: String) = message(Log.DEBUG, args)
            override fun info(vararg args: String) = message(Log.INFO, args)
            override fun warn(vararg args: String) = message(Log.WARN, args)
            override fun err(vararg args: String) = message(Log.ERROR, args)
            override fun exception(vararg args: String) = message(Log.ERROR, args)

            override fun assert(assertion: Boolean, vararg args: String) {
                if (!assertion) {
                    message(Log.ASSERT, arrayOf("Assertion failed:", *args))
                }
            }
        }

        return JsValue.fromNativeObject(jsBridge, consoleNativeObject)
    }

    private fun createJsonConsole(): JsValue {
        val consoleNativeObject = object: JsonConsole {
            override fun log(vararg args: JsonObjectWrapper) = message(Log.DEBUG, j2s(args))
            override fun debug(vararg args: JsonObjectWrapper) = message(Log.DEBUG, j2s(args))
            override fun trace(vararg args: JsonObjectWrapper) = message(Log.DEBUG, j2s(args))
            override fun info(vararg args: JsonObjectWrapper) = message(Log.INFO, j2s(args))
            override fun warn(vararg args: JsonObjectWrapper) = message(Log.WARN, j2s(args))
            override fun err(vararg args: JsonObjectWrapper) = message(Log.ERROR, j2s(args))
            override fun exception(vararg args: JsonObjectWrapper) = message(Log.ERROR, j2s(args))

            override fun assert(assertion: Boolean, vararg args: JsonObjectWrapper) {
                if (!assertion) {
                    message(Log.ASSERT, arrayOf("Assertion failed:", *j2s(args)))
                }
            }
        }

        return JsValue.fromNativeObject(jsBridge, consoleNativeObject)
    }

    private fun j2s(json: Array<out JsonObjectWrapper>): Array<out String> {
        return json.map {
            val jsonString = if (it == JsonObjectWrapper.Undefined) "undefined" else it.toString()
            jsonString.replace("^\"(.*)\"$".toRegex(), "$1")  // remove quotes of simple strings
        }.toTypedArray()
    }

    private fun message(priority: Int, args: Array<out String>) {
        val message = args.joinToString(" ")
        config.appendMessage(priority, message)
    }
}

