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
import org.junit.Test
import kotlin.test.*

private fun String.compact() = this
    .replace(" ", "")
    .replace("\n".toRegex(), "")
    .replace("\t".toRegex(), "")

class PayloadObjectTest {

    private val expectedJsonString = """{
        "key1": 123.4,
        "key2": "testString",
        "key3": [1, "two", null],
        "key4": {
            "subKey1": 456.7,
            "subKey2": "anotherTestString"
        },
        "key5": null,
        "key6": 69,
        "key7": true,
        "key8": false
    }""".compact()

    private val expectedJsString = """{
        key1: 123.4,
        key2: "testString",
        key3: [1, "two", null],
        key4: {
            subKey1: 456.7,
            subKey2: "anotherTestString"
        },
        key5: null,
        key6: 69,
        key7: true,
        key8: false
    }""".compact()

    @Test
    fun empty() {
        val subject = PayloadObject()

        assertEquals(subject.keyCount, 0)
        assert(subject.keys.isEmpty())
        assertEquals(subject.toJsString(true), "{}")
        assertEquals(subject.toJsonString(true), "{}")
    }

    @Test
    fun payloadObjectOf() {
        // WHEN
        val subject = payloadObjectOf(
            "key1" to 123.4,
            "key2" to "testString",
            "key3" to payloadArrayOf(1, "two", null),
            "key4" to payloadObjectOf(
                "subKey1" to 456.7,
                "subKey2" to "anotherTestString"
            ),
            "key5" to null,
            "key6" to 69,
            "key7" to true,
            "key8" to false
        )

        // THEN
        assertEquals(subject.toJsonString(true).compact(), expectedJsonString)
        assertEquals(subject.toJsString(true).compact(), expectedJsString)
    }

    @Test
    fun fromValues() {
        // WHEN
        val subject = PayloadObject.fromValues(
            "key1" to 123.4,
            "key2" to "testString",
            "key3" to PayloadArray.fromValues(1, "two", null),
            "key4" to PayloadObject.fromValues(
                "subKey1" to 456.7,
                "subKey2" to "anotherTestString"
            ),
            "key5" to null,
            "key6" to 69,
            "key7" to true,
            "key8" to false
        )

        // THEN
        assertEquals(subject.toJsonString(true).compact(), expectedJsonString)
        assertEquals(subject.toJsString(true).compact(), expectedJsString)
    }

    @Test
    fun fromValuesMap() {
        // WHEN
        val subject = PayloadObject.fromMap(mapOf(
            "key1" to 123.4,
            "key2" to "testString",
            "key3" to arrayOf(1, "two", null),
            "key4" to hashMapOf(
                "subKey1" to 456.7,
                "subKey2" to "anotherTestString"
            ),
            "key5" to null,
            "key6" to 69,
            "key7" to true,
            "key8" to false
        ))

        // THEN
        assertEquals(subject.toJsonString(true).compact(), expectedJsonString)
        assertEquals(subject.toJsString(true).compact(), expectedJsString)
    }

    @Test
    fun fromJSONbject() {
        // GIVEN
        val jsonObject = JSONObject(expectedJsonString)

        // WHEN
        val subject = PayloadObject.fromJsonObject(jsonObject)

        // THEN
        assertEquals(subject?.toJsonString(true)?.compact(), expectedJsonString)
        assertEquals(subject?.toJsString(true)?.compact(), expectedJsString)
    }

    @Test
    fun fromJsonString() {
        // WHEN
        val subject = PayloadObject.fromJsonString(expectedJsonString)

        // THEN
        assertEquals(subject?.toJsonString(true)?.compact(), expectedJsonString)
        assertEquals(subject?.toJsString(true)?.compact(), expectedJsString)
    }

    @Test
    fun toJsonString() {
        // WHEN
        val subject = PayloadObject.fromMap(mapOf(
            "title" to "Welcome to \"Jupiter\"",
            "duration" to 1234
        ))

        // THEN
        assertEquals(
            subject.toJsonString(true),
            """{"duration": 1234, "title": "Welcome to \"Jupiter\""}"""
        )
    }

