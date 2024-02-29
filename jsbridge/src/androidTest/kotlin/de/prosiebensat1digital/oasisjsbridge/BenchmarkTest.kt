package de.prosiebensat1digital.oasisjsbridge

import android.util.Log
import org.junit.Test
import kotlin.system.measureNanoTime

class BenchmarkTest {


    @Test
    fun profileJsonObjectCreation() {
        val collection = createCollection(size = 1000, properties = 10)
        val newTime = profile(iterations = 20) {
            JsonObjectWrapper(collection)
        }
        val oldTime = profile(iterations = 20) {
            collectionToJsonOld(collection)
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

    @Suppress("UNCHECKED_CAST")
    private fun <T> valueToStringOld(value: T) = when (value) {
        null -> "null"
        JsonObjectWrapper.Undefined -> ""
        is JsonObjectWrapper -> value.jsonString
        is Number -> "$value"
        is Boolean -> if (value) "true" else "false"
        is String -> "\"${escapeOld(value)}\""
        is Array<*> -> JsonObjectWrapper(value).jsonString
        is Collection<*> -> JsonObjectWrapper(value as Collection<Any?>).jsonString
        is Map<*, *> -> JsonObjectWrapper(value as Map<String, Any?>).jsonString
        else -> throw Exception("Unsupported value in JsonStringWrapper ($value)")
    }

    private fun escapeOld(s: String): String = s.replace("\"", "\\\"")
        .replace("\n", "\\n")
        .replace("\t", "\\t")
        .replace("\r", "\\r")


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
