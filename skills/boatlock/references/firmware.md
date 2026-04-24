# Firmware Reference

## Canonical Files

- `boatlock/main.cpp`: hardware wiring, setup/loop orchestration, BLE publish path
- `boatlock/RuntimeControl.h`: mode arbitration and the narrow runtime control contract
- `boatlock/RuntimeGnss.h`: GNSS state, quality gate inputs, correction, bearing/distance cache
- `boatlock/RuntimeMotion.h`: drift/safety state, stepper/motor gating, failsafe application
- `boatlock/RuntimeButtons.h`: BOOT/STOP action decisions built on top of hold-button timing
- `boatlock/RuntimeBleParams.h`: BLE telemetry registry and status notify glue
- `boatlock/RuntimeTelemetryCadence.h`: UI/BLE publish cadence policy
- `boatlock/RuntimeGpsUart.h`: GPS UART warning/restart state machine
- `boatlock/RuntimeSimCommand.h`: on-device HIL command parsing for `SIM_*`
- `boatlock/RuntimeSimExecution.h`: on-device HIL execution outcomes and report chunking
- `boatlock/RuntimeSimLog.h`: stable SIM log line mapping from execution outcomes
- `boatlock/RuntimeSimBadge.h`: simulation result banner state and expiry policy
- `boatlock/RuntimeAnchorNudge.h`: anchor nudge bearing mapping and geodesic projection
- `boatlock/RuntimeAnchorGate.h`: ordered anchor enable denial resolution
- `boatlock/RuntimeSupervisorPolicy.h`: anchor supervisor config/input policy builders
- `boatlock/RuntimeCompassRecovery.h`: compass retry execution and recovery log lines
- `boatlock/RuntimeUiSnapshot.h`: display-facing UI snapshot contract with sim overrides
- `boatlock/HoldButtonController.h`: long-press button state machine used by BOOT/STOP handlers
- `boatlock/BleCommandHandler.h`: accepted BLE ASCII commands and removed-command behavior
- `boatlock/BNO08xCompass.h`: BNO08x init, heading offset, quality telemetry, DCD autosave
- `boatlock/display.cpp`: on-device UI math and sign conventions
- `boatlock/Settings.h`: persisted settings, defaults, ranges, schema version
- `boatlock/AnchorControl.h`: persisted anchor-point writes
- `boatlock/MotorControl.h`: PID tuning and PID persistence throttling

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

## Settings And Persistence

- Settings are stored in EEPROM as:
  - format version
  - raw float value array
  - CRC32 over the values array
- Current schema version is `Settings::VERSION = 0x15`.
- `Settings::set()` clamps values to each key's configured range.
- `Settings::setStrict()` rejects non-finite or out-of-range values and logs `CONFIG_REJECTED`.
- `Settings::load()` may write back on boot if:
  - stored version mismatches
  - CRC mismatches
  - persisted values need normalization
- Do not treat PID auto-save as the only EEPROM writer.
- Current write paths include at least:
  - BLE config and mode commands in `boatlock/BleCommandHandler.h`
  - anchor persistence in `boatlock/AnchorControl.h`
  - PID persistence throttling in `boatlock/MotorControl.h`
  - additional runtime state changes in `boatlock/main.cpp`
- For wear-risk or batching work, audit all `settings.save()` call sites before changing policy.

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

- Core runtime mode precedence is `SIM > MANUAL > ANCHOR > HOLD > IDLE`.
- `main.cpp` should compose one `RuntimeControlInput` and pass it into `RuntimeMotion`.
- `SET_ANCHOR` stores anchor coordinates plus current heading if compass is ready, otherwise heading `0`.
- `ANCHOR_ON` must be denied unless anchor point exists, onboard heading is available, and GNSS quality gate passes.
- `HOLD` is a latched quiet mode entered by emergency stop and stop-style failsafes.
- Explicit operator actions `ANCHOR_ON`, `ANCHOR_OFF`, and manual-mode entry clear the latched `HOLD` state.
- Publish runtime health as:
  - `status`: `OK|WARN|ALERT`
  - `mode`: `IDLE|HOLD|ANCHOR|MANUAL|SIM`
  - `statusReasons`: comma-separated detail flags
- Build UI-facing display state through `RuntimeUiSnapshot` before passing it into `display_draw_ui()`.
- Keep UI refresh cadence and BLE notify cadence in `RuntimeTelemetryCadence`, not as loose timestamps in `main.cpp`.
- Keep GPS UART no-data/stale/restart policy in `RuntimeGpsUart`, not as loose flags in `main.cpp`.
- Keep simulation result badge state in `RuntimeSimBadge`, not as loose latched strings in `main.cpp`.
- Keep anchor supervisor config/input assembly in `RuntimeSupervisorPolicy`, not as inline `settings.get(...)` policy code in `loop()`.
- Keep `SIM_*` command parsing in `RuntimeSimCommand`, not mixed with side effects in `main.cpp`.
- Keep `SIM_*` execution outcomes and report chunking in `RuntimeSimExecution`, not as mixed run/report policy inside `main.cpp`.
- Keep `SIM_*` log line mapping in `RuntimeSimLog`, not as a switch full of formatted strings inside `main.cpp`.
- Keep anchor nudge math and cardinal bearing mapping in `RuntimeAnchorNudge`, not inline in `main.cpp`.
- Keep anchor enable denial precedence in `RuntimeAnchorGate`, not inline in `main.cpp`.
- Keep control-input derivation in `RuntimeControlInputBuilder`, not as inline mode/heading/bearing glue in `loop()`.
- Keep compass retry cadence in `RuntimeCompassRetry`, not as raw `millis()` gating in `main.cpp`.
- Keep compass retry execution and success/failure follow-up in `RuntimeCompassRecovery`, not as open-coded double-init logic in `loop()`.
- `HoldHeading=1` makes bearing equal stored anchor heading.
- Otherwise bearing is computed from GNSS course to anchor.
- Firmware caches anchor bearing for `120000 ms` so auto mode can keep a bearing when GNSS drops briefly.
- `DriftFail` is a containment boundary: once breached, supervisor exits anchor into latched `HOLD` instead of continuing to hunt.
- `manualMode` cancels auto stepper tracking and drives stepper/motor directly from BLE commands.
- Random `fallbackHeading` and `fallbackBearing` are UI placeholders only.

## Safety And Buttons

- STOP button on GPIO `15` is active low.
- STOP press immediately calls the same stop path as BLE `STOP` and latches `HOLD`.
- STOP long-press `3 s` opens the BLE pairing window for `120 s`.
- BOOT button on GPIO `0` only saves the current anchor point after a `1500 ms` hold; it must not arm anchor mode.
- BOOT/STOP actions are resolved through `RuntimeButtons`, with hold timing tracked through `HoldButtonController`.
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

## Historical Notes To Ignore By Default

- Old threads or patches that mention `boatlock.ino`, `boatlock.cpp`, or a `boatlock/include` plus `boatlock/src` split are describing earlier repo shapes.
- `boatlock/TODO.md` is useful as backlog context, but it is stale for multiple implemented features and should not override current code.
