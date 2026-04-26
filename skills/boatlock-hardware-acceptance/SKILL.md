---
name: boatlock-hardware-acceptance
description: "Use for BoatLock real-hardware acceptance on nh02 after flashing firmware, reading serial boot logs, checking sensor/bridge readiness, or preparing Android USB/BLE smoke tests against the hardware bench."
---

# BoatLock Hardware Acceptance

Use this skill when the task is about validating the real ESP32-S3 bench on `nh02` after a flash, or when planning/running Android USB + BLE smoke against the bench.

## Read Only What You Need

- NH02 deploy/debug path and host boundary:
  - `docs/HARDWARE_NH02.md`
- Current BoatLock hardware/runtime invariants:
  - `skills/boatlock/references/firmware.md`
- BLE/app contract:
  - `skills/boatlock/references/ble-ui.md`
- Android USB + BLE workflow and current limits:
  - `references/android-ble.md`

## NH02 Acceptance Workflow

1. Refresh the bench runtime if tracked helpers or service units changed:
   - `tools/hw/nh02/install.sh`
2. Flash current firmware when needed:
   - `tools/hw/nh02/flash.sh`
   - or `tools/hw/nh02/flash.sh --no-build`
   - or, after a BLE OTA seed, publish `firmware.bin` plus SHA-256 and upload from the phone app Settings screen
3. Run hardware acceptance:
   - `tools/hw/nh02/acceptance.sh`
4. If acceptance fails, inspect:
   - `tools/hw/nh02/status.sh`
   - `tools/hw/nh02/monitor.sh`

## Android USB + BLE Smoke Workflow

1. Check USB device visibility:
   - `tools/android/status.sh`
   - or `tools/hw/nh02/android-status.sh` when the phone is attached to `nh02`
2. Optionally switch the phone to ADB Wi-Fi after USB proof:
   - `tools/hw/nh02/android-wifi-debug.sh`
   - then pass `--serial <ip>:5555` to the Android smoke/e2e wrappers to prove debug/install/logcat over Wi-Fi
3. Build the dedicated smoke APK:
  - `tools/android/build-smoke-apk.sh`
  - or build the production app with e2e hook:
    `tools/android/build-app-apk.sh --e2e-mode basic`
4. Install and run the phone smoke flow:
   - `tools/android/run-smoke.sh`
   - or `tools/hw/nh02/android-run-smoke.sh` when the phone is attached to `nh02`
   - use `tools/hw/nh02/android-run-app-e2e.sh --wait-secs 130` when the acceptance target is the real `lib/main.dart` application rather than the dedicated smoke entrypoint
   - use `tools/hw/nh02/android-run-app-e2e.sh --manual --wait-secs 130`, `--status`, `--anchor`, `--sim`, `--reconnect`, or `--esp-reset` for production-app command/recovery flows
   - use `tools/hw/nh02/android-run-app-e2e.sh --compass --wait-secs 130` after compass BLE command changes; it sends safe DCD service commands and requires device-log acknowledgements
   - use `tools/hw/nh02/android-run-app-e2e.sh --gps --wait-secs 180` after GNSS live-telemetry changes or field GPS checks; it requires valid non-zero coordinates and `gnssQ > 0`
   - use `tools/hw/nh02/android-run-app-e2e.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin` after BLE OTA/app-delivery changes; it serves `firmware.bin` from `nh02`, installs the production app with OTA e2e defines, uploads over BLE from the phone, and requires post-reboot telemetry recovery
   - use `tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` after manual BLE protocol changes; this sends zero-throttle `MANUAL_SET`, verifies `MANUAL`, sends `MANUAL_OFF`, and verifies mode exit
   - use `tools/hw/nh02/android-run-smoke.sh --status --wait-secs 130` after phone-visible status/severity changes; this sends `STOP`, verifies `ALERT/STOP_CMD`, then clears through zero-throttle manual roundtrip
   - use `tools/hw/nh02/android-run-smoke.sh --sim --wait-secs 130` after `SIM_*` BLE/HIL changes; this starts realtime `S0`, verifies `SIM` mode, sends `SIM_ABORT`, then clears the safe hold through zero-throttle manual recovery
   - use `tools/hw/nh02/android-run-smoke.sh --anchor --wait-secs 130` after phone-visible anchor command, safety gate, or anchor telemetry changes; this sends `ANCHOR_ON`, verifies the bench-side safety denial, sends `ANCHOR_OFF`, and verifies the device did not enter `ANCHOR`
   - use `tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` when reconnect behavior is the acceptance target
   - use `tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` when ESP32 reboot recovery is the acceptance target
   - do not use `--no-install` as acceptance unless the user explicitly waives exact APK installation proof for that run
