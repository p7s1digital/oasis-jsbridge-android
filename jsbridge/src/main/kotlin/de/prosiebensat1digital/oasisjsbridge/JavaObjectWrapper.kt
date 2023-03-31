package de.prosiebensat1digital.oasisjsbridge

class JavaObjectWrapper<T: Any?>(val obj: T) {
    @Suppress("PLATFORM_CLASS_MAPPED_TO_KOTLIN", "UNUSED")
    fun extractJavaObject(): Object? = obj as Object?

    companion object {
        @JvmStatic
        @Suppress("PLATFORM_CLASS_MAPPED_TO_KOTLIN", "UNUSED")
        fun getOrCreate(o: Object?): JavaObjectWrapper<Object?> {
            @Suppress("UNCHECKED_CAST")
            if (o is JavaObjectWrapper<*>) return o as JavaObjectWrapper<Object?>
            return fromJavaObject(o)
        }

        @JvmStatic
        @Suppress("PLATFORM_CLASS_MAPPED_TO_KOTLIN", "UNUSED")
        fun fromJavaObject(o: Object?) = JavaObjectWrapper(o)
    }
}
