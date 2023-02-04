package de.prosiebensat1digital.oasisjsbridge

class NativeObjectWrapper<T: Any?>(val obj: T) {
    @Suppress("PLATFORM_CLASS_MAPPED_TO_KOTLIN", "UNUSED")
    fun extractJavaObject(): Object? = obj as Object?

    companion object {
        @JvmStatic
        @Suppress("PLATFORM_CLASS_MAPPED_TO_KOTLIN", "UNUSED")
        fun fromJavaObject(o: Object?) = NativeObjectWrapper(o)
    }
}
