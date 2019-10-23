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

import org.json.JSONObject
import org.json.JSONTokener
import timber.log.Timber
import java.util.*
import kotlin.collections.HashSet
import kotlin.reflect.jvm.internal.impl.load.kotlin.JvmType

fun payloadObjectOf(vararg values: Pair<String, Any?>) = PayloadObject.fromValues(*values)

class PayloadObject: Payload {

    private val values = HashMap<String, Any?>()

    companion object {
        fun fromValues(vararg values: Pair<String, Any?>): PayloadObject = fromMap(hashMapOf(*values))

        fun fromMap(values: Map<String, Any?>): PayloadObject {
            val payloadObject = PayloadObject()

            @Suppress("UNCHECKED_CAST")
            val mappedValues = values.mapValues { entry ->
                val value = entry.value
                when {
                    value == null -> null
                    value is Map<*, *> -> fromMap(value as Map<String, Any?>)
                    value is Array<*> -> PayloadArray.fromArray(value as Array<Any?>)
                    value is Collection<*> -> PayloadArray.fromCollection(value as Collection<Any?>)
                    else -> value
                }
            }

            payloadObject.values.putAll(mappedValues)
            return payloadObject
        }

        fun fromJsonObject(jsonObject: JSONObject?): PayloadObject? {
            jsonObject ?: return null

            val payloadObject = PayloadObject()

            jsonObject.keys().forEach { key ->
                payloadObject.values[key] = jsonValueToPayloadValue(jsonObject.get(key))
            }

            return payloadObject
        }

        fun fromJsonString(jsonString: String?): PayloadObject? {
            if (jsonString.isNullOrEmpty() || jsonString == "null" || jsonString == "undefined") {
                // JSON.stringify(null) == string with "null" contents
                return null
            }

            return try {
                when (val nextValue = JSONTokener(jsonString).nextValue()) {
                    null, JSONObject.NULL -> null
                    is JSONObject -> fromJsonObject(nextValue)
                    else -> null
                }
            } catch (e: Exception) {
                if (BuildConfig.DEBUG) {
                    throw e
                }
                Timber.w("WARNING: invalid JSON: $jsonString")
                null
            }
        }
    }

    val keys: Set<String>
        get() = values.keys
    val keyCount get() = values.size

    fun getBoolean(key: String) = values[key] as? Boolean?
    fun getString(key: String) = values[key] as? String?
    fun getObject(key: String) = values[key] as? PayloadObject?
    fun getArray(key: String) = values[key] as? PayloadArray?
    fun isNull(key: String) = values.containsKey(key) && values[key] == null
    fun isUndefined(key: String) = !values.containsKey(key)

    fun getBoolean(path: Array<out Any>) = getValueFromPath(path) as? Boolean?
    fun getString(path: Array<out Any>) = getValueFromPath(path) as? String?
    fun getObject(path: Array<out Any>) = getValueFromPath(path) as? PayloadObject?
    fun getArray(path: Array<out Any>) = getValueFromPath(path) as? PayloadArray?

    fun getInt(key: String): Int? = getIntFromAny(values[key])
    fun getInt(path: Array<out Any>): Int? = getIntFromAny(getValueFromPath(path))

    fun getDouble(key: String): Double? = getDoubleFromAny(values[key])
    fun getDouble(path: Array<out Any>): Double? = getDoubleFromAny(getValueFromPath(path))

    fun setValue(key: String, value: Any?) {
        assert(
            value == null ||
            value is String ||
            value is Boolean ||
            value is Number ||
            value is PayloadObject ||
            value is PayloadArray
        )
        values[key] = value
    }

    fun isNull(path: Array<out Any>): Boolean {
        val value = path.fold<Any /*key*/, Any? /*value*/>(this) { currentPayloadObject, key ->
            when (key) {
                is Int -> {
                    val innerArray = (currentPayloadObject as? PayloadArray)?.array ?: return false  // invalid
                    if (key < 0 || key >= innerArray.size) return false  // invalid
                    innerArray[key]
                }
                is String -> {
                    val innerObject = (currentPayloadObject as? PayloadObject) ?: return false  // invalid
                    if (!innerObject.keys.contains(key)) return false  // invalid
                    innerObject.values[key]
                }
                else -> throw Exception("Invalid path element!")
            }
        }

        return value == null
    }

