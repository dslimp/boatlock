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
3. Run hardware acceptance:
   - `tools/hw/nh02/acceptance.sh`
4. If acceptance fails, inspect:
   - `tools/hw/nh02/status.sh`
   - `tools/hw/nh02/monitor.sh`

## Android USB + BLE Smoke Workflow

1. Check USB device visibility:
   - `tools/android/status.sh`
2. Build the dedicated smoke APK:
   - `tools/android/build-smoke-apk.sh`
3. Install and run the phone smoke flow:
   - `tools/android/run-smoke.sh`
4. If it fails, inspect:
  - device logcat around `BOATLOCK_SMOKE_RESULT`
  - `tools/hw/nh02/monitor.sh`
  - `tools/hw/nh02/status.sh`
  - `tools/hw/nh02/android-status.sh` when the phone is attached to `nh02` instead of the local workstation

## What Acceptance Proves

- Boot log is reachable over the tracked debug path.
- Bench hardware reports:
  - I2C inventory
  - `BNO08x` ready
  - display ready
  - EEPROM load
  - BLE init + advertising
  - stepper config
  - STOP button init
  - GPS UART activity
- Fatal boot indicators such as panic/assert/advertising failure are not present in captured logs.

## What Acceptance Does Not Prove

- It does not prove BLE control flow end to end.
- Android smoke proves BLE scan/connect/telemetry only; it does not intentionally send actuation commands.
- It does not prove mobile app reconnect/auth behavior.
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
  - `tools/hw/nh02/android-install.sh`
  - `tools/hw/nh02/android-status.sh`
  - `tools/android/status.sh`
  - `tools/android/build-smoke-apk.sh`
  - `tools/android/run-smoke.sh`

## Working Rules

- Keep host-side ownership limited to `/opt/boatlock-hw` and the tracked RFC2217 service.
- Prefer acceptance right after flashing so the boot path stays part of the same validation slice.
- Preserve the documented order `install -> flash -> acceptance -> monitor/debug`; do not skip to a later step just because an earlier prerequisite is broken.
- For phones attached to `nh02`, use the tracked `android-install.sh` and `android-status.sh` path before falling back to local `adb` assumptions.
- Do not interrupt a live flash, build, acceptance run, or phone smoke while it is still making forward progress.
- If a bench or phone wrapper fails, fix that wrapper before normalizing a manual fallback.
- If a bench or phone wrapper returns stale, wrong, or ambiguous data, fix the wrapper or its guidance before trusting the result.
- Do not replace a pending or slow acceptance or phone-smoke verdict with manual `monitor.sh` or `logcat` interpretation. Use those only as blocker-debug context.
- Before live mutation on `nh02` or an attached Android phone, prove the exact target first and keep the recovery path explicit.
- Prefer repo-tracked scripts and remote helpers over ad-hoc inline `ssh` write commands or hand-typed mutation chains.
- If acceptance or monitor output disagrees with docs, update docs in the same change.
- Record every meaningful bench validation stage in `WORKLOG.md`.
