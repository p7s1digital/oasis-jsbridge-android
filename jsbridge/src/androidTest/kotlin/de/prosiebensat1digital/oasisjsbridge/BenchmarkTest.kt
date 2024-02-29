package de.prosiebensat1digital.oasisjsbridge

import android.util.Log
import org.junit.Test
import kotlin.system.measureNanoTime

class BenchmarkTest {


    @Test
    fun profileCollectionJsonObjectCreation() {
        val collection = createCollection(size = 1000, properties = 10)
        val newTime = profile(iterations = 20) {
            JsonObjectWrapper(collection)
        }
        val oldTime = profile(iterations = 20) {
            JsonObjectWrapper(collectionToJsonOld(collection))
        }
        Log.i("BENCHMARK", "Previous: $oldTime ms, New: $newTime ms")
    }

    @Test
    fun profileMapObjectCreation() {
        val map = createMap(depth = 4, properties = 10)
        val newTime = profile(iterations = 20) {
            JsonObjectWrapper(map)
        }
        val oldTime = profile(iterations = 20) {
            JsonObjectWrapper(mapToJsonStringOld(map))
        }
        Log.i("BENCHMARK", "Previous: $oldTime ms, New: $newTime ms")
    }


    private fun profile(iterations: Int, action: () -> Unit): Long {
        val durations = arrayListOf<Long>()
        repeat(iterations) {
            durations.add(measureNanoTime(action))
        }
        durations.sort()
        val median = if (durations.size % 2 == 0) {
            val middleIndex1 = durations.size / 2 - 1
            val middleIndex2 = durations.size / 2
            (durations[middleIndex1] + durations[middleIndex2]) / 2
        } else {
            durations[durations.size / 2]
        }
        // Convert to ms
        return median / 1_000_000L
    }

    private fun collectionToJsonOld(collection: Collection<Any?>): String {
        val inside = collection.fold("") { acc, item ->
            if (item is JsonObjectWrapper.Undefined) return@fold acc
            val prefix = if (acc.isEmpty()) acc else "$acc, "
            """$prefix${valueToStringOld(item)}"""
        }
        return "[$inside]"
    }

    private fun arrayToJsonStringOld(array: Array<out Any?>): String {
        val inside = array.fold("") { acc, item ->
            if (item is JsonObjectWrapper.Undefined) return@fold acc
            val prefix = if (acc.isEmpty()) acc else "$acc, "
            """$prefix${valueToStringOld(item)}"""
        }
        return "[$inside]"
    }

    @Suppress("UNCHECKED_CAST")
    private fun <T> valueToStringOld(value: T) = when (value) {
        null -> "null"
        JsonObjectWrapper.Undefined -> ""
        is JsonObjectWrapper -> value.jsonString
        is Number -> "$value"
        is Boolean -> if (value) "true" else "false"
        is String -> "\"${escapeOld(value)}\""
        is Array<*> -> arrayToJsonStringOld(value)
        is Collection<*> -> collectionToJsonOld(value as Collection<Any?>)
        is Map<*, *> -> mapToJsonStringOld(value as Map<String, Any?>)
        else -> throw Exception("Unsupported value in JsonStringWrapper ($value)")
    }

    private fun escapeOld(s: String): String = s.replace("\"", "\\\"")
        .replace("\n", "\\n")
        .replace("\t", "\\t")
        .replace("\r", "\\r")


    private fun mapToJsonStringOld(map: Map<String, Any?>): String {
        return pairsToJsonStringOld(map.map { it.toPair() }.toTypedArray())
    }

    private fun pairsToJsonStringOld(pairs: Array<out Pair<String, Any?>>): String {
        val inside = pairs.fold("") { acc, pair ->
            if (pair.second == JsonObjectWrapper.Undefined) return@fold acc
            val prefix = if (acc.isEmpty()) acc else "$acc, "
            """$prefix"${pair.first}": ${valueToStringOld(pair.second)}"""
        }
        return "{$inside}"
    }

    private fun createMap(depth: Int, properties: Int = 10): Map<String, Any> {
        val output = linkedMapOf<String, Any>()
        val queue = ArrayDeque<Node>()
        queue.add(Node(properties = output, depth = 0))

        while (queue.isNotEmpty()) {
            val node = queue.removeFirst()
            if (node.depth != depth) {
                repeat(properties) {
                    val childMap = linkedMapOf<String, Any>()
                    node.properties["${generateString(length = 15)}$it"] = childMap
                    queue.add(Node(properties = childMap, depth = node.depth + 1))
                }
            } else {
                repeat(properties) {
                    node.properties["${generateString(length = 15)}$it"] = generateString(10)
                }
            }
        }

        return output
    }

    data class Node(val properties: MutableMap<String, Any>, val depth: Int)

    private fun createCollection(size: Int, properties: Int = 10): List<Any> {
        return List(size) {
            val string = generateString(length = 100)
            val map = mutableMapOf<String, String>()
            repeat(properties) {
                map["property$it"] = string
            }
            map
        }
    }

    private fun generateString(length: Int): String {
        val charArray = CharArray(length) { 'X' }
        return String(charArray)
    }

}