5. If it fails, inspect:
  - device logcat around `BOATLOCK_SMOKE_RESULT`
  - `tools/hw/nh02/monitor.sh`
  - `tools/hw/nh02/status.sh`
  - `tools/hw/nh02/android-status.sh` when the phone is attached to `nh02` instead of the local workstation

## What Acceptance Proves

- Boot log is reachable over the tracked debug path.
- Bench hardware reports:
  - `BNO08x-RVC` ready on `rx=12 baud=115200`
  - live BNO08x UART-RVC heading frames
  - BNO08x reset pin `13` pulse on boot/retry
  - display ready
  - NVS settings load
  - BLE init + advertising
  - stepper config
  - STOP button init
  - GPS UART activity
- Fatal boot indicators such as panic/assert/advertising failure, Arduino `[E]` logs, `COMPASS lost`, or `COMPASS retry ready=0` are not present in captured logs.
- Compass acceptance requires `[COMPASS] heading events ready`; `[COMPASS] ready=1 source=BNO08x-RVC ...` alone is not sufficient.
- Do not count `[COMPASS] reset ... pulse=1` as recovery proof. Recovery is proven only by fresh heading frames after the pulse and a clean long acceptance capture.
- For compass-focused changes, use a longer capture such as `--seconds 180`; a short boot-only pass can miss delayed wiring or frame-loss failures.

## What Acceptance Does Not Prove

- It does not prove BLE control flow end to end.
- Basic Android smoke proves BLE scan/connect and telemetry with known current protocol mode/status values only; it does not intentionally send actuation commands.
- Manual Android smoke proves exact APK install, BLE scan/connect/telemetry, zero-throttle `MANUAL_SET`, observed `MANUAL` mode, `MANUAL_OFF`, and observed mode exit. It is not a powered thrust test.
- Status Android smoke proves exact APK install, BLE scan/connect/telemetry, safe `STOP` command delivery, observed `ALERT/STOP_CMD`, and alert recovery through zero-throttle manual roundtrip. It is not a powered thrust test.
- SIM Android smoke proves exact APK install, BLE scan/connect/telemetry, `SIM_RUN:S0_hold_still_good,1`, observed `SIM` mode, `SIM_ABORT`, observed mode exit, and alert recovery through zero-throttle manual roundtrip. It is not a powered thrust test.
- Anchor Android smoke proves exact APK install, BLE scan/connect/telemetry, `ANCHOR_ON` delivery, observed `[EVENT] ANCHOR_DENIED` from the bench safety gate, `ANCHOR_OFF` cleanup, and no transition into `ANCHOR`. It is not a real anchor-hold quality test.
- Reconnect Android smoke proves exact APK install, first telemetry, host-triggered Bluetooth outage, and telemetry recovery without app restart.
- ESP reset Android smoke proves exact APK install, first telemetry, ESP32 reset through the tracked reset helper, and telemetry recovery without app restart.
- Compass Android smoke proves exact APK install, first telemetry, safe compass calibration command delivery, and device-log acknowledgement. On RVC wiring it proves routing only; it does not prove DCD success.
- GPS Android smoke proves exact APK install, BLE scan/connect/telemetry, valid non-zero coordinates, and GNSS quality `>0`. It does not prove anchor-control GNSS gate readiness unless `gnssQ=2` and anchor-specific acceptance also passes.
- OTA production-app e2e proves exact APK install, phone HTTP download via `adb reverse`, SHA-256 verification in the app, BLE OTA upload, ESP32 reboot, and telemetry recovery after the update. It does not prove bootloader or partition-table changes; those still need USB flash.
- Android smoke does not prove auth behavior.
- Full BNO08x DCD/tare acceptance requires the explicit `esp32s3_bno08x_sh2_uart` target plus SH2-UART wiring; do not count current RVC acceptance as DCD proof.
- It does not prove real anchor hold quality on water.
- It does not replace on-device `SIM_*` checks or offline simulation.

## Current Commands

- Bench runtime refresh:
  - `tools/hw/nh02/install.sh`
- Flash:
  - `tools/hw/nh02/flash.sh`
- Acceptance:
  - `tools/hw/nh02/acceptance.sh`
  - `tools/hw/nh02/acceptance.sh --no-reset`
  - `tools/hw/nh02/acceptance.sh --seconds 15 --log-out /tmp/boatlock-boot.log --json-out /tmp/boatlock-acceptance.json`
- Debug:
  - `tools/hw/nh02/status.sh`
  - `tools/hw/nh02/monitor.sh`
