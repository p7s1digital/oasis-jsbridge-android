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

import com.jaredrummler.android.device.DeviceName
import de.prosiebensat1digital.oasisjsbridge.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import okhttp3.*
import timber.log.Timber
import java.net.SocketTimeoutException
import java.util.*

class XMLHttpRequestExtension(
    private val jsBridge: JsBridge,
    okHttpClient: OkHttpClient?
) {
    private var okHttpClient = okHttpClient ?: OkHttpClient.Builder().build()

    init {
        // Register XMLHttpRequestNativeHelper_send()
        JsValue.fromNativeFunction5(jsBridge, ::nativeSend)
            .assignToGlobal("XMLHttpRequestExtension_send_native")

        // Evaluate JS file
        jsBridge.evaluateLocalFile("js/xmlhttprequest.js")
    }

    fun release() {
        jsBridge.evaluateNoRetVal("""delete global["XMLHttpRequestExtension_send_native"]""")
    }

    private fun nativeSend(httpMethod: String, url: String, headers: JsonObjectWrapper, data: String?, cb: (JsonObjectWrapper, String, String) -> Unit) {
        Timber.v("nativeSend($httpMethod, $url, $headers)")

        jsBridge.launch(Dispatchers.IO) {
            // Load URL and evaluate JS string
            var responseInfo: JsonObjectWrapper? = null
            var errorString: String? = null
            var responseText: String? = null
            try {
                // Validate HTTP method
                when (httpMethod.toLowerCase(Locale.ROOT)) {
                    "get", "post", "put", "delete" -> Unit
                    else -> throw Throwable("Unsupported http method: $httpMethod")
                }

                // User agent
                val userAgent = withContext(Dispatchers.Default) {
                    val deviceInfo = DeviceName.getDeviceInfo(jsBridge.context)
                    "${deviceInfo.manufacturer} ${deviceInfo.model} Android ${android.os.Build.VERSION.RELEASE}"
                }
                Timber.v("User agent is $userAgent")

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

                // Add user argent header if not set
                if (requestHeadersBuilder.get("user-agent") == null) {
                    requestHeadersBuilder.add("User-Agent", userAgent)
                }
                val requestHeaders = requestHeadersBuilder.build()

                // Request body
                val contentType = requestHeaders["content-type"] ?: ""
                val requestBody = data?.let {
                    RequestBody.create(MediaType.parse(contentType), data)
                }

                // Send request via OkHttp
                lateinit var request: Request
                val httpUrl = HttpUrl.parse(url) ?: throw Throwable("Cannot parse URL: $url")
                request = Request.Builder()
                    .url(httpUrl)
                    .headers(requestHeaders)
                    .method(httpMethod.toUpperCase(Locale.ROOT), requestBody)
                    .build()
                val response = okHttpClient.newCall(request).execute()

                // Convert header mutlimap (key -> [value1, value2, ...]) into a list of [key, value] arrays
                val headerKeyValues = response.headers().toMultimap().flatMap { (key, values) -> values.map { arrayOf(key, it.replace("\"", "\\\"")) } }

                responseInfo = JsonObjectWrapper(
                    "statusCode" to response.code(),
                    "statusText" to response.message(),
                    "responseHeaders" to headerKeyValues.toTypedArray()
                )
                responseText = response.body()?.string()

                Timber.d("Successfully fetched XHR response (query: $url)")
                Timber.v("-> responseInfo = $responseInfo")
                Timber.v("-> request headers = $requestHeaders")
            } catch (e: SocketTimeoutException) {
                JsBridgeError.XhrError("$httpMethod $url", e).let { jsBridge.notifyErrorListeners(it) }
                errorString = "timeout"
            } catch (t: Throwable) {
                JsBridgeError.XhrError("$httpMethod $url", t).let { jsBridge.notifyErrorListeners(it) }
                errorString = t.message ?: "unknown XHR error"
            }

            cb(responseInfo ?: JsonObjectWrapper(), responseText ?: "", errorString ?: "")

            withContext(jsBridge.coroutineContext) {
                jsBridge.processPromiseQueue()
            }
        }
    }
}

