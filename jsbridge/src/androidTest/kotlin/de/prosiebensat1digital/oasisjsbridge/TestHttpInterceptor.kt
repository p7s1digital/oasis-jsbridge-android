package de.prosiebensat1digital.oasisjsbridge

import okhttp3.*
import org.json.JSONObject

class TestHttpInterceptor : Interceptor {

    private val urlResponseMocks = mutableMapOf<HttpUrl, Response>()

    fun mockRequest(url: String, responseText: String, responseHeadersJson: String? = "{}") {
        val request = Request.Builder().url(url).build()
        val protocol = Protocol.HTTP_1_1
        val responseBody = ResponseBody.create(MediaType.parse("application/json"), responseText)
        val responseHeaders = Headers.Builder().also { headersBuilder ->
            JSONObject(responseHeadersJson).let { headersObject ->
                headersObject.keys().forEach { key ->
                    headersBuilder.add(key, headersObject.getString(key))
                }
            }
        }.build()
        val response = Response.Builder().request(request).headers(responseHeaders).body(responseBody).protocol(protocol).code(200).message("OK").build()
        val httpUrl = HttpUrl.parse(url)!!

        urlResponseMocks[httpUrl] = response
    }

    override fun intercept(chain: Interceptor.Chain): Response {

        val request = chain.request()
        val response = urlResponseMocks[request.url()]

        return response ?: chain.proceed(request)
    }
}
