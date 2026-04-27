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
- `boatlock/RuntimeUiSnapshot.h`: display-facing UI snapshot contract with sim overrides
- `boatlock/HoldButtonController.h`: long-press button state machine used by BOOT/STOP handlers
- `boatlock/BleCommandHandler.h`: accepted BLE ASCII commands and removed-command behavior
- `boatlock/BNO08xCompass.h`: BNO08x UART-RVC init, frame freshness, heading offset, and inertial telemetry
- `boatlock/BnoRvcFrame.h`: UART-RVC frame checksum, decode, and stream resync parser
- `boatlock/display.cpp`: on-device UI math and sign conventions
- `boatlock/SdCardLogger.cpp`: onboard microSD JSONL diagnostics, queueing, and rotation
- `boatlock/Settings.h`: persisted settings, defaults, ranges, schema version
- `boatlock/AnchorControl.h`: persisted anchor-point writes
- `boatlock/MotorControl.h`: anchor thrust output limiting, ramping, anti-hunt timing, and manual motor output

## Repo Map

- `boatlock/`: production firmware
- `boatlock/debug/`: manual sketches only
- `boatlock/test/`: native unit tests with mocks
- `docs/`: protocol, config, manual-control, release docs
- `tools/sim/`: offline Python simulation harness

## Hardware Baseline

- Main firmware supports BNO08x only.
- Accepted bench compass transport is currently UART-RVC. Full BNO08x DCD/calibration
  work uses the explicit `esp32s3_bno08x_sh2_uart` build target and requires
  rewiring PS0/PS1 plus BNO RX/TX before flashing that target.
- BNO08x UART-RVC is a one-way heading stream and cannot perform SH2 commands
  such as DCD save, dynamic calibration config, or tare.
- Current active development and hardware acceptance target one default ESP32-S3 bench board. Do not add new `BOATLOCK_BOARD_JC4832W535` or other board-specific runtime branches unless explicitly requested.
- Compass integration notes and current bench evidence live in `docs/COMPASS_BNO08X.md`.
- Current default-board pins from `boatlock/main.cpp`:
  - GPS `RX=17`, `TX=18`
  - BNO08x UART-RVC `RX=12`, `TX=11` reserved for SH2 UART, `RST=13`, `baud=115200`
  - BNO08x protocol select wiring: `P0/PS0=3V3`, `P1/PS1=GND`
  - motor `PWM=7`, direction pins `8/10`
  - steering DRV8825 `STEP=6`, `DIR=16`
  - BOOT button `0`
  - STOP button `15`
- The bench board is the Waveshare `ESP32-S3-LCD-2` class board. Vendor demos
  show the onboard TF/microSD slot on SDSPI `SCLK=39`, `MOSI=38`, `MISO=40`,
  `CS=41`. The ST7789 LCD shares `SCLK=39`, `MOSI=38`, `MISO=40` and uses
  `CS=45`, `DC=42`, `BL=1`.
- Current `display.cpp` keeps the already proven LCD write-only config with
  `MISO=-1`. Do not change it just for SD logging unless hardware testing shows
  the shared-bus display path needs it.
- The production SD logger writes JSONL files under `/boatlock/blNNNNN.jsonl`,
  rotates each file at approximately `8 MiB`, and deletes older BoatLock log
  files when free space falls below approximately `8 MiB`. It must remain
  bounded and nonblocking: dropped lines are preferable to blocking control.
- The user-supplied ESP32-S3-LCD-2 header pinout exposes current steering, GPS,
  BNO08x, STOP, motor `PWM=7`, motor `DIR1=8`, and motor `DIR2=10` pins.
  `GPIO5` is not exposed in that pinout, so do not reuse it for powered actuator
  wiring without a separate board-level proof.
- `README.md` may lag current motor-pin assignments; trust `boatlock/main.cpp`.

## Settings And Persistence

- Settings are stored in ESP32 NVS namespace `boatlock_cfg` as:
  - `schema` marker
  - one raw `float` blob per setting key
