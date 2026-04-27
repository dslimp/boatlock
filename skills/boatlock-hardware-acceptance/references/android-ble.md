# Android USB + BLE Notes

## Current Local Capability

- `adb` is installed locally.
- Flutter and Android SDK are present locally.
- Current repo has Android app code under `boatlock_ui/android`.
- Current repo does not have a ready `integration_test/` BLE end-to-end suite.

## NH02 Bench Capability

- `nh02` can also act as the Android USB host, not only as the ESP32-S3 host.
- Current moved-hardware firmware updates normally use the phone as the BLE OTA
  bridge. USB ESP32 flashing is only the seed/recovery path unless the task
  explicitly targets USB bench flashing.
- Use `tools/hw/nh02/android-install.sh` to ensure `adb` exists on `nh02`.
- Use `tools/hw/nh02/android-status.sh` to check USB enumeration and `adb devices -l` on `nh02`.
- Use `tools/hw/nh02/android-wifi-debug.sh` after USB proof to switch the phone to ADB TCP/IP and print the Wi-Fi serial as `<ip>:5555`.
- Use `tools/hw/nh02/android-install-app.sh` to install the normal production app APK on the `nh02` phone without starting a smoke/check probe.
- Use `tools/hw/nh02/android-run-smoke.sh` to copy the ordinary release app APK to `nh02`, install or update it on the attached phone, launch a runtime check, and wait for `BOATLOCK_SMOKE_RESULT`.
- Pass `--serial <ip>:5555` to Android smoke/check wrappers when proving install/logcat/debug over Wi-Fi instead of USB.
- Use `tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` to install/update the release app, wait for first telemetry, cycle phone Bluetooth through ADB, and require telemetry recovery without restarting the app.
- Use `tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` to install/update the release app, wait for first telemetry, reset the ESP32-S3 through the tracked reset helper, and require telemetry recovery without restarting the app.
- Use `tools/hw/nh02/android-run-app-check.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin` to install/update the release app, serve firmware through `nh02` + `adb reverse`, start its runtime OTA check, upload over BLE from the phone, and require post-update telemetry recovery.
- Current BLE OTA requests high connection priority, a larger MTU, and write-without-response for chunks when Android and the characteristic support it; it falls back to acknowledged writes when required, so keep wrapper timeouts long enough for the slower path.
- On the Xiaomi test phone, BLE scan/OTA can fail silently when the screen is
  off. ADB `input keyevent` may be denied with `INJECT_EVENTS`, so the app
  Activity keeps itself visible with `showWhenLocked`, `turnScreenOn`, and
  `FLAG_KEEP_SCREEN_ON`.
- The app scan path must stay unfiltered at the Android scanner level. Apply
  BoatLock matching in Dart so name-only `BoatLock` adverts and `12ab` service
  adverts both work.
- If `nh02` shows the phone only as `MTP` or a vendor USB device and not in `adb devices`, the cable path is alive but USB debugging is still off on the phone.

## Install / Update Semantics Seen On The Xiaomi Test Phone

- Full acceptance requires the tracked wrapper to install the exact just-built APK with `adb install -r`; an already-installed app is not enough proof.
- Xiaomi/MIUI can block both first install and later update with `INSTALL_FAILED_USER_RESTRICTED`.
- Current Xiaomi bench phone has proven canonical ADB update only after the phone-side MIUI flow was satisfied: Xiaomi account, inserted SIM card, and `Install via USB` approval.
- Required phone-side fixes are `Install via USB`, any MIUI security install confirmation such as `USB debugging (Security settings)`, and any Xiaomi account, SIM-card, or policy prompt that gates ADB installs.
- If MIUI asks for `insert SIM card` after Xiaomi account login, treat it as a hard phone-side prerequisite for the canonical ADB install path. Do not patch around it in repo scripts.
- Use the wrapper's actual `adb install -r` result as the source of truth. `dumpsys user` may still print default restriction names and is blocker context, not a pass/fail verdict by itself.
- If a run returns `INSTALL_FAILED_USER_RESTRICTED` after this setup was previously proven, repeat the same exact install once while the phone is unlocked and the user can approve MIUI prompts.
- If the repeated exact install also fails, stop the acceptance path and fix the phone-side install policy. Do not continue with `--no-install`.
- `tools/hw/nh02/android-run-smoke.sh --no-install` exists for BLE-runtime debugging only. Do not use it as acceptance unless the user explicitly waives exact APK installation proof for that run.

## What Can Be Done Today

- Detect a USB-connected Android phone with `adb devices -l`.
- Detect a USB-connected Android phone on `nh02` with `tools/hw/nh02/android-status.sh`.
- Enable and verify ADB Wi-Fi on the same phone with `tools/hw/nh02/android-wifi-debug.sh`.
- Build the ordinary release app APK with `tools/android/build-app-apk.sh`.
- Install the normal production app APK on the `nh02` phone with `tools/hw/nh02/android-install-app.sh`.
- Install and run the smoke app with `tools/android/run-smoke.sh`.
- Use the phone as the real BLE central against the BoatLock bench.
- Prove BLE reconnect behavior on `nh02` with `tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130`.
- Prove recovery after ESP32 reboot on `nh02` with `tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130`.
- Prove and perform the normal phone-bridged BLE firmware update on `nh02` with `tools/hw/nh02/android-run-app-check.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin --wait-secs 1800`.
- Read device logs with `adb logcat`.
- Update an already-installed release app APK over USB with `adb install -r` through the tracked wrapper.
- Drive assisted/manual smoke tests while watching:
  - phone logs
  - bench serial logs via `tools/hw/nh02/monitor.sh`
  - bench status via `tools/hw/nh02/status.sh`

## Current Smoke Scope

- The ordinary release app is built from `boatlock_ui/lib/main.dart`; smoke and acceptance checks are runtime modes inside that same app.
- It auto-starts BLE scan/connect on app launch.
- It passes only after receiving BoatLock telemetry with non-empty `mode` and `status`.
- It emits one structured log line prefixed with `BOATLOCK_SMOKE_RESULT ` for `adb` parsing.
- In reconnect mode, it first emits `BOATLOCK_SMOKE_STAGE {"stage":"first_telemetry"}`, waits for a telemetry gap caused by the wrapper's Bluetooth cycle, and passes only after telemetry returns.
- In ESP reset mode, the same reconnect APK is used, but the wrapper triggers the gap by resetting the ESP32-S3 instead of cycling phone Bluetooth.
- It is intentionally read-only and does not send `ANCHOR_ON`, manual, or other actuation commands.

## What Still Needs Extra Work For Full Automation

- Stable automation hooks for:
  - pairing/auth
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
  - `tools/hw/nh02/android-run-smoke.sh`
- For reconnect acceptance, run:
  - `tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130`
- For ESP32 reboot recovery acceptance, run:
  - `tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130`
- For phone-bridged BLE OTA flashing/acceptance, build firmware first and run:
  - `tools/hw/nh02/android-run-app-check.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin --wait-secs 1800`
- After that, if stronger coverage is needed, add a narrow app-side flow for connect + auth + heartbeat + anchor-on deny/allow checks.
