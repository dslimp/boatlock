package com.example.boatlock_ui

import android.app.KeyguardManager
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.view.WindowManager
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    private val runtimeChannelName = "boatlock/runtime"

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        keepActivityVisibleForControlSession()
    }

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, runtimeChannelName).setMethodCallHandler { call, result ->
            when (call.method) {
                "initialCommand" -> result.success(runtimeCommandFromIntent(intent))
                else -> result.notImplemented()
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
    }

    private fun runtimeCommandFromIntent(intent: Intent?): Map<String, Any> {
        if (intent == null) {
            return emptyMap()
        }
        val values = mutableMapOf<String, Any>()
        for (key in listOf(
            "boatlock_check_mode",
            "boatlock_ota_url",
            "boatlock_ota_sha256",
            "boatlock_firmware_manifest_url",
        )) {
            intent.getStringExtra(key)?.trim()?.takeIf { it.isNotEmpty() }?.let {
                values[key] = it
            }
        }
        if (intent.getBooleanExtra("boatlock_ota_latest_release", false)) {
            values["boatlock_ota_latest_release"] = true
        }
        return values
    }

    private fun keepActivityVisibleForControlSession() {
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            setShowWhenLocked(true)
            setTurnScreenOn(true)
            val keyguardManager = getSystemService(Context.KEYGUARD_SERVICE) as KeyguardManager
            keyguardManager.requestDismissKeyguard(this, null)
        } else {
            @Suppress("DEPRECATION")
            window.addFlags(
                WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED or
                    WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON or
                    WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD,
            )
        }
    }
}
