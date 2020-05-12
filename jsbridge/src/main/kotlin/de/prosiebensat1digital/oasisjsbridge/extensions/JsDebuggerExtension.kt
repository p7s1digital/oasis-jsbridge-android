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

import android.app.Activity
import android.app.AlertDialog
import android.app.Dialog
import android.content.Context.WIFI_SERVICE
import android.net.wifi.WifiManager
import de.prosiebensat1digital.oasisjsbridge.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import timber.log.Timber
import java.lang.ref.WeakReference
import java.math.BigInteger
import java.net.InetAddress
import java.nio.ByteOrder

class JsDebuggerExtension(
    private val jsBridge: JsBridge,
    val config: JsBridgeConfig.JsDebuggerConfig
) {
    // Activity (only needed to display the debugging dialog)
    private var activityRef = WeakReference<Activity?>(null)
    var activity: Activity?
        get() = activityRef.get()
        set(value) { activityRef = WeakReference(value) }

    private var debuggerDialog: Dialog? = null

    fun release() {
        jsBridge.launch(Dispatchers.Main) {
            debuggerDialog?.cancel()
            debuggerDialog = null
        }
    }

    private fun getLocalIpAddress(): String? {
        val activity = this.activityRef.get() ?: return null

        val wifiManager = activity.applicationContext.getSystemService(WIFI_SERVICE) as WifiManager
        var ipAddress = wifiManager.connectionInfo.ipAddress

        // Convert little-endian to big-endianif needed
        if (ByteOrder.nativeOrder() == ByteOrder.LITTLE_ENDIAN) {
            ipAddress = Integer.reverseBytes(ipAddress)
        }

        val ipByteArray = BigInteger.valueOf(ipAddress.toLong()).toByteArray()

        return try {
            InetAddress.getByAddress(ipByteArray).hostAddress
        } catch (t: Throwable) {
            println("Duktape debugger exception: $t")
            return "<device-ip-address>"
        }
    }

    internal fun onDebuggerPending() {
        Timber.v("onDebuggerPending")

        val activity = this.activityRef.get() ?: return

        jsBridge.launch(Dispatchers.Main) {
            Timber.v("onDebuggerPending - launch")

            val builder = AlertDialog.Builder(activity)

            builder.setMessage("""
                |Please attach to the Duktape debugger:
                |- go to the "jsbridge/src/duktape" folder
                |- run "./startDebugger.sh -i ${getLocalIpAddress()} -p ${config.port} --open-browser"
                |- click on "attach" in the browser window

                |Or under VS Code:
                |- install Duktape Debugger plugin (>= v0.4.6)
                |- open project root folder
                |- set up launch.json (or press the 'wheel' icon in the debugger tab)
                |  - "address" = "${getLocalIpAddress()}"
                |  - "port" = ${config.port}
                |  - "outDir" = "${"$"}{workspaceRoot}/path/to/js/assets"
                |- attach debugger

                |If you are using the simulator, use "localhost" and first execute:
                |- adb forward tcp:${config.port} tcp:${config.port}
            """.trimMargin())
                .setTitle("Waiting for debugger")
                .setNegativeButton("Stop debugging") { _, _ ->
                    Timber.i("Cancelling debugging...")
                    jsBridge.cancelDebug()
                    debuggerDialog?.cancel()
                    debuggerDialog = null
                }

            Timber.v("onDebuggerPending - show dialog")
            debuggerDialog = builder.create()
            debuggerDialog?.show()
        }
    }

    @Suppress("UNUSED")  // Called from JNI
    internal fun onDebuggerReady() {
        jsBridge.launch(Dispatchers.Main) {
            debuggerDialog?.cancel()
            debuggerDialog = null
        }
    }

}