    fun toMap(): Map<String, Any?> = values.mapValues { entry ->
        val newValue = when (val value = entry.value) {
            is PayloadArray -> value.toList()
            is PayloadObject -> value.toMap()
            else -> value
        }
        newValue
    }

    override fun equals(other: Any?): Boolean {
        if (other !is PayloadObject) return false
        return values == other.values
    }

    override fun hashCode() = values.hashCode()

    fun exists(path: Array<out Any>): Boolean {
        path.fold<Any /*key*/, Any? /*value*/>(this) { currentPayloadObject, key ->
            when (key) {
                is Int -> {
                    val subArray = (currentPayloadObject as? PayloadArray)?.array ?: return false  // invalid
                    if (key < 0 || key >= subArray.size) return false  // invalid
                    subArray[key]
                }
                is String -> {
                    val subObject = (currentPayloadObject as? PayloadObject) ?: return false  // invalid
                    if (!subObject.keys.contains(key)) return false  // invalid
                    subObject.getObject(key)
                }
                else -> throw Exception("Invalid path element!")
            }
        }

        return true
    }

    @Throws
    override fun toJsString(orderAlphabetically: Boolean): String {
        val keys = if (orderAlphabetically) values.keys.sorted() else values.keys

        return "{" + keys.fold("") { acc, key ->
            val valueString = valueToJsString(values[key])
            val prefix = if (acc.isEmpty()) "" else ", "
            "$acc$prefix$key: $valueString"
        } + "}"
    }

    @Throws
    override fun toJsonString(orderAlphabetically: Boolean): String {
        val keys = if (orderAlphabetically) values.keys.sorted() else values.keys

        return "{" + keys.fold("") { acc, key ->
            val valueString = valueToJsonString(values[key])
            val prefix = if (acc.isEmpty()) "" else ", "
            "$acc$prefix\"$key\": $valueString"
        } + "}"
    }

    override fun toString(): String {
        data class StackItem(val key: String?, val value: Any?, val depth: Int)

        // Use a stack for depth-first traversing
        val stack = Stack<StackItem>()
        stack.add(StackItem(null, this, 0))

        val sb = StringBuilder()

        while (!stack.isEmpty()) {
            val (key, value, depth) = stack.pop()

            // Indentation
            sb.append(" ".repeat(depth * 2))

            // key:
            key?.let { sb.append("- $key:") }

            when (value) {
                is PayloadArray -> {
                    // value is an Array, directly write items as JS value string
                    sb.append(
                        value.array.joinToString(
                            prefix = " [",
                            postfix = "]\n",
                            transform = ::valueToJsString
                        )
                    )
                }
                is PayloadObject -> {
                    // value is an Object -> push values (in the reversed order) into the stack
                    if (key != null) sb.append("\n")
                    value.keys.sortedDescending().forEach { subKey ->
                        val subValue = value.values[subKey]
                        stack.add(StackItem(subKey, subValue, depth + 1))
                    }
                }
                else -> {
                    // value is a leaf: print the value
                    sb.append(" ${valueToJsString(value)}\n")
                }

            }
        }

        return sb.toString()
    }


    // Private methods
    // ---

    // Returns the value from its path. The path is composed of key elements of type:
    // - String: the key is the name of a field of an object
    // - Int: the key is the position inside an array
    private fun getValueFromPath(path: Array<out Any>): Any? {
        return path.fold<Any /*key*/, Any? /*value*/>(this) { currentPayloadObject, key ->
            when (key) {
                is Int -> (currentPayloadObject as? PayloadArray)?.array?.getOrNull(key)
                is String -> (currentPayloadObject as? PayloadObject)?.values?.get(key)
                else -> throw Exception("Invalid path element!")
            }
        }
    }
}
