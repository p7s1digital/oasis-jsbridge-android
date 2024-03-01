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
 (function() {
   var sendNative = XMLHttpRequestExtension_send_native;

 function isListenerWithMatchingName(eventName) {
     return function (listener) {
         return listener.name === eventName;
     };
 }

 function extractHandler(listener) {
     return listener.handler;
 }

 function invokeHandler(eventPayload) {
     return function (handler) {
         handler(eventPayload);
     };
 }

 function emit(eventListeners, eventName, eventPayload) {
     var matchingListeners = eventListeners.filter(isListenerWithMatchingName(eventName)),
         matchingHandlers = matchingListeners.map(extractHandler);

     matchingHandlers.forEach(invokeHandler(eventPayload));
 }

 function noop() {}

 function createProgressEventPayload(options) {
     return {
         type: options.type,
         bubbles: false,
         cancelBubble: false,
         cancelable: false,
         composed: false,
         defaultPrevented: false,
         eventPhase: 0,
         isTrusted: true,
         lengthComputable: false,
         loaded: -1,
         path: [],
         returnValue: true,
         srcElement: options.xhr,
         currentTarget: options.xhr,
         target: options.xhr,
         timeStamp: 0,
         total: -1,
         preventDefault: noop,
         stopImmediatePropagation: noop,
         stopPropagation: noop
     };
 }


 var XMLHttpRequest = function() {
   this._httpMethod = null;
   this._url = null;
   this._requestHeaders = [];
   this._responseHeaders = [];
   this._eventListeners = [];

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
   sendNative(this._httpMethod, this._url, this._requestHeaders, data || null, function(responseInfo, responseText, error) {
     that._send_native_callback(responseInfo, responseText, error);
   });
 };

 XMLHttpRequest.prototype.addEventListener = function(eventName, handler) {
     this._eventListeners.push({ name: eventName, handler: handler });
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
     emit(this._eventListeners, 'abort', createProgressEventPayload({ type: 'abort', xhr: this }));
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
     emit(this._eventListeners, 'error', createProgressEventPayload({ type: 'error', xhr: this }));
   } else if (error) {
     // Error
     console.warn("Got XHR error:", error);
     if (typeof this.onerror === "function") {
       //console.log("Calling onerror()...");
       this.onerror();
     }
     emit(this._eventListeners, 'error', createProgressEventPayload({ type: 'error', xhr: this }));
   } else {
     // Success
     //console.log("XHR success: response =", this.response);
     if (typeof this.onload === "function") {
       //console.log("Calling onload()...");
       this.onload();
     }
     emit(this._eventListeners, 'load', createProgressEventPayload({ type: 'load', xhr: this }));
   }

   if (typeof this.onloadend === "function") {
     //console.log("Calling onloadend()...");
     this.onloadend();
   }
   emit(this._eventListeners, 'loadend', createProgressEventPayload({ type: 'loadend', xhr: this }));
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
   var ret = [];

   for (var i = 0; i < this._responseHeaders.length; i++) {
     var keyValue = this._responseHeaders[i];
     if (keyValue[0] !== name) continue;
     if (keyValue[1] !== "") {
         ret.push(keyValue[1]);
     }
   }

   return ret.join(", ");
 };

 XMLHttpRequest.prototype.overrideMimeType = function() {
   // TODO
 };

 globalThis.XMLHttpRequest = XMLHttpRequest;
 }());