    @Test
    fun toJsString() {
        // WHEN
        val subject = PayloadObject.fromMap(mapOf(
            "title" to "Welcome to \"Jupiter\"",
            "duration" to 1234
        ))

        // THEN
        assertEquals(
            subject.toJsString(true),
            """{duration: 1234, title: "Welcome to \"Jupiter\""}"""
        )
    }

    @Test
    fun getValues() {
        // WHEN
        val subject = PayloadObject.fromJsonString(expectedJsonString)

        // THEN
        assertNotNull(subject)

        assertEquals(subject.keyCount, 8)
        assertEquals(subject.keys, setOf("key1", "key2", "key3", "key4", "key5", "key6", "key7", "key8"))

        assertEquals(subject.getDouble("key1"), 123.4)
        assertEquals(subject.getString("key2"), "testString")
        assertEquals(subject.getArray("key3")?.toJsString(true)?.compact(), "[1,\"two\",null]")
        assertEquals(subject.getObject("key4")?.toJsString(true)?.compact(), "{subKey1:456.7,subKey2:\"anotherTestString\"}")
        assertTrue(subject.isNull("key5"))
        assertFalse(subject.isUndefined("key5"))
        assertEquals(subject.getInt("key6"), 69)
        assertEquals(subject.getDouble("key6"), 69.0)
        assertEquals(subject.getBoolean("key7"), true)
        assertEquals(subject.getBoolean("key8"), false)

        assertEquals(subject.isUndefined("nonExisting"), true)
        assertEquals(subject.isNull("nonExisting"), false)
        assertNull(subject.getBoolean("notExisting"))
        assertNull(subject.getInt("notExisting"))
        assertNull(subject.getDouble("notExisting"))
        assertNull(subject.getString("notExisting"))
        assertNull(subject.getObject("notExisting"))
        assertNull(subject.getArray("notExisting"))
    }

    @Test
    fun setValues() {
        // WHEN
        val subject = PayloadObject()

        // THEN
        subject.setValue("key1", 123.4)
        subject.setValue("key2", "testString")
        subject.setValue("key3", PayloadArray(4).also { innerArray ->
            innerArray.set(0, 1)
            innerArray.set(1, "two")
            innerArray.set(2, null)
        })
        subject.setValue("key4", PayloadObject().also { innerObject ->
            innerObject.setValue("subKey1", 456.7)
            innerObject.setValue("subKey2", "anotherTestString")
        })
        subject.setValue("key5", null)
        subject.setValue("key6", 69)
        subject.setValue("key7", true)
        subject.setValue("key8", false)
    }