- Current schema version is `Settings::VERSION = 0x1C`.
- Missing NVS keys keep current defaults and are persisted on boot writeback; removed keys are ignored.
- Older schemas keep existing values by current key, apply explicit versioned migration rules, and update the schema marker plus normalized/missing keys.
- Future schemas are loaded read-only on boot: known current keys may be used in RAM, but firmware must not downgrade the schema marker or write missing/default keys just because an older image booted after OTA rollback.
- Key renames belong in `Settings::applyMigrations()` as explicit legacy-key rules; current legacy rule migrates pre-`0x18` `MaxThrust` to `MaxThrustA` when the current key is missing.
- `Settings` objects must be safe immediately after construction: RAM defaults and key map ready, with no flash write from the constructor.
- `Settings::set()` rejects non-finite values, clamps finite values to each key's configured range, then normalizes by declared type.
- `Settings::setStrict()` rejects non-finite or out-of-range values and logs `CONFIG_REJECTED`.
- `Settings::save()` is dirty-state guarded. Calling it after no-op `set()` calls must not write flash.
- A clean settings object must never commit flash on `save()`.
- `Settings::save()` stages dirty keys and uses one `nvs_commit()` per save.
- `Settings::save()` must check NVS set/commit results. Failed writes log `CONFIG_SAVE_FAILED` and keep dirty state for a later retry.
- `Settings::load()` may write back on boot if:
  - stored schema is missing or mismatched
  - persisted keys are missing or unreadable
  - persisted values need normalization
- `Settings::load()` must not write back on boot when stored schema is newer than the firmware schema.
- Current write paths include at least:
  - BLE config and mode commands in `boatlock/BleCommandHandler.h`
  - anchor persistence in `boatlock/AnchorControl.h`
  - additional runtime state changes in `boatlock/main.cpp`
- For wear-risk or batching work, audit all `settings.save()` call sites before changing policy.

## GPS And Heading Rules

- GPS position priority:
  1. hardware GPS when `gps.location.isValid()` and age `< MaxGpsAgeMs`
  2. phone GPS from `SET_PHONE_GPS` while age `<= 5000 ms`
  3. otherwise no fix
- Control GNSS is hardware-only. Phone GPS fallback is a UI/telemetry fallback and must not pass anchor quality gate, save BOOT anchor points, or reuse hardware HDOP/sentence metrics.
- Hardware and phone GNSS inputs must validate coordinates at the `RuntimeGnss` boundary; `NaN`, out-of-range, and `0/0` must not become a live fix.
- `MaxGpsAgeMs` is configurable. Default is `1500 ms`; range is `300..20000`.
- Hardware GPS path:
  - applies moving-average filter window `GpsFWin` (`1..20`)
  - rejects the first jump above `MaxPosJumpM`
  - accepts a repeated stable jump candidate as the new baseline to avoid permanent lockout after real relocation or cold-start drift
  - updates speed, HDOP, accel, and sentence counters
- GNSS quality gate must fail closed on invalid/non-finite config or sample values; a bad threshold or `NaN` motion sample must not silently pass anchor pre-enable.
- Phone GPS fallback must never seed hardware GNSS filter, jump baseline, speed baseline, or acceleration baseline.
- When GNSS leaves hardware source or loses fix, reset hardware motion/filter baselines so reacquisition starts from fresh trusted hardware data.
- GNSS motion freshness must use explicit sample-valid flags. Timestamp `0` is a valid sample time, not a "no sample" sentinel.
- Anchor pre-enable treats HDOP as a required quality metric. Missing, non-finite, or zero HDOP maps to `GPS_HDOP_MISSING`, not to a pass.
- GNSS status reasons must preserve exact gate names end-to-end, including `GPS_DATA_STALE`, `GPS_POSITION_JUMP`, and `GPS_HDOP_MISSING`.
- Heading is onboard BNO08x UART-RVC only.
- `headingAvailable()` is true only when BNO08x is initialized and fresh heading frames are arriving.
- BNO08x UART-RVC frames must pass header/checksum and datasheet angle ranges before they update heading state.
- BNO08x event freshness must use explicit "event seen" flags plus unsigned elapsed-time math. Timestamp `0` is a valid event time, not a "no event" sentinel.
- If the BNO08x reset GPIO logs `pulse=1`, still require fresh heading frames afterward; GPIO toggling alone does not prove sensor recovery.
- There is no phone-heading or `SET_HEADING` fallback in current runtime logic.

