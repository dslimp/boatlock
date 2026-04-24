# Android USB + BLE Notes

## Current Local Capability

- `adb` is installed locally.
- Flutter and Android SDK are present locally.
- Current repo has Android app code under `boatlock_ui/android`.
- Current repo does not have a ready `integration_test/` BLE end-to-end suite.

## NH02 Bench Capability

- `nh02` can also act as the Android USB host, not only as the ESP32-S3 host.
- Use `tools/hw/nh02/android-install.sh` to ensure `adb` exists on `nh02`.
- Use `tools/hw/nh02/android-status.sh` to check USB enumeration and `adb devices -l` on `nh02`.
- If `nh02` shows the phone only as `MTP` or a vendor USB device and not in `adb devices`, the cable path is alive but USB debugging is still off on the phone.

## What Can Be Done Today

- Detect a USB-connected Android phone with `adb devices -l`.
- Detect a USB-connected Android phone on `nh02` with `tools/hw/nh02/android-status.sh`.
- Build the dedicated smoke APK with `tools/android/build-smoke-apk.sh`.
- Install and run the smoke app with `tools/android/run-smoke.sh`.
- Use the phone as the real BLE central against the BoatLock bench.
- Read device logs with `adb logcat`.
- Drive assisted/manual smoke tests while watching:
  - phone logs
  - bench serial logs via `tools/hw/nh02/monitor.sh`
  - bench status via `tools/hw/nh02/status.sh`

## Current Smoke Scope

- The smoke APK is built from `boatlock_ui/lib/main_smoke.dart`.
- It auto-starts BLE scan/connect on app launch.
- It passes only after receiving BoatLock telemetry with non-empty `mode` and `status`.
- It emits one structured log line prefixed with `BOATLOCK_SMOKE_RESULT ` for `adb` parsing.
- It is intentionally read-only and does not send `ANCHOR_ON`, manual, or other actuation commands.

## What Still Needs Extra Work For Full Automation

- A repeatable device-side BLE smoke harness in the app.
- Stable automation hooks for:
  - pairing/auth
  - reconnect
  - anchor-on deny/allow assertions
  - richer telemetry assertions

## Environment Limits Seen In This Session

- Local sandbox blocks starting the `adb` daemon without elevated execution.
- `flutter doctor` reports Android licenses are not fully accepted yet.

## Practical Next Step

- If a phone is connected with USB debugging enabled, first verify:
  - `tools/android/status.sh`
- If the phone is attached to `nh02` instead of the local workstation, first verify:
  - `tools/hw/nh02/android-install.sh`
  - `tools/hw/nh02/android-status.sh`
- Then run:
  - `tools/android/run-smoke.sh`
- After that, if stronger coverage is needed, add a narrow app-side flow for connect + auth + heartbeat + anchor-on deny/allow checks.
