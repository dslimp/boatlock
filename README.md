# BoatLock

BoatLock is an ESP32-S3 anchor-hold controller with a Flutter companion app.
The current development target is a single bench board running the main anchor
runtime, BLE phone control, onboard BNO08x heading, GNSS telemetry, manual
deadman control, and service maintenance flows.

Current firmware release: `0.2.0`

## What It Does

- Saves the current GNSS position as an anchor point.
- Holds position around the saved point using GNSS distance and onboard BNO08x
  heading.
- Supports phone manual control through the same deadman path planned for a
  future external BLE controller.
- Drives the thruster with PWM plus two direction pins.
- Drives steering through a DRV8825-compatible STEP/DIR driver.
- Streams compact BLE telemetry to the app and mirrors firmware logs over the
  BLE log characteristic.
- Supports service operations from the app: stepper tuning, compass service
  controls, and BLE OTA firmware update.
- Provides deterministic on-device HIL scenarios (`SIM_*`, `S0..S19`) for
  regression checks without relying on live sensors.

The BLE protocol is documented in [docs/BLE_PROTOCOL.md](docs/BLE_PROTOCOL.md).

## Repository Layout

- `boatlock/` - ESP32-S3 firmware, built with PlatformIO.
- `boatlock_ui/` - Flutter app for Android, macOS, and other Flutter targets.
- `docs/` - protocol, hardware, release, and powered-bench notes.
- `tools/hw/nh02/` - tracked hardware-bench wrappers for flashing, OTA, and
  Android BLE smoke checks.
- `tools/android/` - Android app build and smoke helpers.
- `tools/sim/` - offline anchor simulation harness.

## Current Bench Hardware

`boatlock/main.cpp` is the source of truth for the active pinout.

| Function | ESP32-S3 pin | Notes |
| --- | ---: | --- |
| GPS RX | GPIO17 | connect GPS TX |
| GPS TX | GPIO18 | connect GPS RX |
| BNO08x UART-RVC RX | GPIO12 | connect BNO08x RVC TX/SDA |
| BNO08x reset | GPIO13 | firmware reset pulse |
| BNO08x P0/PS0 | 3V3 | selects UART-RVC |
| BNO08x P1/PS1 | GND | selects UART-RVC |
| Thruster PWM | GPIO7 | PWM output |
| Thruster direction | GPIO8, GPIO10 | H-bridge direction inputs |
| Steering DRV8825 STEP | GPIO6 | STEP input |
| Steering DRV8825 DIR | GPIO16 | DIR input |
| BOOT anchor-save button | GPIO0 | debounced long-press |
| STOP button | GPIO15 | hardware stop / pairing window |

The steering geometry is set for the Vanchor gearbox:

- motor steps per revolution: `200`
- gearbox ratio: `36:1`
- output steps per steering revolution: `7200`
- default `StepMaxSpd`: `1200`
- default `StepAccel`: `800`

If the DRV8825 `MODE0/MODE1/MODE2` pins are strapped for microstepping, the
mechanical speed is divided by that microstep factor unless the geometry and
speed limits are intentionally retuned.

Complete the powered checks before connecting real motor load:

- [docs/POWERED_BENCH_CHECKLIST.md](docs/POWERED_BENCH_CHECKLIST.md)
- [docs/STEERING_DRIVER_INTAKE.md](docs/STEERING_DRIVER_INTAKE.md)
- [docs/BRUSHED_MOTOR_DRIVER_INTAKE.md](docs/BRUSHED_MOTOR_DRIVER_INTAKE.md)

## Firmware Build

Use PlatformIO from `boatlock/`.

```bash
cd boatlock
pio run -e esp32s3_service
```

Useful environments:

- `esp32s3` - release-compatible default build.
- `esp32s3_service` - bench/service build with BLE OTA and service commands.
- `esp32s3_release` - explicit release profile.
- `esp32s3_acceptance` - broad bench acceptance profile.

For active bench development we normally use `esp32s3_service`, because the app
needs OTA and tuning commands while hardware is still being adjusted.

## Flashing And OTA

Normal firmware update for the moved hardware is phone-bridged BLE OTA through
the production app:

```bash
cd boatlock
pio run -e esp32s3_service
cd ..
tools/hw/nh02/android-run-app-e2e.sh \
  --ota \
  --ota-firmware boatlock/.pio/build/esp32s3_service/firmware.bin \
  --wait-secs 1800
```

Use the longer OTA wait when BLE discovery may be cold. The wrapper timeout
starts before scan/connect and can otherwise expire during an active transfer.

USB flash on the `nh02` bench is the seed/recovery path:

```bash
tools/hw/nh02/flash.sh --profile service
```

## Flutter App

The app wrappers build one release app. Service controls are part of that app
and stay hidden during normal use; open Settings and enable `Сервисный режим`
to show stepper tuning, compass service rows, and firmware OTA.

Android build:

```bash
tools/android/build-app-apk.sh
```

macOS build:

```bash
tools/macos/build-app.sh
```

Run from source:

```bash
cd boatlock_ui
flutter pub get
flutter run
```

## Validation

Firmware tests:

```bash
cd boatlock
platformio test -e native
```

Flutter checks:

```bash
cd boatlock_ui
flutter test
flutter analyze
```

High-signal targeted checks used during bench work:

```bash
cd boatlock
platformio test -e native -f test_settings -f test_runtime_ble_command_log -f test_ble_command_handler -f test_stepper_control -f test_runtime_motion

cd ../boatlock_ui
flutter test test/settings_page_test.dart
flutter test test/settings_page_service_ui_test.dart
```

Android production-app manual smoke on `nh02`:

```bash
tools/hw/nh02/android-run-app-e2e.sh --manual --wait-secs 130
```

This sends zero-throttle `MANUAL_TARGET`, observes `MANUAL`, sends `MANUAL_OFF`,
and verifies the firmware returns to a quiet mode. It is not a powered thrust
test.

## Runtime Rules

- Heading comes only from onboard BNO08x UART-RVC frames.
- Control GNSS is hardware GPS only; phone GPS is telemetry/UI fallback and does
  not pass the anchor-control quality gate.
- `ANCHOR_ON` requires a saved anchor point, fresh onboard heading, and GNSS
  quality.
- Manual control uses atomic `MANUAL_TARGET:<angleDeg>,<throttlePct>,<ttlMs>`
  plus `MANUAL_OFF`, with `angleDeg=-90..90` across the steering arc.
- Runtime faults and STOP enter safe quiet output; they do not automatically
  resume Anchor or Manual.
- BLE security is owner-secret based; paired devices must wrap control commands
  in `SEC_CMD`.

## License

This project is licensed under the terms of the [MIT License](LICENSE).
