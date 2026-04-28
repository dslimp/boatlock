---
name: boatlock-hardware-acceptance
description: "Use for BoatLock real-hardware acceptance on nh02 after phone BLE OTA or recovery USB flashing, reading serial boot logs, checking sensor/bridge readiness, or preparing Android USB/BLE smoke tests against the hardware bench."
---

# BoatLock Hardware Acceptance

Use this skill when the task is about updating or validating the real ESP32-S3 hardware through phone BLE OTA, recovery USB flashing on `nh02`, or Android USB + BLE smoke against the bench.

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
2. Update current firmware when needed:
   - operator path A: Android Settings -> `Файл на телефоне`
   - operator path B: Android Settings -> `Последняя с GitHub`
   - bench automation path: `tools/hw/nh02/deploy.sh`
   - reuse already-built firmware/APK through the same path: `tools/hw/nh02/deploy.sh --no-build`
   - recovery/seed path only: `tools/hw/nh02/flash.sh` or `tools/hw/nh02/flash.sh --no-build`
3. Run hardware acceptance:
   - `tools/hw/nh02/acceptance.sh`
4. For microSD logger acceptance, insert a FAT-formatted card before boot and
   verify `[SD] logger ready=1`, display redraws, `/boatlock/*.jsonl` growth,
   and `sd_dropped` stays bounded under the expected log rate.
5. If acceptance fails, inspect:
   - `tools/hw/nh02/status.sh`
   - `tools/hw/nh02/monitor.sh`

## Android USB + BLE Smoke Workflow

1. Check USB device visibility:
   - `tools/android/status.sh`
   - or `tools/hw/nh02/android-status.sh` when the phone is attached to `nh02`
2. Optionally switch the phone to ADB Wi-Fi after USB proof:
   - `tools/hw/nh02/android-wifi-debug.sh`
   - Android smoke/check wrappers auto-enable and use ADB Wi-Fi when no
     `--serial` is passed; set `BOATLOCK_NH02_ANDROID_WIFI_ADB=0` only when USB
     `adb` must be forced.
3. Build the ordinary release app APK:
  - `tools/android/build-app-apk.sh`
4. Install and run the phone check flow through the same release app:
   - `tools/android/run-smoke.sh`
   - or `tools/hw/nh02/android-run-smoke.sh` when the phone is attached to `nh02`
   - use `tools/hw/nh02/android-run-app-check.sh --manual --wait-secs 130`, `--status`, `--anchor`, `--sim`, `--reconnect`, or `--esp-reset` for production-app command/recovery flows
   - use `tools/hw/nh02/android-run-app-check.sh --sim-suite --wait-secs 1800` to run all listed on-device HIL scenarios through the production app on an already-flashed release profile
   - use `tools/hw/nh02/run-sim-suite.sh` for the standard full bench gate; it installs helpers, proves targets, flashes normal firmware, runs boot acceptance and `sim_suite`
   - use `tools/hw/nh02/android-run-app-check.sh --compass --wait-secs 130` after compass BLE command changes; it sends safe DCD setup commands and requires device-log acknowledgements
   - use `tools/hw/nh02/android-run-app-check.sh --gps --wait-secs 180` after GNSS live-telemetry changes or field GPS checks; it requires valid non-zero coordinates and `gnssQ > 0`
   - use `tools/hw/nh02/deploy.sh` after BLE OTA/app-delivery changes; it builds the normal firmware and release APK, refreshes Android helpers, installs the same release app, serves `firmware.bin` from `nh02`, uploads over BLE from the phone, and requires post-reboot telemetry recovery
  - use `tools/hw/nh02/android-run-app-check.sh --ota-latest-release --wait-secs 1800` to prove the public GitHub Release button source through the production app
  - use `tools/hw/nh02/android-run-app-check.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin` only as the lower-level local-firmware OTA check when you need to bypass the build wrapper
   - pass a longer OTA wait such as `--wait-secs 1800` when BLE discovery may
     be cold or intermittent; the wrapper timeout includes scan/reconnect time
     before upload starts and can otherwise expire during an active transfer
   - use `tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` after manual BLE protocol changes; this sends zero-throttle `MANUAL_TARGET`, verifies `MANUAL`, sends `MANUAL_OFF`, and verifies mode exit
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
- microSD logger acceptance requires real media proof. Compile success and a
  boot log alone do not prove `/boatlock/*.jsonl` creation, write cadence,
  rotation, or low-space deletion.

