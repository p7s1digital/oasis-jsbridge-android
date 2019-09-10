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

import java.util.regex.Pattern
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
        // - Duktape stack trace strings have multiple lines of the format "    at func (file.ext:line)".
        // - QuickJS stack trace strings have multiple lines of the format "    at func (file.ext)".
        // "func" is optional, but we'll omit frames without a function, since it means the frame is in
        // native code.
        private val STACK_TRACE_PATTERN: Pattern
        /** Java StackTraceElements require a class name.  We don't have one in JS, so use this.  */
        private const val STACK_TRACE_CLASS_NAME = "JavaScript"

        init {
            STACK_TRACE_PATTERN = if (BuildConfig.FLAVOR == "duktape")
                Pattern.compile("\\s*at ([^\\s^\\[]+ ) ?\\(?([^\\s:]+):?(\\d+)?\\).*$")
            else if (BuildConfig.FLAVOR == "quickjs")
                Pattern.compile("\\s*at ([^\\s^\\[]+) \\(([^\\s]+):(\\d+)\\).*$");
            else
                throw JsBridgeError.InternalError(customMessage = "Unsupported flavor: ${BuildConfig.FLAVOR}")
        }

        private fun toStackTraceElement(s: String): StackTraceElement? {
            val m = STACK_TRACE_PATTERN.matcher(s)

            if (!m.matches()) {
                // Nothing interesting on this line.
                return null
            }

            val m3 = if (m.groupCount() >= 3) m.group(3) else null
            return StackTraceElement(
                STACK_TRACE_CLASS_NAME, m.group(1) ?: "<unknown func>", m.group(2),
                m3?.toInt() ?: 0
            )
        }
    }
}
