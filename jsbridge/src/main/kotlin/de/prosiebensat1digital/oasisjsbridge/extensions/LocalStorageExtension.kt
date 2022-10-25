/*
 * Copyright (C) 2019 ProSiebenSat1.Digital GmbH.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package de.prosiebensat1digital.oasisjsbridge.extensions

import android.content.Context
import de.prosiebensat1digital.oasisjsbridge.JsBridge
import de.prosiebensat1digital.oasisjsbridge.JsBridgeConfig
import de.prosiebensat1digital.oasisjsbridge.JsValue

internal class LocalStorageExtension(
    private val jsBridge: JsBridge,
    val config: JsBridgeConfig.LocalStorageConfig,
    context: Context,
) {

    init {
        if (config.useDefaultLocalStorage) {
            val localStorageJsValue: JsValue
            val localStorage: LocalStorageInteface = LocalStorage(context)
            localStorageJsValue = JsValue.fromNativeObject(jsBridge, localStorage)
            localStorageJsValue.assignToGlobal("localStorage")
        }
    }
}

