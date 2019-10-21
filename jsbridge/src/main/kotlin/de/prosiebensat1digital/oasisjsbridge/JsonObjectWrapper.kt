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

open class JsonObjectWrapper(val jsonString: String) {

    constructor(undefined: Undefined): this("")
    constructor(vararg pairs: Pair<String, Any?>): this(pairsToJsonString(pairs))
    constructor(map: Map<String, Any?>): this(*map.map { it.toPair() }.toTypedArray())
    constructor(array: Array<out Any?>): this(arrayToJsonString(array))

    object Undefined: JsonObjectWrapper("")

    companion object {
        private fun mapToJsonString(map: Map<String, Any?>): String {
            val inside = map.entries.fold("") { acc, entry ->
                if (entry.value == Undefined) return@fold acc
                val prefix = if (acc.isEmpty()) acc else "$acc, "
                """$prefix"${entry.key}": ${valueToString(entry.value)}"""
            }
            return "{$inside}"
        }

        private fun pairsToJsonString(pairs: Array<out Pair<String, Any?>>): String {
            val inside = pairs.fold("") { acc, pair ->
                if (pair.second == Undefined) return@fold acc
                val prefix = if (acc.isEmpty()) acc else "$acc, "
                """$prefix"${pair.first}": ${valueToString(pair.second)}"""
            }
            return "{$inside}"
        }

        private fun arrayToJsonString(array: Array<out Any?>): String {
            val inside = array.fold("") { acc, item ->
                if (item is Undefined) return@fold acc
                val prefix = if (acc.isEmpty()) acc else "$acc, "
                """$prefix${valueToString(item)}"""
            }
            return "[$inside]"
        }

        @Suppress("UNCHECKED_CAST")
        private fun<T> valueToString(value: T) = when (value) {
            null -> "null"
            Undefined -> ""
            is JsonObjectWrapper -> value.jsonString
            is Number -> "$value"
            is Boolean -> if (value) "true" else "false"
            is String -> "\"$value\""
            is Array<*> -> JsonObjectWrapper(value).jsonString
            is Map<*, *> -> JsonObjectWrapper(value as Map<String, Any?>).jsonString
            else -> throw Exception("Unsupported value in JsonStringWrapper ($value)")
        }
    }

    fun toJsString() = jsonString
        .replace("\"", "\\\"")
        .replace("\n", " ")
        .let { if (it.isEmpty()) "undefined" else "JSON.parse(\"$it\")" }

    override fun equals(other: Any?): Boolean {
        if (other !is JsonObjectWrapper) return false
        return jsonString == other.jsonString
    }

    override fun hashCode() = jsonString.hashCode()

    override fun toString() = jsonString
}
