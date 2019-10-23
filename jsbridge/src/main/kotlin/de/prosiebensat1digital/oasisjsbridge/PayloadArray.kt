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
import org.json.JSONTokener

fun payloadArrayOf(vararg values: Any?) = PayloadArray.fromValues(*values)

class PayloadArray(length: Int): Payload {

    val array = arrayOfNulls<Any?>(length)
    val count get() = array.count()
    val isEmpty get() = array.isEmpty()

    companion object {
        fun fromValues(vararg values: Any?): PayloadArray? = fromArray(arrayOf(*values))

        fun fromArray(values: Array<Any?>): PayloadArray? {
            val payloadArray = PayloadArray(values.size)
            values.forEachIndexed { index, value -> payloadArray.array[index] = value }
            return payloadArray
        }

        fun fromCollection(values: Collection<Any?>): PayloadArray? {
            val payloadArray = PayloadArray(values.size)
            values.forEachIndexed { index, value -> payloadArray.array[index] = value }
            return payloadArray
        }

        fun fromJsonArray(jsonArray: JSONArray?): PayloadArray? {
            jsonArray ?: return null

            val count = jsonArray.length()
            val payloadArray = PayloadArray(count)

            for (i in 0 until count) {
                payloadArray.array[i] = jsonValueToPayloadValue(jsonArray.get(i))
            }

            return payloadArray
        }

        fun fromJsonString(jsonString: String?): PayloadArray? {
            if (jsonString.isNullOrEmpty() || jsonString == "null" || jsonString == "undefined") {
                // JSON.stringify(null) == string with "null" contents
                return null
            }

            return try {
                return when (val nextValue = JSONTokener(jsonString).nextValue()) {
                    null -> null
                    is JSONArray -> fromJsonArray(nextValue)
                    else -> null
                }
            } catch (e: Exception) {
                if (BuildConfig.DEBUG) {
                    throw e
                }
                println("WARNING: invalid JSON: $jsonString")
                null
            }
        }
    }

    fun getString(index: Int) = array.getOrNull(index) as String?
    fun getBoolean(index: Int) = array.getOrNull(index) as Boolean?
    fun getInt(index: Int) = getIntFromAny(array.getOrNull(index))
    fun getDouble(index: Int) = getDoubleFromAny(array.getOrNull(index))
    fun getObject(index: Int) = array.getOrNull(index) as PayloadObject?
    fun getArray(index: Int) = array.getOrNull(index) as PayloadArray?
    fun isNull(index: Int) = index >= 0 && array.size > index && array[index] == null

    fun set(index: Int, value: Any?) {
        assert(index >= 0 && index < array.count())
        assert(
            value == null ||
                value is String ||
                value is Boolean ||
                value is Number ||
                value is Array<*> ||
                value is PayloadObject
        )

        array[index] = value
    }

    fun toList(): List<Any?> = array.map { value ->
        when (value) {
            is PayloadObject -> value.toMap()
            is PayloadArray -> value.toList()
            else -> value
        }
    }

    override fun toJsString(orderAlphabetically: Boolean) = array.joinToString(", ", prefix = "[", postfix = "]", transform = ::valueToJsString)
    override fun toJsonString(orderAlphabetically: Boolean) = array.joinToString(", ", prefix = "[", postfix = "]", transform = ::valueToJsonString)

    override fun equals(other: Any?): Boolean {
        if (other !is PayloadArray) return false
        return other.array.contentEquals(array)
    }

    override fun hashCode() = array.hashCode()
}
