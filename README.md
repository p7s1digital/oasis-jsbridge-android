🏝 Oasis JsBridge
=================

![jitpack](https://jitpack.io/v/p7s1digital/oasis-jsbridge-android.svg)
![build](https://github.com/p7s1digital/oasis-jsbridge-android/workflows/build/badge.svg)

Evaluate JavaScript code and map values, objects and functions between Kotlin/Java and JavaScript on Android.

```kotlin
val jsBridge = JsBridge(JsBridgeConfig.bareConfig())
val msg: String = jsBridge.evaluate("'Hello world!'.toUpperCase()")
println(msg)  // HELLO WORLD!
```

Powered by:
- [Duktape][duktape] (ES5 + partially ES6) or
- [QuickJS][quickjs] (ES2020)


## Features

 * evaluate JavaScript code from Kotlin/Java
 * map values, objects and functions between Kotlin/Java and JavaScript
 * propagate exceptions between JavaScript and Kotlin/Java (including stack trace)
 * non-blocking API (via coroutines)
 * support for suspending functions and JavaScript promises
 * support for AIDL interfaces and parcelable (experimental)
 * extensions (optional): console, setTimeout/setInterval, XmlHttpRequest, Promise, JS debugger

See [Example](#example-consuming-a-js-api-from-kotlin).


## Installation

Add jitpack repository (root gradle):
```
allprojects {
    repositories {
        ...
        maven { url 'https://jitpack.io' }
    }
}
```

Add jsbridge dependency (module gradle):
```
implementation "de.prosiebensat1digital.oasis-jsbridge-android:oasis-jsbridge-quickjs:<version>"  // QuickJS flavor
// OR: implementation "de.prosiebensat1digital.oasis-jsbridge-android:oasis-jsbridge-quickjs:<version>"  // Duktape flavor
```


## Usage

1. [Evaluate JS code](#evaluating-js-code)
1. [Reference any JS value](#jsvalue)
1. [Map JS objects to Kotlin/Java](#using-js-objects-from-kotlinjava)
1. [Map Kotlin/Java objects to JS](#using-kotlinjava-objects-from-js)
1. [Map JS functions to Kotlin](#calling-js-functions-from-kotlin)
1. [Map Kotlin functions to JS](#calling-kotlin-functions-from-js)
1. [Map JS objects to AIDL](#using-aidl-from-js)
1. [Extensions](#extensions)


### Evaluating JS code

Unsync calls (will be evaluated synchronously):
```kotlin
jsBridge.evaluateUnsync("console.log('hello');")
jsBridge.evaluateFileContentUnsync("console.log('hello')", "js/test.js")
```

Suspending calls:
```kotlin
// Without return value:
jsBridge.evaluate<Unit>("console.log('hello');")
jsBridge.evaluateFileContent("console.log('hello')", "js/test.js")

// With return value:
val sum: Int = jsBridge.evaluate("1+2")  // suspending call
val sum: Int = jsBridge.evaluate("new Promise(function(resolve) { resolve(1+2); })")  // suspending call (JS promise)
val msg: String = jsBridge.evaluate("'message'.toUpperCase()")  // suspending call
val obj: JsonObjectWrapper = jsBridge.evaluate("({one: 1, two: 'two'})")  // suspending call (wrapped JS object via JSON)
```

Blocking evaluation:
```kotlin
val result1: Int = jsBridge.evaluateBlocking("1+2")  // generic type inferred
val result2 = jsBridge.evaluateBlocking<Int>("1+2")  // with explicit generic type
```

From Java (blocking):
```java
Integer sum = (Integer) jsBridge.evaluateBlocking("1+2", Integer.class);
```

Exception handling:
```kotlin
try {
    jsBridge.evaluate<Unit>("""
        |function buggy() { throw new Error('wrong') }
        |buggy();
    """.trimMargin())
} catch (jse: JsException) {
    // jse.message = "wrong"
    // jse.stackTrace = [JavaScript.buggy(eval:1), JavaScript.<eval>(eval:2), ....JsBridge.jniEvaluateString(Native Method), ...]
}
```

Note: the JS code is evaluated in a dedicated "JS" thread.


### JsValue

A JsValue is a reference to any JS value.

```kotlin
val jsInt = JsValue(jsBridge, "123")
val jsInt = JsValue.fromNativeValue(jsBridge, 123)
val jsString = JsValue(jsBridge, "'hello'.toUpperCase()")
val jsString = JsValue.fromNativeValue(jsBridge, "HELLO")
val jsObject = JsValue(jsBridge, "({one: 1, two: 'two'})")
val jsObject = JsValue.fromNativeValue(jsBridge, JsonObjectWrapper("one" to 1, "two" to "two"))
val calcSumJs = JsValue(jsBridge, "(function(a, b) { return a + b; })")
val calcSumJs = JsValue.newFunction(jsBridge, "a", "b", "return a + b;")
val calcSumJs = JsValue.fromNativeFunction2(jsBridge) { a: Int, b: Int -> a + b }
```

It has an associated (global) JS variable whose name can be accessed via `toString()` which makes it easy to re-use it from JS code:<br/>
```kotlin
val sum: Int = jsBridge.evaluate("$calcSumJs(2, 3)")
```

The scope of a JsValue is defined by JVM. In other words, the associated global
variable in JavaScript will be avalaible as long as the JsValue instance is not  
garbage-collected.

Evaluating a JsValue:
```kotlin
val i = jsInt.evaluate<Int>()  // suspending function + explicit generic parameter
val i: Int = jsInt.evaluate()  // suspending function + inferred generic parameter
val i: Int = jsString.evaluateBlocking()  // blocking
val s: String = jsString.evaluate()
val o: JsonObjectWrapper = jsObject.evaluate()
```

From Java (blocking):
```java
String s = (String) jsString.evaluateBlocking(String.class);
```

Additionally, a JS (proxy) value can be created from:
- [a Kotlin/Java object](#using-kotlinjava-objects-from-js) via `JsValue.fromNativeObject()`.
- [a Kotlin function](#calling-kotlin-functions-from-js) via `JsValue.fromNativeFunction()`.

A JS value can be mapped to:
- [a Kotlin/Java proxy object](#using-js-objects-from-kotlinjava) via `JsValue.mapToNativeObject()`.
- [a Kotlin proxy function](#calling-js-functions-from-kotlin) via `JsValue.mapToNativeFunction()`.


### Using JS objects from Kotlin/Java

An interface extending `NativeToJsInterface` must be defined with the methods of the
JS object:

```kotlin
interface JsApi : NativeToJsInterface {
    fun method1(a: Int, b: String)
    suspend fun method2(c: Double): String
}

val jsObject = JsValue(jsBridge, """({
  method1: function(a, b) { /*...*/ },
  method2: function(c) { return "Value: " + c; }
})""")

// Create a native proxy to the JS object
val jsApi: JsApi = jsObject.mapToNativeObject()  // no check
val jsApi: JsApi = jsObject.mapToNativeObject(check = true)  // suspending, optionally check that all methods are defined in the JS object
val jsApi: JsApi = jsObject.mapToNativeObjectBlocking(check = true)  // blocking (with optional check)

// Call JS methods from native
jsApi.method1(1, "two")
val s = jsApi.method2(3.456)  // suspending
```

See [Example](#example-consuming-a-js-api-from-kotlin).

Note: when calling a non-suspending method with return value, the caller thread will be blocked until the result has been returned.


### Using Kotlin/Java objects from JS

An interface extending `JsToNativeInterface` must be defined with the methods of the
native object:

```kotlin
interface NativeApi : JsToNativeInterface {
    fun method(a: Int, b: String): Double
}

val obj = object : NativeApi {
    override fun method(a: Int, b: String): Double { ... }
}

// Create a JS proxy to the native object
val nativeApi: JsValue = JsValue.fromNativeObject(jsBridge, obj)

// Call native method from JS
jsBridge.evaluateNoRetVal("globalThis.x = $nativeApi.method(1, 'two');")
```

See [Example](#example-consuming-a-js-api-from-kotlin).

Note: the native methods are called from the "JS" thread and must properly
manage the execution context (e.g.: going to the main thread when calling
UI methods). To avoid blocking the JS thread for asynchronous operations, it
is possible to return a Deferred.


### Calling JS functions from Kotlin

```kotlin
val calcSumJs: suspend (Int, Int) -> Int = JsValue
    .newFunction(jsBridge, "a", "b", "return a + b;")
    .mapToNativeFunction2()

println("Sum is $calcSumJs(1, 2)")
```

Available methods:
 * `JsValue.mapToNativeFunctionX()` (where X is the number of arguments)
 * `JsValue.mapToNativeBlockingFunctionX()`: blocks the current thread until the JS code has been evaluated


### Calling Kotlin functions from JS

```kotlin
val calcSumNative = JsValue.fromNativeFunction2(jsBridge) { a: Int, b: Int -> a + b }

jsBridge.evaluateNoRetVal("console.log('Sum is', $calcSumNative(1, 2))");
```

Note: the native function is triggered from the "JS" thread


### Using AIDL from JS

Note: this feature is still experimental!

It is also possible to register AIDL interfaces and call AIDL methods, send and receive
parcelable values, pass AIDL interfaces as parameter.

To map an AIDL interface to JS:

```kotlin
// Register existing AIDL interface
val aidlJsValue = JsValue.fromAidlInterface(jsBridge, aidlInterface)

// Call an AIDL method from JS
jsBridge.evaluateNoRetVal("""$aidlJsValue.aidlMethod({parcelableField1: 1, parcelableField2: "two"});""")
```

### Extensions

Extensions can be enabled/disabled via the JsBridgeConfig given to the JsBridge constructor.

- **setTimeout/setInterval(cb, interval):**<br/>
Trigger JS callback using coroutines.delay internally.

- **console.log(), .warn(), ...:**<br/>
Append output to the logcat (or to a custom block). Parameters are displayed either via string
conversion or via JSON serialization. JSON serialization provides much more detailed output
(including objects and Error instances) but is slower than the string variant (which displays
objects as "[object Object]").

- **XMLHtmlRequest (XHR):**<br/>
Support for XmlHttpRequest network requests using `okhttp` client internally. The `okhttp` instance
can be injected in the `JsBridgeConfig` object.
_Note: not all HTTP methods are currently implemented, check the source code for details._
Other network clients are not tested but should work as well (polyfill for
[fetch](https://www.npmjs.com/package/whatwg-fetch),
[axios](https://github.com/axios/axios#features) uses XHR in browser mode)

- **Promise:**<br/>
Support for ES6 promises (Duktape: via polyfill, QuickJS: built-in). Pending jobs are triggered
after each evaluation.

- **JS Debugger:**<br/>
JS debugger support (Duktape only via Visual Studio Code plugin)


## Supported types

| Kotlin              | Java                 | JS         | Note
| ------------------- | -------------------- | ---------- | ---
| `Boolean`           | `boolean`, `Boolean` | `number`   |
| `Byte`              | `byte`, `Byte`       | `number`   |
| `Int`               | `int`, `Integer`     | `number`   |
| `Float`             | `float`, `Float`     | `number`   |
| `Double`            | `double`, `Double`   | `number`   |
| `String`            | `String`             | `string`   |
| `BooleanArray`      | `boolean[]`          | `Array`    |
| `ByteArray`         | `byte[]`             | `Array`    |
| `IntArray`          | `int[]`              | `Array`    |
| `FloatArray`        | `float[]`            | `Array`    |
| `DoubleArray`       | `double[]`           | `Array`    |
| `Array<T: Any>`     | `T[]`                | `Array`    | T must be a supported type
| `Function<R>`       | n.a.                 | `function` | lambda with supported types
| `Deferred<T>`       | n.a.                 | `Promise`  | T must be a supported type
| `JsonObjectWrapper` | `JsonObjectWrapper`  | `object`   | serializes JS objects via JSON
| `JsValue`           | `JsValue`            | any        | references any JS value


## Example: consuming a JS API from Kotlin

 JavaScript <=> Kotlin API:
```kotlin
interface JsApi : NativeToJsInterface {
    suspend fun createMessage(): String
    suspend fun calcSum(a: Int, b: Int): Int
}

interface NativeApi : JsToNativeInterface {
    fun getPlatformName(): String
    fun getTemperatureCelcius(): Deferred<Float>
}
```

JavaScript API (js/api.js):
<details>
    <summary>ES5</summary>

```js
globalThis.createApi = function(nativeApi, config) {
  return {
    createMessage: function() {
      const platformName = nativeApi.getPlatformName();
      return nativeApi.getTemperatureCelcius().then(function(celcius) {
        const value = config.useFahrenheit ? celcius * x + c : celcius;
        const unit = config.useFahrenheit ? "degrees F" : "degrees C";
        return "Hello " + platformName + "! The temperature is " + value + " " + unit + ".";
      });
    },
    calcSum: function(a, b) {
      return new Promise(function(resolve) { resolve(a + b); });
    }
  };
};
```
</details>
<details>
    <summary>ES6</summary>

```js
globalThis.createApi = (nativeApi, config) => {(
  createMessage: async () => {
    const platformName = nativeApi.getPlatformName();
    const celcius = await nativeApi.getTemperatureCelcius();
    const value = config.useFahrenheit ? celcius * x + c : celcius;
    const unit = config.useFahrenheit ? "degrees F" : "degrees C";
    return `Hello ${platformName}! The temperature is ${value} ${unit}.`;
  },
  calcSum: async (a, b) => a + b
});
```
</details>

Kotlin API:
```kotlin
val nativeApi = object: NativeApi {
    override fun getPlatformName() = "Android"
    override fun getTemperatureCelcius() = async {
        // Getting current temperature from sensor or via network service
        37.2f
    }
}
```

Bridging JavaScript and Kotlin:
```kotlin
val jsBridge = JsBridge(JsBridgeConfig.standardConfig())
jsBridge.evaluateLocalFileUnsync(context, "js/api.js")

// JS "proxy" to native API
val nativeApiJsValue = JsValue.fromNativeObject(jsBridge, nativeApi)

// JS function createApi(nativeApi, config)
val config = JsonObjectWrapper("debug" to true, "useFahrenheit" to false)  // {debug: true, useFahrenheit: false}
val createJsApi: suspend (JsValue, JsonObjectWrapper) -> JsValue
    = JsValue(jsBridge, "createApi").mapToNativeFunction2()
    
// Create native "proxy" to JS API
val jsApi: JsApi = createJsApi(nativeApiJsValue, config).mapToNativeObject()
```

Consume API:
```kotlin
val msg = jsApi.createMessage()  // (suspending) "Hello Android, the temperature is 37.2 degrees C."
val sum = jsApi.calcSum(3, 2)  // (suspending) 5
```

## Changelog
Until we have a proper Changelog file, you can use a convenient way to see changes between 2 tags in GitHub, e.g.  
https://github.com/p7s1digital/oasis-jsbridge-android/compare/0.13.0...0.14.2

## License

```
Copyright (C) 2018-2020 ProSiebenSat1.Digital GmbH
🏝 Oasis Player team

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```


Originally based on [Duktape Android][duktape-android] (Apache license, version 2.0)<br/>
`Copyright (C) 2015 Square, Inc.`

Includes C code from [Duktape][duktape] (MIT license)<br/>
`Copyright (c) 2013-2020 by Duktape authors`

Includes C code from [QuickJS][quickjs] (MIT license)<br/>
`Copyright (c) 2017-2020 Fabrice Bellard`<br/>
`Copyright (c) 2017-2010 Charlie Gordon`


 [duktape-android]: https://github.com/square/duktape-android/
 [duktape]: http://duktape.org/
 [quickjs]: https://bellard.org/quickjs/
