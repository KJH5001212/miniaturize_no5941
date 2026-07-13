# JS bridge methods are called reflectively from WebView — keep them.
-keepclassmembers class com.ampero.pstat.NativeBridge {
    @android.webkit.JavascriptInterface <methods>;
}
