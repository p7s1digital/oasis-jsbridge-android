🏝 Oasis JsBridge
=================

![jitpack](https://jitpack.io/v/p7s1digital/oasis-jsbridge-android.svg)
![build](https://github.com/p7s1digital/oasis-jsbridge-android/workflows/build/badge.svg)

Evaluate JavaScript code and map values, objects and functions between Kotlin/Java and JavaScript on Android.

```kotlin
val jsBridge = JsBridge(JsBridgeConfig.bareConfig(), context)
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
 * extensions (optional): console, setTimeout/setInterval, XmlHttpRequest, Promise, JS debugger, JVM

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
// OR: implementation "de.prosiebensat1digital.oasis-jsbridge-android:oasis-jsbridge-duktape:<version>"  // Duktape flavor
```


## Usage

1. [Evaluate JS code](#evaluating-js-code)
1. [Reference any JS value](#jsvalue)
1. [Using JS objects from Kotlin/Java](#using-js-objects-from-kotlinjava)
1. [Using Kotlin/Java objects from JS](#using-kotlinjava-objects-from-js)
1. [Calling JS functions from Kotlin](#calling-js-functions-from-kotlin)
1. [Calling Kotlin/Java functions from JS](#calling-kotlin-functions-from-js)
1. [Wrap a Java object in JS code](#wrap-java-objects-in-js-code)
1. [ES6 modules](#es6-modules)
1. [Extensions](#extensions)


### Evaluating JS code

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

Fire-and-forget evaluation:
```kotlin
jsBridge.evaluateUnsync("console.log('hello');")
jsBridge.evaluateFileContentUnsync("console.log('hello')", "js/test.js")
```

From Java (fire-and-forget and blocking):
```java
jsBridge.evaluateUnsync("console.log('hello');");
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
    // jse.stackTrace = [JavaScript.buggy(eval:1), JavaScript.<eval>(eval:2), ....JsBridge.jniEvaluateString(Java Method), ...]
}
```

Note: the JS code is evaluated in a dedicated "JS" thread.


### JsValue

A JsValue is a reference to any JS value.

```kotlin
val jsInt = JsValue(jsBridge, "123")
val jsInt = JsValue.fromJavaValue(jsBridge, 123)
val jsString = JsValue(jsBridge, "'hello'.toUpperCase()")
val jsString = JsValue.fromJavaValue(jsBridge, "HELLO")
val jsObject = JsValue(jsBridge, "({one: 1, two: 'two'})")
val jsObject = JsValue.fromJavaValue(jsBridge, JsonObjectWrapper("one" to 1, "two" to "two"))
val calcSumJs = JsValue(jsBridge, "(function(a, b) { return a + b; })")
val calcSumJs = JsValue.newFunction(jsBridge, "a", "b", "return a + b;")
val calcSumJs = JsValue.createJsToJavaProxyFunction2(jsBridge) { a: Int, b: Int -> a + b }
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
- [a JS-to-Java proxy object](#using-kotlinjava-objects-from-js) via `JsValue.createJsToJavaProxy()`.
- [a JS-to-Java proxy function](#calling-kotlin-functions-from-js) via `JsValue.createJsToJavaProxyFunction()`.

And JS objects/functions can be accessed from Java/Kotlin using:
- [a Java-to-JS proxy object](#using-js-objects-from-kotlinjava) via `JsValue.createJavaToJsProxy()`.
- [a Java-to-JS proxy function](#calling-js-functions-from-kotlin) via `JsValue.createJavaToJsProxyFunctionX()`.


### Using JS objects from Kotlin/Java

An interface extending `JavaToJsInterface` must be defined with the methods of the
JS object:

```kotlin
interface JsApi : JavaToJsInterface {
    fun method1(a: Int, b: String)
    suspend fun method2(c: Double): String
}

val jsObject = JsValue(jsBridge, """({
  method1: function(a, b) { /*...*/ },
  method2: function(c) { return "Value: " + c; }
})""")

// Create a Java proxy to the JS object
val jsApi: JsApi = jsObject.createJavaToJsProxy()  // no check
val jsApi: JsApi = jsObject.createJavaToJsProxy(check = true)  // suspending, optionally check that all methods are defined in the JS object
val jsApi: JsApi = jsObject.createJavaToJsProxy(check = true)  // blocking (with optional check)

// Call JS methods from Java
jsApi.method1(1, "two")
val s = jsApi.method2(3.456)  // suspending
```

See [Example](#example-consuming-a-js-api-from-kotlin).

Note: when calling a non-suspending method with return value, the caller thread will be blocked until the result has been returned.


### Using Kotlin/Java objects from JS

An interface extending `JsToJavaInterface` must be defined with the methods of the
Java object:

```kotlin
interface JavaApi : JsToJavaInterface {
    fun method(a: Int, b: String): Double
}

val obj = object : JavaApi {
    override fun method(a: Int, b: String): Double { ... }
}

// Create a JS proxy to the Java object
val javaApi = JsValue.createJsToJavaProxy(jsBridge, obj)

// Call Java method from JS
jsBridge.evaluate("globalThis.x = $javaApi.method(1, 'two');")
```

See [Example](#example-consuming-a-js-api-from-kotlin).

Note: the Java methods are called from the "JS" thread and must properly
manage the execution context (e.g.: going to the main thread when calling
UI methods). To avoid blocking the JS thread for asynchronous operations, it
is possible to return a Deferred.


### Calling JS functions from Kotlin

```kotlin
val calcSumJs: suspend (Int, Int) -> Int = JsValue
    .newFunction(jsBridge, "a", "b", "return a + b;")
    .createJavaToJsProxyFunction2()

println("Sum is $calcSumJs(1, 2)")
```

Available methods:
 * `JsValue.createJavaToJsProxyFunctionX()` (where X is the number of arguments)
 * `JsValue.createJavaToJsBlockingProxyFunctionX()`: blocks the current thread until the JS code has been evaluated


### Calling Kotlin functions from JS

```kotlin
val calcSumJava = JsValue.createJsToJavaProxyFunction2(jsBridge) { a: Int, b: Int -> a + b }

jsBridge.evaluate("console.log('Sum is', $calcSumJava(1, 2))");
```

Note: the Java function is triggered from the "JS" thread


### Wrap Java objects in JS code

It is possible to wrap a Java object in JS code. The Java object itself cannot be directly used
from JS but can be passed again to Kotlin/Java when needed.

```kotlin
val javaObject = android.os.Binder("dummy")
val jsJavaObject = JsValue.fromJavaValue(subject, JavaObjectWrapper(javaObject))
val javaObjectBack: JavaObjectWrapper<android.os.Binder> = jsJavaObject.evaluate()
assertSame(javaObjectBack, javaObject)
```

### ES6 modules

ES6 modules are fully supported on QuickJS. This is done by:

- Evaluating files as modules:

Example:
```
jsBridge.evaluateLocalFile(context, "js/module.js", false, JsBridge.JsFileEvaluationType.Module)
```

- Defining a custom module loader which is triggered as needed to get the JS code of a given module:

Example:
```
jsBridge.setJsModuleLoader { moduleName -> "<module_content>" }
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

- **LocalStorage:**<br/>
Built-in support for browser-like local storage. Use `JsBridgeConfig.standardConfig(namespace)`
to initialise the local storage extension using a namespace for separation of saved data between
multiple JsBridge instances.
_Note: If you use your own implementation of local storage you should disable this extension!_

- **JS Debugger:**<br/>
JS debugger support (Duktape only via Visual Studio Code plugin)

- **JVM config:**<br/>
Offers the possibility to set a custom class loader which will be used by the JsBridge to find classes.

## Supported types

| Kotlin                | Java                  | JS         | Note
| --------------------- | --------------------- | ---------- | ---
| `Boolean`             | `boolean`, `Boolean`  | `number`   |
| `Byte`                | `byte`, `Byte`        | `number`   |
| `Short`               | `short`, `Short`      | `number`   |
| `Int`                 | `int`, `Integer`      | `number`   |
| `Long`                | `long`, `Long`        | `number`   |
| `Float`               | `float`, `Float`      | `number`   |
| `Double`              | `double`, `Double`    | `number`   |
| `String`              | `String`              | `string`   |
| `BooleanArray`        | `boolean[]`           | `Array`    |
| `ByteArray`           | `byte[]`              | `Array`    |
| `IntArray`            | `int[]`               | `Array`    |
| `FloatArray`          | `float[]`             | `Array`    |
| `DoubleArray`         | `double[]`            | `Array`    |
| `Array<T: Any>`       | `T[]`                 | `Array`    | T must be a supported type
| `List<T: Any>`        | `List                 | `Array`    | T must be a supported type. Backed up by ArrayList.
| `Function<R>`         | n.a.                  | `function` | lambda with supported types
| `Deferred<T>`         | n.a.                  | `Promise`  | T must be a supported type
| `JsonObjectWrapper`   | `JsonObjectWrapper`   | `object`   | serializes JS objects via JSON
| `JavaObjectWrapper`   | `JavaObjectWrapper`   | `object`   | serializes JS objects via JSON
| `JsValue`             | `JsValue`             | `any       | references any JS value
| `JsToJavaProxy<T>`    | `JsToJavaProxy`       | `object`   | references a JS object proxy to a Java interface
| `Any?`                | `Object`              | <auto>     | dynamically mapped to a string, number, boolean, array or wrapped Java objects


## Example: consuming a JS API from Kotlin

 JavaScript <=> Kotlin API:
```kotlin
interface JsApi : JavaToJsInterface {
    suspend fun createMessage(): String
    suspend fun calcSum(a: Int, b: Int): Int
}

interface JavaApi : JsToJavaInterface {
    fun getPlatformName(): String
    fun getTemperatureCelcius(): Deferred<Float>
}
```

JavaScript API (js/api.js):
<details>
    <summary>ES5</summary>

```js
globalThis.createApi = function(javaApi, config) {
  return {
    createMessage: function() {
      const platformName = javaApi.getPlatformName();
      return javaApi.getTemperatureCelcius().then(function(celcius) {
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
globalThis.createApi = (javaApi, config) => {(
  createMessage: async () => {
    const platformName = javaApi.getPlatformName();
    const celcius = await javaApi.getTemperatureCelcius();
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
val javaApi = object: JavaApi {
    override fun getPlatformName() = "Android"
    override fun getTemperatureCelcius() = async {
        // Getting current temperature from sensor or via network service
        37.2f
    }
}
```

Bridging JavaScript and Kotlin:
```kotlin
val jsBridge = JsBridge(JsBridgeConfig.standardConfig("namespace"), context)
jsBridge.evaluateLocalFile(context, "js/api.js")

// JS "proxy" to Java API
val javaApiJsValue = JsValue.createJsToJavaProxy(jsBridge, javaApi)

// JS function createApi(javaApi, config)
val config = JsonObjectWrapper("debug" to true, "useFahrenheit" to false)  // {debug: true, useFahrenheit: false}
val createJsApi: suspend (JsValue, JsonObjectWrapper) -> JsValue
    = JsValue(jsBridge, "createApi").createJavaToJsProxyFunction2()
    
// Create Java "proxy" to JS API
val jsApi: JsApi = createJsApi(javaApiJsValue, config).createJavaToJsProxy()
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