## GPS-To-Compass Correction

- Purpose: correct constant yaw bias using movement-derived GPS course.
- Only hardware GPS samples may seed or update this correction. Phone GPS fallback is telemetry/display only and must not influence corrected control heading.
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
- `SET_ANCHOR` stores valid non-zero anchor coordinates plus current heading only if fresh heading is available, otherwise heading `0`.
- Anchor-point persistence belongs in `AnchorControl`; saving a point must require an explicit `enableAnchor` argument and must reject invalid/non-finite coordinates instead of clamping them.
- Anchor heading normalization must use bounded math; do not use repeated `while` loops that can hang on very large finite headings.
- Anchor-point persistence must return failure when the settings commit fails, restore previous settings RAM values, and leave the live anchor point unchanged.
- Loaded anchor coordinates must be validated as a pair; invalid/default coordinates clear the live anchor point and heading.
- `ANCHOR_ON` must be denied unless anchor point exists, onboard heading is available, and GNSS quality gate passes.
- `HOLD` is a latched quiet mode entered by emergency stop and stop-style failsafes.
- Explicit operator actions `ANCHOR_ON`, `ANCHOR_OFF`, and manual-mode entry clear the latched `HOLD` state.
- Stop-style failsafes should have one owner for clearing anchor, saving settings, stopping outputs, and logging. Do not pre-clear `AnchorEnabled` before calling the shared stop path because it hides the previous state from `ANCHOR_OFF` event logging.
- Publish runtime health as:
  - `status`: `OK|WARN|ALERT`
  - `mode`: `IDLE|HOLD|ANCHOR|MANUAL|SIM`
  - `statusReasons`: comma-separated detail flags
