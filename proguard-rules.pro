# JS bridge
-keep class de.prosiebensat1digital.oasisjsbridge.** { *; }

# Fix coroutines throw IllegalAccessError at
# "kotlin.coroutines.intrinsics.IntrinsicsKt__IntrinsicsKt__Clinit"
-keepclassmembers class * {
  void $$clinit();
}

# Need access by package in JNI code
-keep class kotlin.** { *; }
-keep class kotlinx.** { *; }

# OKHTTP
-dontwarn okhttp3.**
