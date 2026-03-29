# BoatLock Agent Notes

## Repo Skill

- Use the repo-local skill at `skills/boatlock/SKILL.md` for any BoatLock task.
- Load only the reference file you need:
  - firmware/runtime/hardware: `skills/boatlock/references/firmware.md`
  - BLE/UI/security: `skills/boatlock/references/ble-ui.md`
  - build/test/release workflow: `skills/boatlock/references/validation.md`

## Canonical Sources

- Treat code as the source of truth when prose docs disagree.
- Runtime, pinout, source selection, safety, and BLE params:
  - `boatlock/main.cpp`
  - `boatlock/BNO08xCompass.h`
  - `boatlock/display.cpp`
  - `boatlock/Settings.h`
- BLE transport and accepted command surface:
  - `boatlock/BLEBoatLock.cpp`
  - `boatlock/BleCommandHandler.h`
- Flutter BLE client and parsing:
  - `boatlock_ui/lib/ble/ble_boatlock.dart`
  - `boatlock_ui/lib/models/boat_data.dart`
- Protocol and schema docs:
  - `docs/BLE_PROTOCOL.md`
  - `docs/CONFIG_SCHEMA.md`
- Known stale areas:
  - historical compass/phone-heading notes may lag the current code
  - the HC 160A direction pin snippet in `README.md` is not the current source of truth; trust `boatlock/main.cpp`

## Repo Layout

- `boatlock/`: ESP32-S3 firmware built with PlatformIO
- `boatlock_ui/`: Flutter mobile/desktop client
- `docs/`: BLE protocol, config schema, manual control, release notes
- `tools/sim/`: offline Python anchor simulation harness
- `tools/ci/`: version/schema/release-note checks and shell lint helpers

## Hardware And Runtime Invariants

- Compass support in main firmware is `BNO08x` only.
- Do not reintroduce `HMC5883`, phone-heading fallback, route subsystem, or removed log-export flows unless the task explicitly restores them end-to-end.
- Current firmware pinout in `boatlock/main.cpp`:
  - default board:
    - I2C `SDA=47`, `SCL=48`
    - GPS `RX=17`, `TX=18`
    - motor `PWM=7`, direction pins `5/10`
    - BOOT button `0`
    - STOP button `15`
  - `BOATLOCK_BOARD_JC4832W535`:
    - I2C `SDA=4`, `SCL=8` because LCD QSPI occupies `47/48`
    - stepper pins `11/12/13/14`
- BNO08x probing order is `0x4B`, then `0x4A`.
- Firmware enables BNO runtime calibration and DCD autosave.
- On boot and on failed compass retries, firmware prints I2C inventory as `[I2C] ...`.
- Production firmware build compiles only top-level `boatlock/*.cpp`.
- Files under `boatlock/debug/` are manual test sketches and must not silently replace production logic.
- USB CDC is enabled in `boatlock/platformio.ini`; only one process can own `/dev/cu.usbmodem2101` at a time.

## GPS, Heading, And Control Rules

- GPS source priority:
  1. hardware GPS when `gps.location.isValid()` and age is below `MaxGpsAgeMs`
  2. phone GPS from `SET_PHONE_GPS` while age is `<= 5000 ms`
  3. otherwise no fix
- `MaxGpsAgeMs` is a setting, not a hard-coded constant. Default is `1500 ms`; allowed range is `300..20000`.
- Hardware GPS path applies moving-average filtering via `GpsFWin` (`1..20`) and rejects jumps above `MaxPosJumpM`.
- Heading comes from onboard BNO08x only. `headingAvailable()` is true only when `compassReady`.
- Commands such as `SET_HEADING`, `EMU_COMPASS`, and `CALIB_COMPASS` are removed/no-op territory in the current product surface.
- GPS-to-compass yaw correction is active only when:
  - compass is ready
  - speed is `>= 3.0 km/h`
  - movement since reference point is `>= 4.0 m`
- GPS heading correction behavior:
  - target correction is clamped to `[-90, +90]`
  - smoothing `alpha=0.18`
  - correction expires after `180000 ms` without updates
- `SET_ANCHOR` stores the current heading if compass is ready, otherwise `0`.
- `HoldHeading=1` makes anchor bearing come from stored anchor heading.
- Otherwise anchor bearing comes from GNSS course to anchor, with a `120 s` cache if GNSS temporarily drops.
- `fallbackHeading` and `fallbackBearing` are UI placeholders only, not real sensor fallbacks.

## Display, BLE, And Security

- Display convention:
  - boat nose is fixed at the top of the screen
  - compass card rotation uses `worldDeg + heading`
  - anchor arrow uses `anchorBearing + heading`
- BLE identity is fixed:
  - device name `BoatLock`
  - service `12ab`
  - data `34cd`
  - command `56ef`
  - log `78ab`
- Boot logs should include:
  - `[BLE] init ...`
  - `[BLE] advertising started|failed`
- Flutter device match is true when:
  - advertised or platform name equals/contains `boatlock`, or
  - advertised service UUID contains `12ab`
- Flutter stops scanning before connect and retries scan/reconnect every `3 s`.
- App heartbeat is sent every `2 s` once connected.
- Security flow:
  - hardware STOP long-press `3 s` opens pairing window
  - pairing window duration is `120 s`
  - pairing uses `PAIR_SET`
  - auth flow is `AUTH_HELLO` -> read `secNonce` -> `AUTH_PROVE`
  - when `secPaired=1`, control/write commands must be wrapped in `SEC_CMD`
- `SET_STEP_SPR` is compatibility-only and remains fixed at `4096`.

## Simulation And Validation

- On-device HIL scenarios `S0..S19` are part of the product surface and regression suite.
- `SIM_*` commands are handled in the normal BLE command path.
- Offline anchor simulation lives in `tools/sim/`.
- Native firmware tests live under `boatlock/test/`.
- If you change BLE commands, telemetry keys, settings schema, or simulation behavior, update code, tests, and docs together.

## Local Commands

- Firmware build:
  - `cd boatlock && pio run -e esp32s3`
- Alt-board build:
  - `cd boatlock && pio run -e jc4832w535`
- Native firmware tests:
  - `cd boatlock && platformio test -e native`
- Flutter tests:
  - `cd boatlock_ui && flutter test`
- Offline simulation:
  - `python3 tools/sim/test_sim_core.py`
  - `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json`
- CI/version helpers:
  - `python3 tools/ci/check_firmware_version.py`
  - `python3 tools/ci/check_config_schema_version.py`
  - `pytest tools/ci/test_*.py`