- Runtime health severity predicates must stay separated: `HOLD`, `DRIFT_FAIL`, and active failsafe reasons are `ALERT`; degraded sensors/GNSS/anchor-denied reasons are `WARN`; informational acknowledgements such as `NUDGE_OK` stay `OK`.
- `statusReasons` tokens are phone/BLE-facing protocol values: keep them single-token, bounded, and free of comma/control characters before publishing.
- `statusReasons` may include informational operator acknowledgements such as `NUDGE_OK`; those must not elevate `status` above `OK` without a real health warning.
- Build UI-facing display state through `RuntimeUiSnapshot` before passing it into `display_draw_ui()`.
- Keep UI refresh cadence and BLE notify cadence in `RuntimeTelemetryCadence`, not as loose timestamps in `main.cpp`.
- Runtime cadence timers must share unsigned elapsed-time logic; interval `0` is floored to `1 ms` so callers cannot create same-timestamp bursts, and rollover coverage is required.
- Keep GPS UART no-data/stale/restart policy in `RuntimeGpsUart`, not as loose flags in `main.cpp`.
- GPS UART restart must reset the no-data baseline so post-restart silence gets a fresh grace window instead of an immediate no-data warning.
- GPS UART restart cooldown must use an explicit restart-seen flag; timestamp `0` is a valid restart time, not a sentinel.
- GPS UART watchdog timing config must have local minimum floors; zero values must not create no-data or restart thrash.
- Runtime logs should be formatted into a bounded buffer and emitted with the known formatted byte count; do not rely on C-string scanning after truncation.
- Keep simulation result badge state in `RuntimeSimBadge`, not as loose latched strings in `main.cpp`.
- Simulation result badge expiry uses start-time plus duration with unsigned elapsed-time math. Duration `0` means hidden, and rollover behavior must be covered by direct tests.
- Keep anchor supervisor config/input assembly in `RuntimeSupervisorPolicy`, not as inline `settings.get(...)` policy code in `loop()`.
- `RuntimeSupervisorPolicy` must use `AnchorSupervisor` minimum timeout constants instead of duplicating floor literals.
- Core failsafe modules must apply local fail-closed timeout floors instead of relying only on upstream settings clamps.
- Input fields that represent current control activity must refresh the matching core deadline, or the field should be removed.
- Keep `SIM_*` command parsing in `RuntimeSimCommand`, not mixed with side effects in `main.cpp`.
- `SIM_*` parser accepts only documented command names and payload forms. `SIM_RUN` payload is `SIM_RUN:<scenario_id>[,<0|1>]`; unsupported JSON/space payloads and prefix lookalikes are rejected.
- Keep `SIM_*` execution outcomes and report chunking in `RuntimeSimExecution`, not as mixed run/report policy inside `main.cpp`.
- Failed or malformed `SIM_RUN` must not clear runtime failsafe/anchor-denial state or stop live motion; those state-clearing side effects are allowed only after a simulation run actually starts.
- Keep `SIM_*` log line mapping in `RuntimeSimLog`, not as a switch full of formatted strings inside `main.cpp`.
- `RuntimeSimLog` output must stay single-line and bounded. Sanitize CR/LF/TAB/NUL/control characters before forwarding SIM outcome fields into logs.
- Keep anchor nudge math and cardinal bearing mapping in `RuntimeAnchorNudge`, not inline in `main.cpp`.
- Nudge/jog uses a fixed small `1.5 m` step. Do not re-add arbitrary distance parameters without a product reason and tests.
- Anchor nudge projection must be fail-atomic: rejected source/bearing/output values must not leave a partially updated target point.
- Keep anchor enable denial precedence in `RuntimeAnchorGate`, not inline in `main.cpp`.
- Keep control-input derivation in `RuntimeControlInputBuilder`, not as inline mode/heading/bearing glue in `loop()`.
- Runtime control-input builders must validate numeric availability at the boundary; non-finite heading or bearing values become unavailable and zeroed before motion code sees them.
- Invalid distance input in auto-control mode should make control GPS unavailable and clamp distance to safe neutral; it must not be propagated as `NaN`, negative distance, or a fake zero-error fix.
- Keep compass retry cadence in `RuntimeCompassRetry`, not as raw `millis()` gating in `main.cpp`.
- Keep compass event freshness policy in `RuntimeCompassHealth`, not as loose age checks spread through runtime code.
- Compass health timeouts must apply local fail-closed floors; zero or tiny timeout input must not disable first-event or stale-event detection.
- Runtime sensor watchdog timers must use unsigned elapsed-time math so `millis()` rollover does not disable stale/no-data detection.
- Diagnostic timers must track event presence separately from timestamp values; `0` is a valid `millis()` sample.
- Diagnostic saturation checks must reject invalid non-positive actuator limits before comparing output against the limit.
- HIL/control-loop timers follow the same explicit-seen-flag rule; timestamp `0` is valid for GNSS quality windows, control-loop deadlines, sensor timers, and thrust timing.
- HIL core modules need direct unit tests for boundary timing behavior; scenario-level tests alone are not enough.
- `HoldHeading=1` makes bearing equal stored anchor heading.
- Otherwise bearing is computed from GNSS course to anchor.
- Firmware caches anchor bearing for `120000 ms` so auto mode can keep a bearing when GNSS drops briefly.
- `DriftFail` is a containment boundary: once breached, supervisor exits anchor into latched `HOLD` instead of continuing to hunt.
- Drift/containment thresholds must sanitize non-finite input before comparisons.
- Drift-speed telemetry must reset after long data gaps instead of surviving as stale motion evidence.
- Manual mode cancels auto stepper tracking and drives stepper/motor through shared `ManualControl` state.
- Manual control is entered/refreshed atomically and expires through a short deadman TTL; split mode/dir/speed state is not allowed.
- Manual steering target is an angle in degrees relative to bow-forward, clamped to `-90..90`; the stepper moves toward that target and is canceled by TTL expiry, `MANUAL_OFF`, STOP, or non-manual quiet paths.
- Manual control activation must require an explicit source; `NONE` is never a valid live controller.
- Invalid manual packets must not refresh the deadman lease.
- Manual deadman behavior must be tested for timestamp zero and unsigned `millis()` rollover.
- Motor output must stay bounded and deterministic. Do not reintroduce hidden runtime self-adaptive PID tuning or PID auto-persistence in the actuator path.
- Motor auto-thrust must fail closed when distance or tuning inputs are non-finite; do not let `NaN` pass into clamp/ramp math.
- Manual and anchor-auto motor state must stay isolated. Manual PWM, timestamps, and ramp state must not seed anchor-auto output.
- Motor stop/zero paths should drive PWM to zero and direction pins to a known idle state.
- Runtime motion must validate auto-control settings as finite before using them for heading alignment, thrust limits, or ramp policy.
- Runtime supervisor config must clamp/fail closed before timeout or command-limit values enter failsafe decisions.
- Manual control is a source-owned deadman lease. Same-source updates may refresh it; competing sources wait until TTL expiry or explicit manual stop.
- Stepper control uses `AccelStepper::DRIVER` for the DRV8825-compatible
  STEP/DIR path. `GPIO6` is STEP and `GPIO16` is DIR for the current bench
  wiring, using the two former ULN2003 header positions closest to board center.
