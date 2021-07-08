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
    private val jsBridge: JsBridge,
    val config: JsBridgeConfig.XMLHttpRequestConfig
) {
    private var okHttpClient = config.okHttpClient ?: OkHttpClient.Builder().build()

    init {
        // Register XMLHttpRequestNativeHelper_send()
        JsValue.fromNativeFunction5(jsBridge, ::nativeSend)
            .assignToGlobal("XMLHttpRequestExtension_send_native")

        // Evaluate JS file
        jsBridge.evaluateNoRetVal(xhrJsCode)
    }

    fun release() {
        jsBridge.evaluateNoRetVal("""delete globalThis["XMLHttpRequestExtension_send_native"]""")
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
                when (httpMethod.toLowerCase(Locale.ROOT)) {
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
                    data == null && httpMethod.toLowerCase(Locale.ROOT) == "post" -> {
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
                    .method(httpMethod.toUpperCase(Locale.ROOT), requestBody)
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

/*
 * Based on bits and pieces from different OSS sources
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
const val xhrJsCode: String = """
var XMLHttpRequest = function() {
  this._send_native = XMLHttpRequestExtension_send_native;

  this._httpMethod = null;
  this._url = null;
  this._requestHeaders = [];
  this._responseHeaders = [];

  this.response = null;
  this.responseText = null;
  this.responseXML = null;
  this.responseType = "";
  this.onreadystatechange = null;
  this.onloadstart = null;
  this.onprogress = null;
  this.onabort = null;
  this.onerror = null;
  this.onload = null;
  this.onloadend = null;
  this.ontimeout = null;
  this.readyState = 0;
  this.status = 0;
  this.statusText = "";
  this.withCredentials = null;
};

// readystate enum
XMLHttpRequest.UNSENT = 0;
XMLHttpRequest.OPENED = 1;
XMLHttpRequest.HEADERS = 2;
XMLHttpRequest.LOADING = 3;
XMLHttpRequest.DONE = 4;

XMLHttpRequest.prototype.constructor = XMLHttpRequest;

XMLHttpRequest.prototype.open = function(httpMethod, url) {
  this._httpMethod = httpMethod;
  this._url = url;

  this.readyState = XMLHttpRequest.OPENED;
  if (typeof this.onreadystatechange === "function") {
    //console.log("Calling onreadystatechange(OPENED)...");
    this.onreadystatechange();
  }
};

XMLHttpRequest.prototype.send = function(data) {
  this.readyState = XMLHttpRequest.LOADING;
  if (typeof this.onreadystatechange === "function") {
    //console.log("Calling onreadystatechange(LOADING)...");
    this.onreadystatechange();
  }

  if (typeof this.onloadstart === "function") {
    //console.log("Calling onloadstart()...");
    this.onloadstart();
  }

  var that = this;
  this._send_native(this._httpMethod, this._url, this._requestHeaders, data || null, function(responseInfo, responseText, error) {
    that._send_native_callback(responseInfo, responseText, error);
  });
};

XMLHttpRequest.prototype.abort = function() {
  this.readyState = XMLHttpRequest.UNSENT;
  // Note: this.onreadystatechange() is not supposed to be called according to the XHR specs
}

// responseInfo: {statusCode, statusText, responseHeaders}
XMLHttpRequest.prototype._send_native_callback = function(responseInfo, responseText, error) {
  //console.log("XMLHttpRequest._send_native_callback");
  //console.log("- responseInfo =", JSON.stringify(responseInfo));
  //console.log("- responseText =", responseText);
  //console.log("- error =", error);

  if (this.readyState === XMLHttpRequest.UNSENT) {
    console.log("XHR native callback ignored because the request has been aborted");
    if (typeof this.onabort === "function") {
      //console.log("Calling onabort()...");
      this.onabort();
    }
    return;
  }

  if (this.readyState != XMLHttpRequest.LOADING) {
    // Request was not expected
    console.log("XHR native callback ignored because the current state is not LOADING");
    return;
  }

  // Response info
  // TODO: responseXML?
  this.responseURL = this._url;
  this.status = responseInfo.statusCode;
  this.statusText = responseInfo.statusText;
  this._responseHeaders = responseInfo.responseHeaders || [];

  this.readyState = XMLHttpRequest.DONE;

  // Response
  this.response = null;
  this.responseText = null;
  this.responseXML = null;
  if (error) {
    this.responseText = error;
  } else {
    this.responseText = responseText;

    switch (this.responseType) {
      case "":
      case "text":
        this.response = this.responseText;
        break;
      case "arraybuffer":
        error = "XHR arraybuffer response is not supported!";
        break;
      case "document":
        this.response = this.responseText;
        this.responseXML = this.responseText;
        break;
      case "json":
        try {
            this.response = JSON.parse(responseText);
        }
        catch (e) {
            error = "Could not parse JSON response: " + responseText;
        }
        break;
      default:
        error = "Unsupported responseType: " + responseInfo.responseType;
    }
  }

  this.readyState = XMLHttpRequest.DONE;
  if (typeof this.onreadystatechange === "function") {
    //console.log("Calling onreadystatechange(DONE)...");
    this.onreadystatechange();
  }

  if (error === "timeout") {
    // Timeout
    console.warn("Got XHR timeout");
    if (typeof this.ontimeout === "function") {
      //console.log("Calling ontimeout()...");
      this.ontimeout();
    }
  } else if (error) {
    // Error
    console.warn("Got XHR error:", error);
    if (typeof this.onerror === "function") {
      //console.log("Calling onerror()...");
      this.onerror();
    }
  } else {
    // Success
    //console.log("XHR success: response =", this.response);
    if (typeof this.onload === "function") {
      //console.log("Calling onload()...");
      this.onload();
    }
  }

  if (typeof this.onloadend === "function") {
    //console.log("Calling onloadend()...");
    this.onloadend();
  }
};

XMLHttpRequest.prototype.setRequestHeader = function(header, value) {
  this._requestHeaders.push([header, value]);
};

XMLHttpRequest.prototype.getAllResponseHeaders = function() {
  var ret = "";

  for (var i = 0; i < this._responseHeaders.length; i++) {
    var keyValue = this._responseHeaders[i];
    ret += keyValue[0] + ": " + keyValue[1] + "\r\n";
  }

  return ret;
};

XMLHttpRequest.prototype.getResponseHeader = function(name) {
  var ret = "";

  for (var i = 0; i < this._responseHeaders.length; i++) {
    var keyValue = this._responseHeaders[i];
    if (keyValue[0] !== name) continue;
    if (ret === "") ret += ", ";
    ret += keyValue[1];
  }

  return ret;
};

XMLHttpRequest.prototype.overrideMimeType = function() {
  // TODO
};

globalThis.XMLHttpRequest = XMLHttpRequest;
"""

