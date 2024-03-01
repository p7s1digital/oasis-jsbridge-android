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
package de.prosiebensat1digital.oasisjsbridge.extensions

import android.content.Context
import de.prosiebensat1digital.oasisjsbridge.*
import java.net.SocketTimeoutException
import java.util.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.*
import okhttp3.HttpUrl.Companion.toHttpUrlOrNull
import okhttp3.MediaType.Companion.toMediaTypeOrNull
import okhttp3.RequestBody.Companion.toRequestBody
import timber.log.Timber

internal class XMLHttpRequestExtension(
    context: Context,
    private val jsBridge: JsBridge,
    val config: JsBridgeConfig.XMLHttpRequestConfig
) {
    private var okHttpClient = config.okHttpClient ?: OkHttpClient.Builder().build()

    init {
        // Register XMLHttpRequestNativeHelper_send()
        JsValue.fromNativeFunction5(jsBridge, ::nativeSend)
            .assignToGlobal("XMLHttpRequestExtension_send_native")

        // Evaluate JS file
        val jsCode = context.assets.open("js/xhr.js")
            .bufferedReader()
            .use { it.readText() }
        jsBridge.evaluateUnsync(jsCode)
    }

    fun release() {
        jsBridge.evaluateUnsync("""delete globalThis["XMLHttpRequestExtension_send_native"]""")
    }

    private fun nativeSend(
        httpMethod: String,
        url: String,
        headers: JsonObjectWrapper,
        data: String?,
        cb: (JsonObjectWrapper, String, String) -> Unit
    ) {
        Timber.v("nativeSend($httpMethod, $url, $headers)")

        jsBridge.launch(Dispatchers.IO) {
            // Load URL and evaluate JS string
            var responseInfo: JsonObjectWrapper? = null
            var errorString: String? = null
            var responseText: String? = null
            try {
                // Validate HTTP method
                when (httpMethod.lowercase(Locale.ROOT)) {
                    "get", "post", "put", "delete" -> Unit
                    else -> throw Throwable("Unsupported http method: $httpMethod")
                }

                val requestHeadersBuilder = Headers.Builder()

                // Add each request header (given as [key, value] arrays)
                val headersPayload = headers.toPayload() as? PayloadArray
                if (headersPayload != null) {
                    for (i in 0 until headersPayload.count) {
                        val keyValue = headersPayload.getArray(i)
                        val key = keyValue?.getString(0)
                        val value = keyValue?.getString(1)
                        if (key != null && value != null) {
                            requestHeadersBuilder.add(key, value)
                        } else {
                            Timber.w("Invalid header keyValue: $keyValue")
                        }
                    }
                }

                // Add user agent header if not set
                if (requestHeadersBuilder["user-agent"] == null) {
                    config.userAgent?.let { requestHeadersBuilder.add("User-Agent", it) }
                }
                val requestHeaders = requestHeadersBuilder.build()

                // Request body
                val contentType = requestHeaders["content-type"] ?: ""
                val requestBody = when {
                    /* in specific case when we want to send an empty post request,
                     we should instead send request with body with no data.
                     Otherwise, OkHttp will throw an exception:
                     "Method post must have a request body */
                    data == null && httpMethod.lowercase(Locale.ROOT) == "post" -> {
                        "".toRequestBody(contentType.toMediaTypeOrNull())
                    }
                    else -> {
                        data?.toRequestBody(contentType.toMediaTypeOrNull())
                    }
                }

                Timber.d("Performing XHR request (query: $url)...")

                // Send request via OkHttp
                lateinit var request: Request
                val httpUrl = url.toHttpUrlOrNull() ?: throw Throwable("Cannot parse URL: $url")
                request = Request.Builder()
                    .url(httpUrl)
                    .headers(requestHeaders)
                    .method(httpMethod.uppercase(Locale.ROOT), requestBody)
                    .build()
                val response = okHttpClient.newCall(request).execute()

                // Convert header mutlimap (key -> [value1, value2, ...]) into a list of [key, value] arrays
                val headerKeyValues = response
                    .headers
                    .toMultimap()
                    .flatMap { (key, values) ->
                        values
                            .map { value ->
                                arrayOf(
                                    key,
                                    value.replace("""([^\])"""", """$1\"""")
                                )
                            }
                    }

                responseInfo = JsonObjectWrapper(
                    "statusCode" to response.code,
                    "statusText" to response.message,
                    "responseHeaders" to headerKeyValues.toTypedArray()
                )
                responseText = response.body?.string()

                Timber.d("Successfully fetched XHR response (query: $url)")
                Timber.v("-> responseInfo = $responseInfo")
                Timber.v("-> request headers = $requestHeaders")
            } catch (e: SocketTimeoutException) {
                Timber.d("XHR timeout ($httpMethod $url): $e")
                errorString = "timeout"
            } catch (t: Throwable) {
                Timber.d("XHR error ($httpMethod $url): $t")
                errorString = t.message ?: "unknown XHR error"
            }

            cb(responseInfo ?: JsonObjectWrapper(), responseText ?: "", errorString ?: "")

            withContext(jsBridge.coroutineContext) {
                jsBridge.processPromiseQueue()
            }
        }
    }

}