## What Acceptance Does Not Prove

- It does not prove BLE control flow end to end.
- Basic Android smoke proves BLE scan/connect and telemetry with known current protocol mode/status values only; it does not intentionally send actuation commands.
- Manual Android smoke proves exact APK install, BLE scan/connect/telemetry, zero-throttle `MANUAL_TARGET`, observed `MANUAL` mode, `MANUAL_OFF`, and observed mode exit. It is not a powered thrust test.
- Status Android smoke proves exact APK install, BLE scan/connect/telemetry, safe `STOP` command delivery, observed `ALERT/STOP_CMD`, and alert recovery through zero-throttle manual roundtrip. Recovery may clear directly to a non-alert `IDLE/WARN` frame without an observed intermediate `MANUAL` frame; still require `MANUAL_OFF` cleanup. It is not a powered thrust test.
- SIM Android smoke proves exact APK install, BLE scan/connect/telemetry, `SIM_RUN:S0,1`, observed `SIM` mode, `SIM_ABORT`, observed mode exit, and alert recovery through zero-throttle manual roundtrip. It is not a powered thrust test.
- Anchor Android smoke proves exact APK install, BLE scan/connect/telemetry, `ANCHOR_ON` delivery, observed `[EVENT] ANCHOR_DENIED` from the bench safety gate, `ANCHOR_OFF` cleanup, and no transition into `ANCHOR`. It is not a real anchor-hold quality test.
- Reconnect Android smoke proves exact APK install, first telemetry, host-triggered Bluetooth outage, and telemetry recovery without app restart.
- ESP reset Android smoke proves exact APK install, first telemetry, ESP32 reset through the tracked reset helper, and telemetry recovery without app restart.
- Compass Android smoke proves exact APK install, first telemetry, safe compass calibration command delivery, and device-log acknowledgement. On RVC wiring it proves routing only; it does not prove DCD success.
- GPS Android smoke proves exact APK install, BLE scan/connect/telemetry, valid non-zero coordinates, and GNSS quality `>0`. It does not prove anchor-control GNSS gate readiness unless `gnssQ=2` and anchor-specific acceptance also passes.
- OTA production-app check proves exact APK install, phone HTTP download via `adb reverse`, SHA-256 verification in the app, BLE OTA upload, ESP32 reboot, and telemetry recovery after the update. A post-`OTA_FINISH` disconnect plus reconnect/telemetry is acceptable even if the app misses the `[OTA] finish ok` notify before reboot. It does not prove bootloader or partition-table changes; those still need USB flash.
- Persistent bench wireless is Android ADB Wi-Fi to the phone plus BLE to ESP32.
  Direct ESP32 debug Wi-Fi OTA is a boot-window/debug path, not the normal
  persistent channel while BLE is active on the ESP32-S3 bench.
- BLE log characteristic `78ab` mirrors firmware serial log lines generated
  after the phone is connected/subscribed, including `[BLE]` lines. Use
  RFC2217/USB serial for pre-BLE boot logs.
- Android smoke does not prove real powered actuation or on-water hold quality.
- Full BNO08x DCD/tare acceptance requires the explicit `esp32s3_bno08x_sh2_uart` target plus SH2-UART wiring; do not count current RVC acceptance as DCD proof.
- It does not prove real anchor hold quality on water.
- It does not replace on-device `SIM_*` checks or offline simulation.
- It does not prove durable water-debug logging unless the FAT microSD card is
  inserted and checked after runtime.