- Steering geometry is `7200` output steps/rev: Vanchor uses a `36:1` gearbox
  and its Arduino/config baseline uses a `200` step/rev motor.
  If DRV8825 microstepping is wired active, multiply this by the microstep
  factor and update firmware/docs together.
- Current default steering tuning is `StepMaxSpd=1200` and `StepAccel=800`;
  schema migration preserves custom values but updates the untouched old
  `700/250` pair.
- Stepper control must fail closed on neutral/invalid manual input, use bounded angle normalization, and release coils after idle/cancel through a deterministic timer.
- Stepper idle release timing must use explicit active state; `idleSinceMs == 0` is a valid timestamp, not a sentinel.
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
- Anchor arrow uses `anchorBearing - heading`.
- Historical regression note: the anchor arrow was `anchorBearing - heading` before `2a4fa3d0`; do not change it back to `+ heading` unless a real hardware sign test proves the BNO/screen frame changed.
- If you touch sign math here, verify on real hardware.

## Debug And Build Boundary

- `boatlock/platformio.ini` uses `build_src_filter = +<*.cpp>`, so only top-level `boatlock/*.cpp` are built in production.
- `boatlock/debug/` files are manual-only sketches and should never silently take over production logic.
- USB CDC is enabled in `boatlock/platformio.ini`.
- Only one process can own `/dev/cu.usbmodem2101`; close the serial monitor before flashing or opening another reader.
- BLE OTA is the preferred no-USB firmware update path after the first seed flash.
- BLE OTA command flow is `OTA_BEGIN:<size>,<sha256>` on `56ef`, sequential binary chunks on `9abc`, then `OTA_FINISH` on `56ef`.
- `OTA_BEGIN` must put runtime outputs in a stopped safe state before accepting chunks. During active OTA, reject normal runtime commands and abort on BLE disconnect.
- The phone app must verify the downloaded `firmware.bin` against an expected SHA-256 before transfer. Firmware validates byte count and SHA-256 before finalizing the OTA partition and rebooting.

## Historical Notes To Ignore By Default

- Old threads or patches that mention `boatlock.ino`, `boatlock.cpp`, or a `boatlock/include` plus `boatlock/src` split are describing earlier repo shapes.
- `boatlock/TODO.md` is useful as backlog context, but it is stale for multiple implemented features and should not override current code.
