# Firmware Reference

## Canonical Files

- `boatlock/main.cpp`: pinout, runtime source selection, safety flow, BLE params, UI publish path
- `boatlock/BleCommandHandler.h`: accepted BLE ASCII commands and removed-command behavior
- `boatlock/BNO08xCompass.h`: BNO08x init, heading offset, quality telemetry, DCD autosave
- `boatlock/display.cpp`: on-device UI math and sign conventions
- `boatlock/Settings.h`: persisted settings, defaults, ranges, schema version

## Repo Map

- `boatlock/`: production firmware
- `boatlock/debug/`: manual sketches only
- `boatlock/test/`: native unit tests with mocks
- `docs/`: protocol, config, manual-control, release docs
- `tools/sim/`: offline Python simulation harness

## Hardware Baseline

- Main firmware supports BNO08x only.
- Probe order is `0x4B` first, then `0x4A`.
- `sh2_setDcdAutoSave(true)` is enabled during compass init.
- Current default-board pins from `boatlock/main.cpp`:
  - I2C `SDA=47`, `SCL=48`
  - GPS `RX=17`, `TX=18`
  - motor `PWM=7`, direction pins `5/10`
  - BOOT button `0`
  - STOP button `15`
- `BOATLOCK_BOARD_JC4832W535` moves I2C to `4/8` because LCD QSPI uses `47/48`, and uses stepper pins `11/12/13/14`.
- `README.md` may lag current motor-pin assignments; trust `boatlock/main.cpp`.

## GPS And Heading Rules

- GPS position priority:
  1. hardware GPS when `gps.location.isValid()` and age `< MaxGpsAgeMs`
  2. phone GPS from `SET_PHONE_GPS` while age `<= 5000 ms`
  3. otherwise no fix
- `MaxGpsAgeMs` is configurable. Default is `1500 ms`; range is `300..20000`.
- Hardware GPS path:
  - applies moving-average filter window `GpsFWin` (`1..20`)
  - rejects jumps above `MaxPosJumpM`
  - updates speed, HDOP, accel, and sentence counters
- Heading is onboard BNO08x only.
- `headingAvailable()` is true only when `compassReady`.
- There is no phone-heading or `SET_HEADING` fallback in current runtime logic.

## GPS-To-Compass Correction

- Purpose: correct constant yaw bias using movement-derived GPS course.
- Active only when:
  - compass is ready
  - speed is `>= 3.0 km/h`
  - movement since reference point is `>= 4.0 m`
- Behavior:
  - correction target is the wrapped GPS-course minus compass-heading delta
  - clamp to `[-90, +90]`
  - smooth with `alpha=0.18`
  - expire after `180000 ms` without updates

## Anchor And Control Behavior

- `SET_ANCHOR` stores anchor coordinates plus current heading if compass is ready, otherwise heading `0`.
- `HoldHeading=1` makes bearing equal stored anchor heading.
- Otherwise bearing is computed from GNSS course to anchor.
- Firmware caches anchor bearing for `120000 ms` so auto mode can keep a bearing when GNSS drops briefly.
- `manualMode` cancels auto stepper tracking and drives stepper/motor directly from BLE commands.
- Random `fallbackHeading` and `fallbackBearing` are UI placeholders only.

## Safety And Buttons

- STOP button on GPIO `15` is active low.
- STOP press immediately calls the same stop path as BLE `STOP`.
- STOP long-press `3 s` opens the BLE pairing window for `120 s`.
- BOOT button on GPIO `0` saves current anchor and tries to enable anchor mode if GNSS quality is good.
- Supervisor considers sensors healthy only when both GPS fix and heading are available.

## Display Convention

- Boat nose is fixed at the top of the screen.
- Compass card rotation uses `worldDeg + heading`.
- Anchor arrow uses `anchorBearing + heading`.
- If you touch sign math here, verify on real hardware.

## Debug And Build Boundary

- `boatlock/platformio.ini` uses `build_src_filter = +<*.cpp>`, so only top-level `boatlock/*.cpp` are built in production.
- `boatlock/debug/` files are manual-only sketches and should never silently take over production logic.
- USB CDC is enabled in `boatlock/platformio.ini`.
- Only one process can own `/dev/cu.usbmodem2101`; close the serial monitor before flashing or opening another reader.
