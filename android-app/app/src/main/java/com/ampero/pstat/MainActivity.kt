package com.ampero.pstat

import android.Manifest
import android.annotation.SuppressLint
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.ContextCompat
import com.ampero.pstat.ble.BleManager

/**
 * Single-activity WebView host. The whole UI lives in assets/app.html; this
 * activity wires up the native BLE bridge and requests runtime BT permissions,
 * then tells the page which permissions it has via `window.AmperoIn("perm", …)`.
 */
class MainActivity : ComponentActivity() {

    private lateinit var web: WebView
    private lateinit var ble: BleManager
    private lateinit var bridge: NativeBridge

    private val permLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { result ->
            val granted = result.values.all { it }
            web.evaluateJavascript(
                "window.AmperoIn && window.AmperoIn('perm', {granted:$granted});", null
            )
        }

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        ble = BleManager(applicationContext)

        web = WebView(this).apply {
            settings.javaScriptEnabled = true
            settings.domStorageEnabled = true          // localStorage for app settings
            settings.mediaPlaybackRequiresUserGesture = false
            webViewClient = WebViewClient()
        }
        bridge = NativeBridge(web, ble)
        web.addJavascriptInterface(bridge, "AmperoNative")
        web.loadUrl("file:///android_asset/app.html")
        setContentView(web)
    }

    /** Called from JS (via bridge) — but also request eagerly on first resume. */
    override fun onResume() {
        super.onResume()
        ensurePermissions()
    }

    private fun ensurePermissions() {
        val needed = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            add(needed, Manifest.permission.BLUETOOTH_SCAN)
            add(needed, Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            add(needed, Manifest.permission.ACCESS_FINE_LOCATION)
        }
        if (needed.isNotEmpty()) permLauncher.launch(needed.toTypedArray())
    }

    private fun add(list: MutableList<String>, perm: String) {
        if (ContextCompat.checkSelfPermission(this, perm) != PackageManager.PERMISSION_GRANTED) {
            list.add(perm)
        }
    }

    override fun onDestroy() {
        ble.close()
        web.destroy()
        super.onDestroy()
    }
}
