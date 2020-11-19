# All Js <-> JVM interfaces must be kept in order to make reflection work
-keep interface * implements de.prosiebensat1digital.oasisjsbridge.JsToNativeInterface { *; }
-keep interface * implements de.prosiebensat1digital.oasisjsbridge.NativeToJsInterface { *; }

# Classes like de.prosiebensat1digital.oasisjsbridge.Method must be kept
-keep class de.prosiebensat1digital.oasisjsbridge.** { *; }

# Fix coroutines throw IllegalAccessError at
# "kotlin.coroutines.intrinsics.IntrinsicsKt__IntrinsicsKt__Clinit"
-keepclassmembers class * {
  void $$clinit();
}

# Keep JNI methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Need access by package in JNI code
-keep class kotlin.** { *; }
-keep class kotlinx.** { *; }

# OKHTTP
-dontwarn okhttp3.**

