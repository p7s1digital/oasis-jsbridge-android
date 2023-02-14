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

import okhttp3.*
import okhttp3.HttpUrl.Companion.toHttpUrlOrNull
import okhttp3.MediaType.Companion.toMediaTypeOrNull
import okhttp3.ResponseBody.Companion.toResponseBody
import org.json.JSONObject
import kotlin.time.Duration

class TestHttpInterceptor : Interceptor {

    private val urlResponseMocks = mutableMapOf<HttpUrl, Response>()
    private val urlResponseDelayMsecs = mutableMapOf<HttpUrl, Long?>()

    fun mockRequest(url: String, responseText: String, responseHeadersJson: String = "{}", delayMsecs: Long? = null) {
        val request = Request.Builder().url(url).build()
        val protocol = Protocol.HTTP_1_1
        val responseBody = responseText.toResponseBody("application/json".toMediaTypeOrNull())
        val responseHeaders = Headers.Builder().also { headersBuilder ->
            JSONObject(responseHeadersJson).let { headersObject ->
                headersObject.keys().forEach { key ->
                    headersBuilder.add(key, headersObject.getString(key))
                }
            }
        }.build()
        val response = Response.Builder().request(request).headers(responseHeaders).body(responseBody).protocol(protocol).code(200).message("OK").build()
        val httpUrl = url.toHttpUrlOrNull()!!

        urlResponseMocks[httpUrl] = response
        delayMsecs?.let { urlResponseDelayMsecs[httpUrl] = it }
    }

    override fun intercept(chain: Interceptor.Chain): Response {

        val request = chain.request()
        val response = urlResponseMocks[request.url]

        urlResponseDelayMsecs[request.url]?.let { Thread.sleep(it) }

        return response ?: chain.proceed(request)
    }
}