- Android:
  - `tools/hw/nh02/android-run-app-e2e.sh`
  - `tools/hw/nh02/android-run-app-e2e.sh --manual --wait-secs 130`
  - `tools/hw/nh02/android-run-app-e2e.sh --status --wait-secs 130`
  - `tools/hw/nh02/android-run-app-e2e.sh --sim --wait-secs 130`
  - `tools/hw/nh02/android-run-app-e2e.sh --anchor --wait-secs 130`
  - `tools/hw/nh02/android-run-app-e2e.sh --compass --wait-secs 130`
  - `tools/hw/nh02/android-run-app-e2e.sh --gps --wait-secs 180`
  - `tools/hw/nh02/android-run-app-e2e.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin`
  - `tools/hw/nh02/android-run-app-e2e.sh --reconnect --wait-secs 130`
  - `tools/hw/nh02/android-run-app-e2e.sh --esp-reset --wait-secs 130`
  - `tools/hw/nh02/android-install.sh`
  - `tools/hw/nh02/android-status.sh`
  - `tools/hw/nh02/android-wifi-debug.sh`
  - `tools/hw/nh02/android-run-smoke.sh`
  - `tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130`
  - `tools/hw/nh02/android-run-smoke.sh --status --wait-secs 130`
  - `tools/hw/nh02/android-run-smoke.sh --sim --wait-secs 130`
  - `tools/hw/nh02/android-run-smoke.sh --anchor --wait-secs 130`
  - `tools/hw/nh02/android-run-smoke.sh --compass --wait-secs 130`
  - `tools/hw/nh02/android-run-smoke.sh --gps --wait-secs 180`
  - `tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130`
  - `tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130`
  - `tools/hw/nh02/android-run-smoke.sh --no-install`
  - `tools/android/status.sh`
  - `tools/android/build-smoke-apk.sh`
  - `tools/android/run-smoke.sh`

## Working Rules

- Keep host-side ownership limited to `/opt/boatlock-hw` and the tracked RFC2217 service.
- Default refactor cadence runs hardware acceptance after every fifteenth module, not after every low-risk module.
- Run acceptance immediately when the module touches hardware drivers, pinout, deploy/debug wrappers, actuator safety, BLE reconnect/install behavior, or another path where local tests cannot bound the risk.
- When acceptance is due, run it right after flashing so the boot path stays part of the same validation slice.
- Preserve the documented order `install -> flash -> acceptance -> monitor/debug`; do not skip to a later step just because an earlier prerequisite is broken.
- For phones attached to `nh02`, use the tracked `android-install.sh` and `android-status.sh` path before falling back to local `adb` assumptions.
- For Wi-Fi ADB acceptance, seed it from the tracked USB-visible phone with `android-wifi-debug.sh`, then run the same smoke wrapper with `--serial <ip>:5555`; `adb devices` alone is not enough.
- Record whether failure is in first install policy, later `adb install -r` update, app launch, or BLE runtime; do not collapse those into one generic "Android smoke failed" verdict.
- Treat `android-run-smoke.sh --no-install` as BLE-runtime proof only; it does not prove that the exact just-built APK was installed on the phone.
- If full smoke fails with `INSTALL_FAILED_USER_RESTRICTED`, stop on that blocker. Required phone-side fixes are `Install via USB`, Xiaomi/MIUI security install confirmation, and any account or policy prompts that gate ADB installs.
- A logged first-attempt `INSTALL_FAILED_USER_RESTRICTED` followed by canonical retry `Success` and a passing `BOATLOCK_SMOKE_RESULT` is not a blocker; record the retry but trust the terminal wrapper verdict.
- Do not interrupt a live flash, build, acceptance run, or phone smoke while it is still making forward progress.
- If a bench or phone wrapper fails, fix that wrapper before normalizing a manual fallback.
- If a bench or phone wrapper returns stale, wrong, or ambiguous data, fix the wrapper or its guidance before trusting the result.
- For bash wrappers that run under macOS bash 3.2 with `set -u`, do not expand an empty array directly as `"${ARGS[@]}"`; branch on `${#ARGS[@]}` before expanding optional args.
- If `tools/hw/nh02/flash.sh --no-build` fails because expected PlatformIO artifacts are missing, rerun canonical `tools/hw/nh02/flash.sh` instead of hand-copying or reconstructing artifacts.
- Do not replace a pending or slow acceptance or phone-smoke verdict with manual `monitor.sh` or `logcat` interpretation. Use those only as blocker-debug context.
- Before live mutation on `nh02` or an attached Android phone, prove the exact target first and keep the recovery path explicit.
- Prefer repo-tracked scripts and remote helpers over ad-hoc inline `ssh` write commands or hand-typed mutation chains.
- If acceptance or monitor output disagrees with docs, update docs in the same change.
- Record every meaningful bench validation stage in `WORKLOG.md`.
