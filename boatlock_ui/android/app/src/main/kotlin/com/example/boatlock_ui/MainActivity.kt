package com.example.boatlock_ui

import android.app.Activity
import android.app.KeyguardManager
import android.content.Context
import android.content.Intent
import android.database.Cursor
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.OpenableColumns
import android.view.WindowManager
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    private val runtimeChannelName = "boatlock/runtime"
    private val firmwareFileChannelName = "boatlock/firmware_file"
    private val pickFirmwareRequestCode = 4202
    private var pendingFirmwarePickResult: MethodChannel.Result? = null

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
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, firmwareFileChannelName).setMethodCallHandler { call, result ->
            when (call.method) {
                "pickFirmwareBin" -> openFirmwarePicker(result)
                else -> result.notImplemented()
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
    }

    @Deprecated("Deprecated in Android framework, still compatible with FlutterActivity result forwarding.")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != pickFirmwareRequestCode) {
            return
        }
        val result = pendingFirmwarePickResult ?: return
        pendingFirmwarePickResult = null
        if (resultCode != Activity.RESULT_OK) {
            result.success(null)
            return
        }
        val uri = data?.data
        if (uri == null) {
            result.success(null)
            return
        }
        try {
            contentResolver.takePersistableUriPermission(
                uri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION,
            )
        } catch (_: SecurityException) {
            // Some providers grant one-shot read permission only; reading below is enough.
        }
        try {
            result.success(readFirmwareFile(uri))
        } catch (e: Exception) {
            result.error("firmware_file_read_failed", e.message, null)
        }
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

    private fun openFirmwarePicker(result: MethodChannel.Result) {
        if (pendingFirmwarePickResult != null) {
            result.error("firmware_picker_busy", "Firmware picker is already open", null)
            return
        }
        pendingFirmwarePickResult = result
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            putExtra(
                Intent.EXTRA_MIME_TYPES,
                arrayOf("application/octet-stream", "application/bin", "application/x-binary"),
            )
            addFlags(
                Intent.FLAG_GRANT_READ_URI_PERMISSION or
                    Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION,
            )
        }
        try {
            startActivityForResult(intent, pickFirmwareRequestCode)
        } catch (e: Exception) {
            pendingFirmwarePickResult = null
            result.error("firmware_picker_unavailable", e.message, null)
        }
    }

    private fun readFirmwareFile(uri: Uri): Map<String, Any> {
        val bytes = contentResolver.openInputStream(uri)?.use { it.readBytes() }
            ?: throw IllegalStateException("Cannot open selected file")
        val metadata = readOpenableMetadata(uri)
        val name = metadata.first ?: uri.lastPathSegment ?: "firmware.bin"
        val size = metadata.second ?: bytes.size.toLong()
        return mapOf(
            "name" to name,
            "size" to size,
            "bytes" to bytes,
        )
    }

    private fun readOpenableMetadata(uri: Uri): Pair<String?, Long?> {
        var cursor: Cursor? = null
        try {
            cursor = contentResolver.query(
                uri,
                arrayOf(OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE),
                null,
                null,
                null,
            )
            if (cursor == null || !cursor.moveToFirst()) {
                return Pair(null, null)
            }
            val nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            val sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE)
            val name = if (nameIndex >= 0 && !cursor.isNull(nameIndex)) {
                cursor.getString(nameIndex)
            } else {
                null
            }
            val size = if (sizeIndex >= 0 && !cursor.isNull(sizeIndex)) {
                cursor.getLong(sizeIndex)
            } else {
                null
            }
            return Pair(name, size)
        } finally {
            cursor?.close()
        }
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