    @Test
    fun getValuesWithPath() {
        // WHEN
        val subject = PayloadObject.fromJsonString("""{
            "key1": "value1",
            "key2": 2,
            "key3": {
                "key3_1": "value3_1",
                "key3_2": {
                    "key3_2_1": "value3_2_1",
                    "key3_2_2": 322,
                    "key3_2_3": 32.3,
                    "key3_2_4": null,
                    "key3_2_5": true,
                    "key3_2_6": false,
                },
                "key3_3": [
                    331,
                    {
                        "key3_3_2_1": "value3_3_2_1",
                        "key3_3_2_2": [33221, "value3_3_2_2_2"]
                    },
                    null,
                    true,
                    false
                ],
                "key3_4": null,
                "key3_5": true,
                "key3_6": false
            },
            "key4": [41, "4_2", 4.3, null, true, false]
        }""")

        // THEN
        assertNotNull(subject)
        assertEquals(subject.getString(arrayOf("key1")), "value1")

        assertEquals(subject.getInt(arrayOf("key2")), 2)

        assertEquals(subject.getString(arrayOf("key3", "key3_1")), "value3_1")
        assertEquals(subject.getObject(arrayOf("key3", "key3_2"))?.keyCount, 6)
        assertEquals(subject.getString(arrayOf("key3", "key3_2", "key3_2_1")), "value3_2_1")
        assertEquals(subject.getInt(arrayOf("key3", "key3_2", "key3_2_2")), 322)
        assertEquals(subject.getDouble(arrayOf("key3", "key3_2", "key3_2_3")), 32.3)
        assertTrue(subject.isNull(arrayOf("key3", "key3_2", "key3_2_4")))
        assertEquals(subject.getBoolean(arrayOf("key3", "key3_2", "key3_2_5")), true)
        assertEquals(subject.getBoolean(arrayOf("key3", "key3_2", "key3_2_6")), false)
        assertEquals(subject.getArray(arrayOf("key3", "key3_3"))?.count, 5)
        assertEquals(subject.getInt(arrayOf("key3", "key3_3", 0)), 331)
        assertEquals(subject.getObject(arrayOf("key3", "key3_3", 1))?.keyCount, 2)
        assertEquals(subject.getString(arrayOf("key3", "key3_3", 1, "key3_3_2_1")), "value3_3_2_1")
        assertEquals(subject.getArray(arrayOf("key3", "key3_3", 1, "key3_3_2_2"))?.count, 2)
        assertEquals(subject.getInt(arrayOf("key3", "key3_3", 1, "key3_3_2_2", 0)), 33221)
        assertEquals(subject.getString(arrayOf("key3", "key3_3", 1, "key3_3_2_2", 1)), "value3_3_2_2_2")
        assertTrue(subject.isNull(arrayOf("key3", "key3_3", 2)))
        assertEquals(subject.getBoolean(arrayOf("key3", "key3_3", 3)), true)
        assertEquals(subject.getBoolean(arrayOf("key3", "key3_3", 4)), false)
        assertTrue(subject.isNull(arrayOf("key3", "key3_4")))
        assertEquals(subject.getBoolean(arrayOf("key3", "key3_5")), true)
        assertEquals(subject.getBoolean(arrayOf("key3", "key3_6")), false)

        assertEquals(subject.getInt(arrayOf("key4", 0)), 41)
        assertEquals(subject.getArray(arrayOf("key4"))?.count, 6)
        assertEquals(subject.getString(arrayOf("key4", 1)), "4_2")
        assertEquals(subject.getDouble(arrayOf("key4", 2)), 4.3)
        assertTrue(subject.isNull(arrayOf("key4", 3)))
        assertEquals(subject.getBoolean(arrayOf("key4", 4)), true)
        assertEquals(subject.getBoolean(arrayOf("key4", 5)), false)

        assertTrue(subject.exists(arrayOf("key1")))
        assertFalse(subject.isNull(arrayOf("key1")))

        assertTrue(subject.exists(arrayOf("key3", "key3_2", "key3_2_4")))
        assertTrue(subject.isNull(arrayOf("key3", "key3_2", "key3_2_4")))

        assertFalse(subject.exists(arrayOf("key3", "key3_2", "nonExisting")))
        assertFalse(subject.isNull(arrayOf("key3", "key3_2", "nonExisting")))
    }

    @Test
    fun escapedString() {
        // WHEN
        val subject = PayloadObject.fromValues(
            "testString" to """This is a "useful" test"""
        )

        // THEN
        assertEquals(subject.getString("testString"), """This is a "useful" test""")
        assertEquals(subject.toJsonString(), """{"testString": "This is a \"useful\" test"}""")
        assertEquals(subject.toJsString(), """{testString: "This is a \"useful\" test"}""")
    }

    @Test
    fun toMap() {
        // WHEN
        val subject = payloadObjectOf(
            "key1" to 123.4,
            "key2" to "testString",
            "key3" to payloadArrayOf(1, "two", null, payloadObjectOf("eins" to 1)),
            "key4" to payloadObjectOf(
                "subKey1" to 456.7,
                "subKey2" to "anotherTestString"
            ),
            "key5" to null,
            "key6" to 69,
            "key7" to true,
            "key8" to false
        )

        // THEN
        val expectedMap = mapOf(
            "key1" to 123.4,
            "key2" to "testString",
            "key3" to listOf(1, "two", null, mapOf("eins" to 1)),
            "key4" to mapOf(
                "subKey1" to 456.7,
                "subKey2" to "anotherTestString"
            ),
            "key5" to null,
            "key6" to 69,
            "key7" to true,
            "key8" to false
        )
        assertEquals(subject.toMap().toSortedMap(), expectedMap.toSortedMap())
    }
}
