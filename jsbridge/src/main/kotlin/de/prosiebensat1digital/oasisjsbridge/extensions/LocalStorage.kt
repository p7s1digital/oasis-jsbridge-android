package de.prosiebensat1digital.oasisjsbridge.extensions

import android.content.Context

import de.prosiebensat1digital.oasisjsbridge.JsToNativeInterface
import de.prosiebensat1digital.oasisjsbridge.JsonObjectWrapper
import timber.log.Timber

interface LocalStorageInteface : JsToNativeInterface {
    fun setItem(keyName: JsonObjectWrapper, keyValue: JsonObjectWrapper)
    fun getItem(keyName: JsonObjectWrapper): JsonObjectWrapper
    fun removeItem(keyName: JsonObjectWrapper)
    fun clear()
}

class LocalStorage(context: Context) : LocalStorageInteface {

    private val localStoragePreferences = context.getSharedPreferences(
        context.applicationInfo.packageName + ".LOCAL_STORAGE_PREFERENCE_FILE_KEY",
        Context.MODE_PRIVATE
    )

    override fun setItem(keyName: JsonObjectWrapper, keyValue: JsonObjectWrapper) {
        with(localStoragePreferences.edit()) {
            putString(keyName.jsonString, keyValue.jsonString)
            Timber.d("Saving to local storage -> key: ${keyName.jsonString} value: ${keyValue.jsonString}")
            commit()
        }
    }

    override fun getItem(keyName: JsonObjectWrapper): JsonObjectWrapper {
        val value = localStoragePreferences.getString(keyName.jsonString, "null")
        checkNotNull(value)

        Timber.d("Loaded from local storage -> key: ${keyName.jsonString} value: $value")

        // Note: default value is an unquoted null-string which is parsed as a null value (not "null"!)
        return JsonObjectWrapper(value)
    }

    override fun removeItem(keyName: JsonObjectWrapper) {
        with(localStoragePreferences.edit()) {
            remove(keyName.jsonString)
            Timber.d("Removing from local storage -> key: ${keyName.jsonString}")
            commit()
        }
    }

    override fun clear() {
        with(localStoragePreferences.edit()) {
            clear()
            Timber.d("Clearing local storage")
            commit()
        }
    }
}
