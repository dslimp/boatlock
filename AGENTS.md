# BoatLock Agent Notes

This file captures non-obvious project behavior that must stay stable unless explicitly changed.

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

1. If `EmuCompass=1`, always use phone heading from `SET_HEADING`.
2. Else if onboard compass is ready, use BNO08x heading (+ optional GPS correction).
3. Else use fresh phone heading fallback if age `< 3000 ms`.
4. Else heading is unavailable.

Related params exposed over BLE:
- `heading`, `headingRaw`, `compassOffset`, `gpsHdgCorr`, `compassQ`, `rvAcc`, `magNorm`, `gyroNorm`, `magQ`, `gyroQ`, `pitch`, `roll`.

## GPS-to-Compass Heading Correction

Purpose: reduce constant yaw bias using movement-based GPS course.

Active only when:
- onboard compass is ready
- `EmuCompass=0`
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
