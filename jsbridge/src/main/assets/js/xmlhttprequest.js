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
  this._responseHeaders = responseInfo.responseHeaders;

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

global.XMLHttpRequest = XMLHttpRequest;
