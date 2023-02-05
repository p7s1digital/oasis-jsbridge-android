package de.prosiebensat1digital.oasisjsbridge

class JsToJavaProxy<T: JsToJavaInterface>
@PublishedApi
internal constructor(jsBridge: JsBridge, val obj: T, associatedJsName: String)
: JsValue(jsBridge, null, associatedJsName) {
    internal constructor(jsBridge: JsBridge, obj: T) : this(jsBridge, obj, associatedJsName = generateJsGlobalName())
}
