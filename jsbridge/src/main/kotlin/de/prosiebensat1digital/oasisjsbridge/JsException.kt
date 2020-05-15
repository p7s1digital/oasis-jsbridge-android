/*
 * Copyright (C) 2019 ProSiebenSat1.Digital GmbH.
 *
 * Originally based on Duktape Android:
 * Copyright (C) 2015 Square, Inc.
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

import timber.log.Timber

@Suppress("UNUSED")  // Called from JNI
class JsException(val jsonValue: String? = null, detailedMessage: String, jsStackTrace: String?, cause: Throwable?) : RuntimeException(detailedMessage, cause) {

    init {
        Timber.v("JsException() - detailedMessage = $detailedMessage")
        Timber.v("JsException() - jsStackTrace = $jsStackTrace")

        // Parses `StackTraceElement`s from `jsStackTrace` and prepend them to the Java stack trace
        stackTrace = jsStackTrace.orEmpty().split('\n')
            .mapNotNull(::toStackTraceElement)
            .plus(stackTrace)
            .toTypedArray()
    }

    companion object {
        // e.g.: "   at [methodName] (file.js:13)"
        private val STACK_TRACE_METHOD_FILE: Regex = "\\s*at\\s+\\[([^\\]\\s]+)\\]\\s+\\(([^:\\s\\)]+):(\\d+)\\).*".toRegex()

        // e.g.: "   at functionName (file.js:13)"
        private val STACK_TRACE_FUNCTION_FILE: Regex = "\\s*at\\s+([^\\]\\s]+)\\s+\\(([^:\\s\\)]+):(\\d+)\\).*".toRegex()

        // e.g.: "   at methodName (object3)"
        private val STACK_TRACE_METHOD_OBJECT: Regex = "\\s*at\\s+([^\\]\\s]+)\\s+\\(([^\\s\\)]+)\\).*".toRegex()

        // e.g.: "   at file.js:13"
        private val STACK_TRACE_FILE: Regex = "\\s*at\\s+([^:\\s]+):([^\\s]+).*".toRegex()

        // Java StackTraceElements require a class name.  We don't have one in JS, so use this.
        private const val STACK_TRACE_CLASS_NAME = "JavaScript"

        private fun toStackTraceElement(s: String): StackTraceElement? {
            var className: String? = null
            val methodName: String
            val fileName: String
            val lineNumber: Int

            val methodFileGroups by lazy { STACK_TRACE_METHOD_FILE.matchEntire(s)?.groups }
            val functionFileGroups by lazy { STACK_TRACE_FUNCTION_FILE.matchEntire(s)?.groups }
            val methodObjectGroups by lazy { STACK_TRACE_METHOD_OBJECT.matchEntire(s)?.groups }
            val fileGroups by lazy { STACK_TRACE_FILE.matchEntire(s)?.groups }

            if (methodFileGroups != null) {
                val groups = methodFileGroups!!
                methodName = groups[1]?.value ?: "<unknown method>"
                fileName = groups[2]?.value ?: "<unknown file>"
                lineNumber = groups[3]?.value?.toIntOrNull() ?: 0
            } else if (functionFileGroups != null) {
                val groups = functionFileGroups!!
                methodName = groups[1]?.value ?: "<unknown func>"
                fileName = groups[2]?.value ?: "<unknown file>"
                lineNumber = groups[3]?.value?.toIntOrNull() ?: 0
            } else if (methodObjectGroups != null) {
                val groups = methodObjectGroups!!
                methodName = groups[1]?.value ?: "<unknown func>"
                className = groups[2]?.value ?: "<unknown file>"
                fileName = "eval"
                lineNumber = 0
            } else if (fileGroups != null) {
                val groups = fileGroups!!
                methodName = "global"
                fileName = groups[1]?.value ?: return null
                lineNumber = groups[2]?.value?.toIntOrNull() ?: 0
            } else {
                return null
            }

            if (fileName.endsWith(".cpp") || fileName.endsWith(".c")) {
                // Internal error
                return null
            }

            val stackTraceClassName = if (className == null) STACK_TRACE_CLASS_NAME else "$STACK_TRACE_CLASS_NAME/$className"
            return StackTraceElement(stackTraceClassName, methodName, fileName, lineNumber)
        }
    }
}
