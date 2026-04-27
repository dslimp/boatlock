# BNO08x Compass Integration

## Production Rule

Current accepted bench firmware uses BNO08x UART-RVC. A SH2-UART build target
exists for the DCD/calibration migration, but it must not be flashed as the
bench acceptance target until the module is physically rewired out of RVC mode.

Runtime heading is valid only when fresh UART-RVC frames are arriving. A boot
line, reset pulse, cached heading, or historical I2C address visibility is not
enough for anchor control.

## Current nh02 Wiring

- BNO08x `SDA/TX` -> ESP32-S3 `GPIO12` (`RX` only)
- BNO08x `RST` -> ESP32-S3 `GPIO13`
- BNO08x `P0/PS0` -> `3V3`
- BNO08x `P1/PS1` -> `GND`
- UART baud: `115200`
- Frame format: `0xAA 0xAA` sync, 17-byte payload, checksum over payload bytes

## SH2-UART DCD Migration Wiring

Use this only when switching from the simple RVC stream to full SH2 command mode:

- BNO08x `TXO` -> ESP32-S3 `GPIO12` (`RX`)
- BNO08x `RXI` -> ESP32-S3 `GPIO11` (`TX`)
- BNO08x `RST` -> ESP32-S3 `GPIO13`
- BNO08x `P0/PS0` -> `GND`
- BNO08x `P1/PS1` -> `3V3`
- UART baud: `3000000`
- Build target: `cd boatlock && pio run -e esp32s3_bno08x_sh2_uart`

SparkFun's protocol-selection table is the source of truth for the PS pins:
`PS0=1,PS1=0` is UART-RVC; `PS0=0,PS1=1` is SH2 UART; `PS0=1,PS1=1` is SPI.

The SH2 build enables rotation vector, calibrated magnetometer, calibrated gyro,
accelerometer, dynamic calibration config, DCD save, DCD autosave control, and
Z-axis tare commands. Do not run `COMPASS_TARE_Z` casually: it changes the
heading basis and can make anchor control point the wrong way.

`GPIO12` and `GPIO13` were verified on the `nh02` bench with the tracked probe
firmware. After the BNO08x header contacts were re-soldered, the RVC probe read
thousands of valid frames with stable yaw/pitch/roll and checksum OK.

## Firmware Contract

- `BNO08xCompass.h` owns UART-RVC parsing, heading offset, freshness, pitch,
  roll, and acceleration telemetry.
- In the SH2-UART build, `BNO08xCompass.h` owns the SHTP UART frame transport,
  rotation-vector heading, quality fields, dynamic calibration, DCD save, and
  tare commands.
- `BnoRvcFrame.h` owns the small frame parser and native unit coverage.
- `headingAvailable()` uses `lastHeadingEventAgeMs`.
- `FIRST_EVENT_TIMEOUT` and `EVENT_STALE` both make compass unavailable.
- Retry pulses `GPIO13`, reopens UART-RVC, and waits for fresh frames.
- RVC does not expose SH2 calibration quality, RV accuracy, magnetometer norm,
  or gyro norm; those telemetry fields are intentionally reported as neutral
  placeholders.

## Validation

Local:

- `cd boatlock && platformio test -e native -f test_bno_rvc_frame -f test_runtime_compass_health`
- `cd boatlock && platformio test -e native -f test_bno08x_compass`
- `cd boatlock && platformio test -e native -f test_ble_command_handler`
- `pytest tools/hw/nh02/test_acceptance.py`
- `cd boatlock && pio run -e esp32s3`
- `cd boatlock && pio run -e esp32s3_bno08x_sh2_uart`

Hardware:

- `tools/hw/nh02/flash.sh`
- `tools/hw/nh02/acceptance.sh --seconds 180 --log-out /tmp/boatlock-compass.log --json-out /tmp/boatlock-compass.json`

Acceptance requires both lines in the same clean capture:

- `[COMPASS] ready=1 source=BNO08x-RVC rx=12 baud=115200`
- `[COMPASS] heading events ready ...`

Android:

- `tools/hw/nh02/android-run-app-check.sh --compass --wait-secs 130`

On current RVC wiring this Android smoke proves only BLE command delivery and
device-log acknowledgements (`ok=0 source=BNO08x-RVC`). DCD/tare success requires
SH2-UART wiring and the explicit SH2 build target.

Acceptance remains red if later logs contain `COMPASS lost`, `COMPASS retry
ready=0`, Arduino `[E]` logs, panic/assert/backtrace, or BLE/display boot
failures.

## Historical Note

The previous I2C/SH2 implementation was rejected for this hardware path after
repeated `nh02` failures: visible `0x4B`, then `Wire.cpp requestFrom()` errors,
missing heading events, and failed retries. That context remains in `WORKLOG.md`
only; it is not a production compatibility path.

## References

- Adafruit BNO085 guide: https://learn.adafruit.com/adafruit-9-dof-orientation-imu-fusion-breakout-bno085
- Adafruit UART-RVC guide: https://learn.adafruit.com/adafruit-9-dof-orientation-imu-fusion-breakout-bno085/uart-rvc-for-arduino
- BNO080 datasheet mirror: https://www.digikey.com/en/htmldatasheets/production/3210483/0/0/1/bno080
