# BoatLock Agent Notes

This file captures non-obvious project behavior that must stay stable unless explicitly changed.

## Agent Workflow Requirements

- At the start of every coding/test session, read this file first and follow it as source-of-truth runbook.
- Before HIL simulation, explicitly confirm active anchor mode and log it (`POSITION` or `POSITION_HEADING`).
- If instructions conflict with earlier chat assumptions, this file has priority until user explicitly changes behavior.
- For Codex sessions: include `AGENTS.md instructions for /Users/user/Documents/boatlock` in the prompt so the file is loaded in context on turn start.

## Python Environment Policy

- Use a single persistent virtual environment at repo root: `/Users/user/Documents/boatlock/.venv`.
- Do not create per-task/per-run virtual environments.
- Install Python tooling/libraries for agent scripts only into this `.venv`.
- Reuse the same `.venv` across sessions to reduce network traffic and speed up repeated runs.

## Hardware Baseline

- Compass: only `BNO08x` is supported in main firmware.
- Legacy `HMC5883` support is removed and must not be reintroduced.
- BNO08x I2C addresses: `0x4B` (primary), `0x4A` (fallback).

## I2C Pins

- Default firmware (`esp32s3`): `SDA=47`, `SCL=48`.
- `BOATLOCK_BOARD_JC4832W535`: `SDA=4`, `SCL=8` because LCD QSPI occupies `47/48`.
- On boot firmware prints I2C inventory (`[I2C] ...`) and should list found devices.

## GPS Source Priority

1. Use hardware GPS when location is valid and age `< 2000 ms`.
2. Otherwise use phone GPS (`SET_PHONE_GPS`) if fresh `< 5000 ms`.
3. Otherwise GPS fix is considered unavailable.

Notes:
- Hardware GPS path applies moving-average filter with window from setting `GpsFWin` (`1..20`).
- `gpsFromPhone` is set only when phone GPS fallback is active.

## Heading Source Priority

1. Use onboard compass (BNO08x) when ready (+ optional GPS correction).
2. Otherwise heading is unavailable.

Notes:
- Phone compass emulation is not used in main firmware.
- Compatibility commands `SET_HEADING` / `EMU_COMPASS` must not alter heading source in production logic.

Related params exposed over BLE:
- `heading`, `headingRaw`, `compassOffset`, `gpsHdgCorr`, `compassQ`, `rvAcc`, `magNorm`, `gyroNorm`, `magQ`, `gyroQ`, `pitch`, `roll`.

## GPS-to-Compass Heading Correction

Purpose: reduce constant yaw bias using movement-based GPS course.

Active only when:
- onboard compass is ready
- speed `>= 3.0 km/h`
- movement since reference point `>= 4.0 m`

Behavior:
- target correction = `wrap180(gpsCourse - compassHeading)`
- clamp correction to `[-90, +90]` deg
- smooth with `alpha=0.18`
- correction expires after `180000 ms` without updates

Do not remove this logic without verifying open-water heading behavior.

## Compass Calibration / Offset

- BNO08x runs dynamic calibration; `CALIB_COMPASS` is compatibility-only (no active procedure).
- Firmware enables BNO runtime calibration and DCD auto-save (`sh2_setDcdAutoSave(true)`).
- User yaw mounting offset is persisted via setting `MagOffX` and applied as heading offset.

## Display Compass Convention (Firmware Screen)

- Boat nose is fixed at top of the screen.
- Compass card (N/E/S/W) rotates with heading using validated sign convention:
  - card rotation uses `worldDeg + heading`.
- Anchor arrow is drawn in boat frame with same sign convention:
  - relative arrow uses `anchorBearing + heading`.

If changing sign/math, validate on real hardware (rotate board physically and confirm north/anchor behavior).

## BLE Contract (Firmware)

- Device name: `BoatLock`
- Service UUID: `12ab`
- Characteristics:
  - `34cd`: data notify/read
  - `56ef`: command write
  - `78ab`: log notify
- Boot logs must include BLE init/advertising status:
  - `[BLE] init ...`
  - `[BLE] advertising started|failed`