## Current Commands

- Bench runtime refresh:
  - `tools/hw/nh02/install.sh`
- BLE OTA bench automation:
  - `tools/hw/nh02/deploy.sh`
  - `tools/hw/nh02/deploy.sh --no-build`
- Low-level BLE OTA app-check:
  - `tools/hw/nh02/android-run-app-check.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin --wait-secs 1800`
- Public GitHub latest-release app-check:
  - `tools/hw/nh02/android-run-app-check.sh --ota-latest-release --wait-secs 1800`
- USB seed/recovery flash:
  - `tools/hw/nh02/flash.sh`
- Acceptance:
  - `tools/hw/nh02/acceptance.sh`
  - `tools/hw/nh02/acceptance.sh --no-reset`
  - `tools/hw/nh02/acceptance.sh --seconds 15 --log-out /tmp/boatlock-boot.log --json-out /tmp/boatlock-acceptance.json`
- Debug:
  - `tools/hw/nh02/status.sh`
  - `tools/hw/nh02/monitor.sh`
- Android:
  - `tools/hw/nh02/android-install-app.sh`
  - `tools/hw/nh02/android-run-app-check.sh`
  - `tools/hw/nh02/android-run-app-check.sh --manual --wait-secs 130`
  - `tools/hw/nh02/android-run-app-check.sh --status --wait-secs 130`
  - `tools/hw/nh02/android-run-app-check.sh --sim --wait-secs 130`
  - `tools/hw/nh02/android-run-app-check.sh --sim-suite --wait-secs 1800`
  - `tools/hw/nh02/android-run-app-check.sh --anchor --wait-secs 130`
  - `tools/hw/nh02/android-run-app-check.sh --compass --wait-secs 130`
  - `tools/hw/nh02/android-run-app-check.sh --gps --wait-secs 180`
  - `tools/hw/nh02/android-run-app-check.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin`
  - `tools/hw/nh02/android-run-app-check.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin --wait-secs 1800`
  - `tools/hw/nh02/android-run-app-check.sh --reconnect --wait-secs 130`
  - `tools/hw/nh02/android-run-app-check.sh --esp-reset --wait-secs 130`
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
  - `tools/hw/nh02/run-sim-suite.sh`
  - `tools/android/status.sh`
  - `tools/android/build-app-apk.sh`
  - `tools/android/run-smoke.sh`

## Working Rules

- Keep host-side ownership limited to `/opt/boatlock-hw` and the tracked RFC2217 service.
- Default refactor cadence runs hardware acceptance after every fifteenth module, not after every low-risk module.
- Run acceptance immediately when the module touches hardware drivers, pinout, deploy/debug wrappers, actuator safety, BLE reconnect/install behavior, or another path where local tests cannot bound the risk.
- When acceptance is due on the USB bench path, run it right after the seed/recovery flash so the boot path stays part of the same validation slice.
- Preserve the documented order for the chosen path. For normal moved hardware that means target proof, firmware build, phone BLE OTA, telemetry recovery, then Android BLE checks. For USB bench recovery that means `install -> flash -> acceptance -> monitor/debug`.
- Do not treat a missing ESP32 USB serial path as a blocker for the normal phone BLE OTA path when the Android phone target is proven and BLE OTA can run.
- For phones attached to `nh02`, use the tracked `android-install.sh` and `android-status.sh` path before falling back to local `adb` assumptions.
- Use `tools/hw/nh02/android-install-app.sh` when the task is only to install the normal production app APK on the `nh02` phone without running a smoke/check probe.
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
- For BLE OTA rollouts after live-frame schema changes, remember that the new
  APK is installed before the old firmware is updated. Keep enough app-side
  parser bridge for the installed firmware to emit first telemetry and start
  OTA, then prove the bridge with `tools/hw/nh02/deploy.sh`.
- If acceptance or monitor output disagrees with docs, update docs in the same change.
- Record every meaningful bench validation stage in `WORKLOG.md`.
