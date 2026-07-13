package com.ampero.pstat

import android.bluetooth.BluetoothDevice
import android.webkit.JavascriptInterface
import android.webkit.WebView
import com.ampero.pstat.ble.BleManager
import org.json.JSONObject

/**
 * JS ↔ native bridge. Exposed to the WebView page as `window.AmperoNative`.
 *
 * The HTML app (assets/app.html) uses these methods for the real device and,
 * when the object is absent (opened in a plain browser), falls back to demo
 * data. The pstat firmware speaks newline-delimited JSON over Nordic UART, so
 * the bridge is a thin pass-through: JS builds command JSON, native writes it;
 * native forwards every NUS notify frame straight to JS as a UTF-8 string.
 *
 * All callbacks into JS are marshalled onto the WebView thread and delivered by
 * calling a global `window.AmperoIn(kind, payload)` dispatcher defined in the page.
 */
class NativeBridge(
    private val web: WebView,
    private val ble: BleManager,
) : BleManager.Listener {

    init { ble.listener = this }

    // ---- JS → native ----------------------------------------------------

    @JavascriptInterface
    fun isBluetoothOn(): Boolean = ble.isBluetoothOn()

    @JavascriptInterface
    fun startScan() { ble.startScan() }

    @JavascriptInterface
    fun stopScan() { ble.stopScan() }

    /** Connect by MAC address (from a prior onScanResult). */
    @JavascriptInterface
    fun connect(address: String) {
        val dev = pendingDevices[address] ?: return
        ble.connect(dev)
    }

    @JavascriptInterface
    fun disconnect() { ble.disconnect() }

    @JavascriptInterface
    fun canSend(): Boolean = ble.canSend()

    /** Send a raw command line (JS appends its own newline if the firmware needs it). */
    @JavascriptInterface
    fun send(line: String): Boolean {
        val bytes = (if (line.endsWith("\n")) line else "$line\n").toByteArray(Charsets.UTF_8)
        return ble.send(bytes)
    }

    /** Streaming = HIGH connection priority; idle = BALANCED (battery). */
    @JavascriptInterface
    fun setStreaming(high: Boolean) { ble.requestHighPriority(high) }

    // ---- native → JS ----------------------------------------------------

    private val pendingDevices = HashMap<String, BluetoothDevice>()

    private fun dispatch(kind: String, payload: JSONObject) {
        val js = "window.AmperoIn && window.AmperoIn(${quote(kind)}, ${payload});"
        web.post { web.evaluateJavascript(js, null) }
    }

    private fun quote(s: String): String = JSONObject.quote(s)

    override fun onScanResult(device: BluetoothDevice, rssi: Int, name: String?) {
        val addr = device.address ?: return
        pendingDevices[addr] = device
        dispatch("scan", JSONObject()
            .put("address", addr)
            .put("name", name ?: "")
            .put("rssi", rssi))
    }

    override fun onConnectionStateChanged(connected: Boolean, deviceName: String?) {
        dispatch("conn", JSONObject()
            .put("connected", connected)
            .put("name", deviceName ?: ""))
    }

    override fun onNotifyReady(uuid: String) {
        dispatch("ready", JSONObject().put("uuid", uuid))
    }

    override fun onDataReceived(bytes: ByteArray) {
        // Firmware frames are UTF-8 JSON lines; forward verbatim, JS reassembles on '\n'.
        dispatch("data", JSONObject().put("text", String(bytes, Charsets.UTF_8)))
    }

    override fun onLog(message: String) {
        dispatch("log", JSONObject().put("msg", message))
    }
}