## Mobile BLE Discovery Rules

In `boatlock_ui/lib/ble/ble_boatlock.dart`:
- Device match is true if:
  - adv/device name equals or contains `boatlock`, OR
  - advertised service UUID contains `12ab`.
- If scan cycle finds nothing, app schedules reconnect/scan retry after `3 s`.
- On successful find, scanning is stopped immediately before connect.

## Serial/Monitor Practical Notes

- USB CDC serial is enabled in `platformio.ini`:
  - `-DARDUINO_USB_MODE=1`
  - `-DARDUINO_USB_CDC_ON_BOOT=1`
- Only one process can own `/dev/cu.usbmodem2101` at a time.
  - Close `pio device monitor` before flashing or opening another serial reader.

## Debug/Test Sketches

- Compass debug sketches are kept under `boatlock/debug/`:
  - `boatlock/debug/bno_compass_test/bno_calib_test.cpp`
  - `boatlock/debug/compass_north_lock_test.cpp`
- Main firmware build in `boatlock/platformio.ini` uses `build_src_filter = +<*.cpp>`.
  - This means only top-level `boatlock/*.cpp` is built for production firmware.
  - Files in `boatlock/debug/` are for manual testing and must not silently replace main logic.

## On-Hardware HIL Simulation Runbook

Use this when running `S0..S19` on a real ESP32 board with the UI connected over BLE.

1. Build and flash firmware:
   - `cd boatlock`
   - `pio run -e esp32s3 -t upload --upload-port /dev/cu.usbmodem2101`
2. Start UI (second terminal):
   - `cd boatlock_ui`
   - `flutter run -d macos`
3. Connect a second BLE client for simulation commands (Python/bleak or similar):
   - write commands to characteristic `56ef`
   - read telemetry from `34cd` and/or logs from `78ab`
4. Useful simulation commands:
   - `SET_ANCHOR_MODE:POSITION`
   - `SET_ANCHOR_MODE:POSITION_HEADING`
   - `SIM_LIST`
   - `SIM_RUN:<scenario_id>,0` for accelerated mode (`0` = fastest)
   - `SIM_RUN:<scenario_id>,1` for realtime mode (`1` = realtime)
   - `SIM_STATUS`
   - `SIM_REPORT`
   - `SIM_ABORT`

Notes:
- Run order for full sweep: `S0_hold_still_good` ... `S19_random_emergency_mix`.
- Run full sweep in both modes:
  - pass 1: `SET_ANCHOR_MODE:POSITION`, then `S0..S19`
  - pass 2: `SET_ANCHOR_MODE:POSITION_HEADING`, then `S0..S19`
- Before starting sweep, ensure exactly one BLE runner is active:
  - stop stale `bleak`/Python runners
  - send `SIM_ABORT`
  - verify `SIM_STATUS` returns `IDLE`
- With UI connected, avoid flooding `SIM_STATUS`; keep polling around `1 Hz` (or slower).
  Too frequent polling can overflow firmware command queue and produce:
  `[BLE] command queue full, dropped: ...`
- If queue overflow appears, stop runners, reset/reflash, and continue with lower command rate.
- BLE server is configured for practical multi-client usage (UI + one control client).

## HIL Acceptance Checklist (Do Not Skip)

- Collect and keep artifacts for every run:
  - boot log with `[I2C]` inventory and `[BLE] init/advertising`
  - serial runtime log
  - BLE command/log stream
  - per-scenario summary JSON
- Treat `SIM_REPORT pass=true` as necessary but not sufficient.
  Always review:
  - `max_error_m`, `p95_error_m`
  - `FAILSAFE_TRIGGERED` reasons and timing
  - long drift after failsafe (especially `> 50 m`, critical `> 100 m`)
- Validate both anchor behavior modes when relevant:
  - `POSITION_HEADING` (bearing lock)
  - `POSITION` (position hold without heading lock)
- If investigating reverse/steering behavior, watch `[EVENT] STEER_CLAMP ...` in logs.
