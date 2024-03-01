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

import java.lang.IllegalArgumentException

open class JsonObjectWrapper(val jsonString: String) {

    constructor(undefined: Undefined) : this("")
    constructor(vararg pairs: Pair<String, Any?>) : this(pairsToJsonString(pairs))
    constructor(map: Map<String, Any?>) : this(mapToJsonString(map))
    constructor(array: Array<out Any?>) : this(arrayToJsonString(array))
    constructor(collection: Collection<Any?>) : this(collectionToJsonString(collection))

    object Undefined : JsonObjectWrapper("")

    companion object {

        const val SEPARATOR = ","

        private fun mapToJsonString(map: Map<String, Any?>): String {
            val stringBuilder = StringBuilder()
            stringBuilder.append('{')
            var index = 0
            map.forEach { entry ->
                val key = entry.key
                val value = entry.value
                if (value !is Undefined) {
                    stringBuilder.append('"')
                    stringBuilder.append(key)
                    stringBuilder.append('"')
                    stringBuilder.append(':')
                    stringBuilder.append(valueToString(value))
                    if (index != map.size - 1) {
                        stringBuilder.append(SEPARATOR)
                    }
                }
                index++
            }
            stringBuilder.append('}')
            return stringBuilder.toString()
        }

        private fun pairsToJsonString(pairs: Array<out Pair<String, Any?>>): String {
            val stringBuilder = StringBuilder()
            stringBuilder.append('{')
            pairs.forEachIndexed { index, pair ->
                val value = pair.second
                if (value !is Undefined) {
                    stringBuilder.append('"')
                    stringBuilder.append(pair.first)
                    stringBuilder.append("\":")
                    stringBuilder.append(valueToString(value))
                    if (index != pairs.size - 1) {
                        stringBuilder.append(SEPARATOR)
                    }
                }
            }
            stringBuilder.append('}')
            return stringBuilder.toString()
        }

        private fun arrayToJsonString(array: Array<out Any?>): String {
            val stringBuilder = StringBuilder()
            stringBuilder.append('[')
            array.forEachIndexed { index, item ->
                if (item !is Undefined) {
                    stringBuilder.append(valueToString(item))
                    if (index != array.size - 1) {
                        stringBuilder.append(SEPARATOR)
                    }
                }
            }
            stringBuilder.append(']')
            return stringBuilder.toString()
        }

        private fun collectionToJsonString(collection: Collection<Any?>): String {
            val stringBuilder = StringBuilder()
            stringBuilder.append('[')

            collection.forEachIndexed { index, item ->
                if (item !is Undefined) {
                    stringBuilder.append(valueToString(item))
                    if (index != collection.size - 1) {
                        stringBuilder.append(SEPARATOR)
                    }
                }
            }
            stringBuilder.append(']')
            return stringBuilder.toString()
        }

        @Suppress("UNCHECKED_CAST")
        private fun <T> valueToString(value: T) = when (value) {
            null -> "null"
            Undefined -> ""
            is JsonObjectWrapper -> value.jsonString
            is Number -> "$value"
            is Boolean -> if (value) "true" else "false"
            is String -> escape(value)
            is Array<*> -> JsonObjectWrapper(value).jsonString
            is Collection<*> -> JsonObjectWrapper(value as Collection<Any?>).jsonString
            is Map<*, *> -> JsonObjectWrapper(value as Map<String, Any?>).jsonString
            else -> throw IllegalArgumentException("Unsupported value in JsonStringWrapper ($value)")
        }

        private fun escape(string: String): String {
            val builder = StringBuilder()
            builder.append('"')
            string.forEach { char ->
                when (char) {
                    '"' -> builder.append("\\\"")
                    '\n' -> builder.append("\\n")
                    '\t' -> builder.append("\\t")
                    '\r' -> builder.append("\\r")
                    else -> builder.append(char)
                }
            }
            builder.append('"')
            return builder.toString()
        }

    }

    fun toJsString(): String {
        if (jsonString.isBlank()) {
            return "undefined"
        }
        val builder = StringBuilder()
        builder.append("JSON.parse(\"")
        jsonString.forEach { char ->
            when (char) {
                '"' -> builder.append("\\\"")
                '\n' -> builder.append(" ")
                else -> builder.append(char)
            }
        }
        builder.append("\")")
        return builder.toString()
    }

    override fun equals(other: Any?): Boolean {
        if (other !is JsonObjectWrapper) return false
        return jsonString == other.jsonString
    }

    override fun hashCode() = jsonString.hashCode()

    override fun toString() = jsonString


}

