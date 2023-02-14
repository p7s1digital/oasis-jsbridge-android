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

import org.json.JSONArray
import org.json.JSONObject
import org.json.JSONTokener
import timber.log.Timber

interface Payload {
    companion object {
        fun fromJsonString(jsonString: String?): Payload? {
            if (jsonString.isNullOrEmpty() || jsonString == "null" || jsonString == "undefined") {
                // JSON.stringify(null) == string with "null" contents
                return null
            }

            return try {
                when (val nextValue = JSONTokener(jsonString).nextValue()) {
                    null, JSONObject.NULL -> null
                    is JSONObject -> PayloadObject.fromJsonObject(nextValue)
                    is JSONArray -> PayloadArray.fromJsonArray(nextValue)
                    is String -> PayloadString(nextValue)
                    is Double -> PayloadNumber(nextValue)
                    is Int -> PayloadNumber(nextValue)
                    is Boolean -> PayloadBoolean(nextValue)
                    else -> null
                }
            } catch (e: Exception) {
                Timber.w("WARNING: invalid JSON: $jsonString")
                if (BuildConfig.DEBUG) {
                    throw e
                }
                null
            }
        }
    }

    fun toJsString(orderAlphabetically: Boolean = false): String
    fun toJsonString(orderAlphabetically: Boolean = false): String

    fun booleanValue() = (this as? PayloadBoolean)?.value
    fun stringValue() = (this as? PayloadString)?.value
    fun intValue() = (this as? PayloadNumber)?.value?.toInt()
    fun doubleValue() = (this as? PayloadNumber)?.value?.toDouble()
    fun isNull() = (this as? PayloadNull) != null
}

@JvmInline
value class PayloadBoolean(val value: Boolean): Payload {
    override fun toJsString(orderAlphabetically: Boolean) = if (value) "true" else "false"
    override fun toJsonString(orderAlphabetically: Boolean) = if (value) "true" else "false"
}

@JvmInline
value class PayloadString(val value: String): Payload {
    override fun toJsString(orderAlphabetically: Boolean) = "\"${escape(value)}\""
    override fun toJsonString(orderAlphabetically: Boolean) = toJsString(orderAlphabetically)
}

@JvmInline
value class PayloadNumber(val value: Number): Payload {
    override fun toJsString(orderAlphabetically: Boolean) = "$value"
    override fun toJsonString(orderAlphabetically: Boolean) = "$value"
}

class PayloadNull: Payload {
    override fun toJsString(orderAlphabetically: Boolean) = "null"
    override fun toJsonString(orderAlphabetically: Boolean) = "null"
}


// JsonObjectWrapper extensions:
// ---

fun JsonObjectWrapper.toPayload() = Payload.fromJsonString(this.jsonString)
fun JsonObjectWrapper.toPayloadObject() = PayloadObject.fromJsonString(this.jsonString)
fun JsonObjectWrapper.toPayloadArray() = PayloadArray.fromJsonString(this.jsonString)


// String extensions
// ---

fun String.toPayload() = Payload.fromJsonString(this)
fun String.toPayloadObject() = PayloadObject.fromJsonString(this)
fun String.toPayloadArray() = PayloadArray.fromJsonString(this)


// Helper functions
// ---

internal fun getIntFromAny(value: Any?): Int? {
    // Number -> Int
    (value as? Number?)?.toInt()?.let { return it }

    // String -> Int
    return (value as? String)?.toIntOrNull()
}

internal fun getDoubleFromAny(value: Any?): Double? {
    // Number -> Double
    (value as? Number?)?.toDouble()?.let { return it }

    // String -> Double
    return (value as? String)?.toDoubleOrNull()
}

internal fun jsonValueToPayloadValue(jsonValue: Any?): Any? = when (jsonValue) {
    null, JSONObject.NULL -> null
    is String, is Number, is Boolean -> jsonValue
    is JSONObject -> PayloadObject.fromJsonObject(jsonValue)
    is JSONArray -> PayloadArray.fromJsonArray(jsonValue)
    else -> {
        Timber.w("WARNING: unsupported JSONObject value: $jsonValue!")
        null
    }
}

internal fun valueToJsString(value: Any?): String = when (value) {
    null -> "null"
    is String -> "\"${escape(value)}\""
    is Number -> "$value"
    is Boolean -> "$value"
    is PayloadObject -> value.toJsString()
    is PayloadArray -> value.toJsString()
    else -> {
        throw Exception("Unsupported value: $value")
    }
}

internal fun valueToJsonString(value: Any?): String = when (value) {
    null -> "null"
    is String -> "\"${escape(value)}\""
    is Number -> "$value"
    is Boolean -> "$value"
    is PayloadObject -> value.toJsonString()
    is PayloadArray -> value.toJsonString()
    else -> {
        throw Exception("Unsupported value: $value")
    }
}

private fun escape(s: String): String = s.replace("\"", "\\\"")
