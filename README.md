🏝 Oasis JsBridge
===============

Evaluate JavaScript code and map values, objects and functions between Kotlin/Java and JavaScript on Android.  

Powered by:
- [Duktape][duktape] or
- [QuickJS][quickjs]


## Features

Based on [Duktape Android][duktape-android] we added:
 * advanced type inference (generics, reified type parameters)
 * support for non-blocking API, suspending functions (coroutines)
 * higher order functions (lambdas, callback functions)
 * two-way support for JS promises (mapped to Kotlin via Deferred or suspending functions)
 * JsValue class to reference JS objects from Kotlin/Java
 * JsonObjectWrapper for object serialization
 * polyfills for some JS runtime features (e.g. setTimeout, XmlHttpRequest, console)


## Usage
  
1. [Evaluate JS code](#1-evaluating-js-code)
1. [Reference any JS value](#2-jsvalue)
1. [Map JS functions to Kotlin](#3-calling-js-functions-from-kotlin)
1. [Map Kotlin functions to JS](#4-calling-kotlin-functions-from-js)
1. [Map JS objects to Java/Kotlin](#5-using-js-objects-from-javakotlin)
1. [Map Kotlin/Java objects to JS](#6-using-javakotlin-objects-from-js)

Minimal hello world:
```kotlin
val jsBridge = JsBridge(application.context)
jsBridge.start()
jsBridge.evaluateNoRetVal("console.log('Hello world!');")
jsBridge.release()
```

Hello world with JS <-> Kotlin function mapping and Promise:
```kotlin
val jsBridge = JsBridge(application.context)
jsBridge.start()

// Create a Kotlin proxy to a JS function
val helloJs: suspend (String) -> Unit = JsValue.newFunction(jsBridge, "s",
    "console.log('Hello ' + s + '!');"
).mapToNativeFunction1()

// Create a JS proxy to a Kotlin function
val toUpperCaseNative = JsValue.fromNativeFunction1(jsBridge) { s: String -> s.toUpperCase() }

runBlocking {
    // Evaluate JS promise calling Kotlin function "toUpperCaseNative()"
    // (suspend function call) -> returns "WORLD" after 1s timeout
    val world: String = jsBridge.evaluate("""new Promise(function(resolve) {
      setTimeout(function() {
        resolve($toUpperCaseNative('world'));
      }, 1000);
    })""".trimIndent())
    
    // Manually release the JsValue after usage (otherwise handled by garbage collector)
    toUpperCaseNative.release()
    
    // Call JS function "helloJs()" -> displays "Hello WORLD!"
    helloJs(world)
}

jsBridge.release()
``` 

### 1. Evaluating JS code

Without return value:
```kotlin
jsBridge.evaluateNoRetVal("console.log('hello');")
```

Blocking evaluation:
```kotlin
val result1: Int = jsBridge.evaluateBlocking("1+2")  // generic type inferred
val result2 = jsBridge.evaluateBlocking<Int>("1+2")  // with explicit generic type
```

Via coroutines:
```kotlin
CoroutineScope.launch {
  val result: Int = jsBridge.evaluate("1+2")  // suspending call
}
```

From Java (blocking):
```java
Integer sum = (Integer) jsBridge.evaluate("1 + 2", Integer.class);
```


### 2. JsValue

A JsValue is a reference to any JS value.

#### 2.1 Creating a JsValue

With initial value (JS code):<br/>
```kotlin
val jsValue = JsValue(jsBridge, "'hello'.toUpperCase()")
```

As a new JS function:<br/>
```kotlin
val calcSumJs = JsValue.newFunction(jsBridge, "a", "b", "return a + b;")
```

As [a proxy to a Kotlin lambda](#4-calling-kotlin-functions-from-js):<br/>
```kotlin
val nativeFunctionJs = JsValue.fromNativeFunction2(jsBridge) { a: Int, b: Int -> a + b }
```

As [a proxy to a Java/Kotlin object](#6-using-javakotlin-objects-from-js):<br/>
```kotlin
val nativeObjectJs = JsValue.fromNativeObject(jsBridge, jsToNativeApi)
```

#### 2.2 Associated JS variable

It has an associated (global) JS variable whose name can be accessed via `toString()` which makes it easy to re-use it from JS code.
e.g.: `val sum: Int = jsBridge.evaluate("$calcSumJs(2, 3)")`


#### 2.3 JsValue scope

The scope a JsValue is the one defined by the JVM. In other words, the associated global
variable will be deleted when JsValue is garbage-collected. To make it persisting in the
JS world, you can either manually copy it via JS code or use JsValue.assignToGlobal(): 
e.g.:
- `jsBridge.evaluate<Unit>("global['nativeApi'] = $nativeObjectJs")`
- `nativeObjectJs.assignToGlobal("nativeApi")`

Note: this implies that any access to the associated global JS variable via `toString()` must not be done when
there is no existing reference to the JsValue instance in the JVM!


#### 2.4 JsValue evaluation and mapping to Kotlin/Java

A JS value can be evaluated via:
- `JsValue.evaluate<T>()`
- `JsValue.evaluateAsync<T>()`

A JS function can be [mapped to a Kotlin proxy function](#using-js-functions-from-native) via `JsValue.mapToNativeObject()`.
A JS object can be [mapped to a Java/Kotlin proxy object](#using-js-objects-from-native) via `JsValue.mapToNativeObject()`.


### 3. Calling JS functions from Kotlin

```kotlin
val calcSumJs: suspend (Int, Int) -> Int = JsValue.newFunction(jsBridge, "a", "b", """
  return a + b;
""".trimIndent())
    .mapToNativeFunction2()

println("Sum is $calcSumJs(1, 2)")
```

Note: the JS code runs asynchronously in a dedicated "JS" thread

Available methods:
 * `JsValue.mapToNativeFunctionX()` (where X is the number of arguments)
 * `JsValue.mapToNativeBlockingFunctionX()`: blocks the current thread until the JS code has been evaluated


### 4. Calling Kotlin functions from JS

```kotlin
val calcSumNative = JsValue.fromNativeFunction2(jsBridge) { a: Int, b: Int -> a + b }

jsBridge.evaluateNoRetVal("""
  console.log("Sum is", $calcSumNative(1, 2));
""".trimIndent())
```

Note: the native function is triggered from the "JS" thread


### 5. Using JS objects from Java/Kotlin

An interface extending `NativeToJsInterface` must be defined with the methods implemented by the
JS object and mapped to a native object via `JsValue.mapToNativeObject()``

```kotlin
interface JsApi: NativeToJsInterface {
  fun calcSum(a: Int, b: Int): Deferred<Int>
  fun setCallback(cb: (payload: JsonObjectWrapper) -> Unit)
  fun triggerEvent()
}

val jsApi: JsApi = JsValue(jsBridge, """({
    calcSum: function(a, b) { return a + b; },
    setCallback: function(cb) { this._cb = cb; },
    triggerEvent: function() { if (_cb) cb({ value: 12 }); } 
})""".trimIndent()).mapToNativeObject()

val result = jsApi.calcSum(1, 2).await()
jsApi.setCallback { payload -> println("Got JS event with payload $payload") }
jsApi.triggerEvent()
```

Note:
- the JS methods are running asynchronously in the "JS" thread
- Kotlin: the return value of the methods may only be `Unit`/`void` or a `Deferred`. Suspending methods will
be supported in the future.
- Java: calling the method will block the caller thread (if it is not a `void` method) until the result has
been returned.



### 6. Using Java/Kotlin objects from JS

An interface extending `JsToNativeInterface` must be defined, implemented by the
native object and mapped to a new JsValue via `JsValue.fromNativeObject()`.

```kotlin
interface NativeApi: JsToNativeInterface {
  fun calcSum(a: Int, b: Int): Int
  fun setCallback(cb: (payload: JsonObjectWrapper) -> Unit)
  fun triggerEvent()
}

val nativeApi = object: NativeApi {
  private var cb: ((JsonObjectWrapper) -> Unit)? = null

  override fun calcSum(a: Int, b: Int) = a + b
  override fun setCallback(cb: (payload: JsonObjectWrapper) -> Unit) {
    this.cb = cb
  }
  override fun triggerEvent() {
    val payload = JsonObjectWrapper("value" to 12)
    cb?.invoke(payload)
  }
}

val nativeApiJsValue = JsValue.fromNativeObject(jsBridge, nativeApi)

jsBridge.evaluateNoRetVal("""
    var result = $nativeApiJsValue.calcSum(1, 2)
    $nativeApi.setCallback(function(payload) {
      console.log("Got native event with value=" + payload.value);
    })
    $nativeApi.triggerEvent()
""".trimIndent())
```

Note: the native methods are called from the "JS" thread and must properly
manage the execution context (e.g.: going to the main thread when calling
UI methods)


## Supported types

| Kotlin             | Java                | JS        | Note
| ------------------ | ------------------- | --------- | ---
| `Boolean`          | `boolean`, `Boolean`| `number`  |
| `Int`              | `int`, `Integer`    | `number`  |
| `Float`            | `float`, `Float`    | `number`  |
| `Double`           | `double`, `Double`  | `number`  |
| `String`           | `String`            | `string`  |
| `BooleanArray`     | `boolean[]`         | `Array`   |
| `IntArray`         | `int[]`             | `Array`   |
| `FloatArray`       | `float[]`           | `Array`   |
| `DoubleArray`      | `double[]`          | `Array`   |
| `Array<T: Any>`    | `T[]`               | `Array`   | T must be a supported type
| `Function<R>`      | n.a.                | `function`| lambda with supported types
| `Deferred<T>`      | n.a.                | `Promise` | T must be a supported type
| `JsonObjectWrapper`| `JsonObjectWrapper` | `object`  | serializes JS objects via JSON
| `JsValue`          | `JsValue`           | any       | reference any JS value


## TODO:
* publish to maven central
* code samples


## License

```
Copyright (C) 2018-2019 ProSiebenSat1.Digital GmbH
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
`Copyright (c) 2013-2019 by Duktape authors`

Includes C code from [QuickJS][quickjs] (MIT license)<br/>
`Copyright (c) 2017-2019 Fabrice Bellard`<br/>
`Copyright (c) 2017-2019 Charlie Gordon`


 [duktape-android]: https://github.com/square/duktape-android/
 [duktape]: http://duktape.org/
 [quickjs]: https://bellard.org/quickjs/
