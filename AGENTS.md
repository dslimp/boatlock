# BoatLock Agent Notes

## Repo Skill

- Use the repo-local skill at `skills/boatlock/SKILL.md` for any BoatLock task.
- Use `skills/boatlock-hardware-acceptance/SKILL.md` for `nh02` bench acceptance, phone BLE OTA flashing, serial log validation, and Android USB/BLE smoke planning.
- Load only the reference file you need:
  - firmware/runtime/hardware: `skills/boatlock/references/firmware.md`
  - BLE/UI/security: `skills/boatlock/references/ble-ui.md`
  - build/test/release workflow: `skills/boatlock/references/validation.md`
  - comparative architecture / external research: `skills/boatlock/references/external-patterns.md`

## Canonical Sources

- Treat code as the source of truth when prose docs disagree.
- Runtime, pinout, source selection, safety, and BLE params:
  - `boatlock/main.cpp`
  - `boatlock/RuntimeControl.h`
  - `boatlock/RuntimeGnss.h`
  - `boatlock/RuntimeMotion.h`
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
  - `boatlock/TODO.md` is a backlog, not a runtime source of truth
  - old threads that mention `boatlock.ino`, `boatlock.cpp`, or `boatlock/include` + `boatlock/src` describe earlier repo shapes, not the current layout

## Repo Layout

- `boatlock/`: ESP32-S3 firmware built with PlatformIO
- `boatlock_ui/`: Flutter mobile/desktop client
- `docs/`: BLE protocol, config schema, manual control, release notes
- `tools/sim/`: offline Python anchor simulation harness
- `tools/ci/`: version/schema/release-note checks and shell lint helpers

## Product Scope

- Current target for `main`: the app and anchor-point hold.
- Do not keep backward compatibility in code for any component unless the user explicitly asks for it in the current task.
- Because the product is not released, remove obsolete commands, fields, shims, and UI flows instead of preserving legacy behavior; record historical context only in `WORKLOG.md`.
- `main` must stay releasable for:
  - Flutter app connect/reconnect over BLE
  - set anchor, enable anchor, disable anchor, emergency stop
  - manual control from the phone and a future external BLE joystick/controller
  - anchor hold based on GNSS + onboard BNO08x heading
  - safety/failsafe behavior required for real operation
  - minimal status needed to understand link, fix, heading, anchor, and safety state
- Keep runtime orchestration simple:
  - `main.cpp` should stay as setup/loop glue
  - mode arbitration belongs in `boatlock/RuntimeControl.h`
  - GNSS state and bearing cache belong in `boatlock/RuntimeGnss.h`
  - motion, drift, and failsafe actuation belong in `boatlock/RuntimeMotion.h`
- Supporting scope that stays in `main` even if it is not end-user UI:
  - test and validation infrastructure
  - offline simulation in `tools/sim/`
  - native/unit tests and CI checks
  - security/pairing/auth required for the shipping control path
- Default out-of-scope for `main` unless explicitly requested and justified:
  - production-exposed dev/diagnostic BLE surfaces not needed for anchor operation
  - broad tuning/calibration UI for non-essential runtime params
  - experimental profiles and convenience features that widen the command surface
- If a feature is useful but not core, keep it behind a dedicated dev/service path or move it to a feature branch before cutting it from `main`.

## Hardware And Runtime Invariants

- Compass support in main firmware is `BNO08x` only.
- Do not reintroduce `HMC5883`, phone-heading fallback, route subsystem, or removed log-export flows unless the task explicitly restores them end-to-end.
- Production BNO08x transport is UART-RVC only; do not reintroduce ESP32-S3 I2C/SH2 compass code or compatibility shims unless explicitly requested.
- Current active development and hardware acceptance target one default ESP32-S3 bench board. Do not add new `BOATLOCK_BOARD_JC4832W535` or other board-specific runtime branches unless explicitly requested.
- Current firmware pinout in `boatlock/main.cpp`:
  - default board:
    - GPS `RX=17`, `TX=18`
    - BNO08x UART-RVC `RX=12`, `RST=13`, `baud=115200`
    - BNO08x protocol select wiring: `P0/PS0=3V3`, `P1/PS1=GND`
    - motor `PWM=7`, direction pins `5/10`
    - BOOT button `0`
    - STOP button `15`
- Production firmware build compiles only top-level `boatlock/*.cpp`.
- Files under `boatlock/debug/` are manual test sketches and must not silently replace production logic.
- USB CDC is enabled in `boatlock/platformio.ini`; only one process can own `/dev/cu.usbmodem2101` at a time.
- Current boat hardware may be away from the ESP32 USB bench link. The normal firmware update path is phone-bridged BLE OTA through the production app wrapper:
  - `tools/hw/nh02/android-run-app-e2e.sh --ota --ota-firmware boatlock/.pio/build/esp32s3_service/firmware.bin --wait-secs 1800`
  - USB flashing through `tools/hw/nh02/flash.sh` is seed/recovery only unless the task explicitly targets the USB bench path.
  - Do not block ordinary firmware update/app rebuild work only because the ESP32 USB serial path is absent, provided the Android phone target is proven and BLE OTA can run.

## GPS, Heading, And Control Rules

- GPS source priority:
  1. hardware GPS when `gps.location.isValid()` and age is below `MaxGpsAgeMs`
  2. phone GPS from `SET_PHONE_GPS` while age is `<= 5000 ms`
  3. otherwise no fix
- Control GNSS is hardware-only. Phone GPS fallback may populate UI/telemetry but must not pass anchor quality gate, save BOOT anchor points, or expose old hardware HDOP/sentence metrics as current control quality.
- `MaxGpsAgeMs` is a setting, not a hard-coded constant. Default is `1500 ms`; allowed range is `300..20000`.
- Hardware GPS path applies moving-average filtering via `GpsFWin` (`1..20`), rejects the first jump above `MaxPosJumpM`, and accepts a repeated stable jump candidate as the new baseline to avoid permanent lockout after real relocation or cold-start drift.
- Heading comes from onboard BNO08x UART-RVC frames only. `headingAvailable()` is true only when BNO08x is initialized and fresh heading frames are arriving; stale/missing frames must fail closed as no heading.
- BNO08x reset GPIO logs such as `pulse=1` are not recovery proof unless fresh heading frames arrive afterward and the long acceptance capture remains clean.
- Commands such as `SET_HEADING`, `EMU_COMPASS`, and `CALIB_COMPASS` are removed/no-op territory in the current product surface.
- GPS-to-compass yaw correction is active only when:
  - compass is ready
  - speed is `>= 3.0 km/h`
  - movement since reference point is `>= 4.0 m`
- GPS heading correction behavior:
  - target correction is clamped to `[-90, +90]`
  - smoothing `alpha=0.18`
  - correction expires after `180000 ms` without updates
- `SET_ANCHOR` stores the current heading only if fresh heading is available, otherwise `0`.
- `ANCHOR_ON` requires all of: saved anchor point, onboard heading available, and GNSS quality gate pass.
- `HoldHeading=1` makes anchor bearing come from stored anchor heading.
- Otherwise anchor bearing comes from GNSS course to anchor, with a `120 s` cache if GNSS temporarily drops.
- `DriftFail` is treated as a containment breach and must exit anchor into a quiet state.
- `fallbackHeading` and `fallbackBearing` are UI placeholders only, not real sensor fallbacks.
- Runtime failsafes must stop motion and latch `HOLD`; they must not automatically enter `MANUAL` after sensor, link, loop, NaN, or command-range faults.
- Manual control is core product scope, but it must be standardized and deadman-protected:
  - phone and future BLE joystick/remotes feed the same manual-control state model
  - BLE control uses atomic `MANUAL_TARGET:<angleDeg>,<throttlePct>,<ttlMs>` and `MANUAL_OFF`
  - `MANUAL_TARGET` disables Anchor mode on entry so manual timeout or reconnect cannot unexpectedly resume Anchor
  - split legacy commands such as `MANUAL_DIR` and `MANUAL_SPEED` must not be reintroduced
  - phone manual UI may latch the selected vector/speed locally, but firmware actuation must stay deadman-protected by repeated `MANUAL_TARGET` refresh and TTL expiry
- Physical button inputs must be debounced. BOOT anchor save and STOP pairing window require stable long-press; a single GPIO bounce must not save an anchor, open pairing, or change runtime mode.

## Display, BLE, And Security

- Display convention:
  - boat nose is fixed at the top of the screen
  - compass card rotation uses `worldDeg + heading`
  - anchor arrow uses `anchorBearing - heading`
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
- Flutter BLE scan must not use an Android service-only scan filter because the
  product contract allows name-only `BoatLock` advertisements as well as
  service UUID matches.
- Flutter stops scanning before connect and retries scan/reconnect every `3 s`.
- Android and macOS app builds are release builds only. Do not create separate
  debug/service app variants; the release app always contains the service UI and
  hides service controls behind the Settings `Сервисный режим` switch.
- App heartbeat is sent every `1 s` once connected.
- Security flow:
  - hardware STOP long-press `3 s` opens pairing window
  - pairing window duration is `120 s`
  - pairing uses `PAIR_SET:<ownerSecretHex>` with a 32-hex owner secret
  - auth flow is `AUTH_HELLO` -> read `secNonce` -> `AUTH_PROVE`
  - `PAIR_CLEAR` is accepted only from owner session or while pairing window is physically open
  - when `secPaired=1`, control/write commands must be wrapped in `SEC_CMD`
- Do not keep protocol commands only for compatibility; remove obsolete commands from firmware, Flutter, tests, and docs together.

## Simulation And Validation

- On-device HIL scenarios `S0..S19` are part of the product surface and regression suite.
- `SIM_*` commands are handled in the normal BLE command path.
- Offline anchor simulation lives in `tools/sim/`.
- Native firmware tests live under `boatlock/test/`.
- EEPROM/write-throttling work must consider all `settings.save()` call sites, not only PID persistence.
- If you change BLE commands, telemetry keys, settings schema, or simulation behavior, update code, tests, and docs together.

## Main And Release Branch Policy

- Treat `main` as the releasable integration branch, not as a parking lot for unfinished runtime features.
- Stabilize shippable cuts on `release/vX.Y.x` branches and publish artifacts only through GitHub Releases from `vX.Y.Z` tags.
- GitHub Pages is not a BoatLock firmware-release channel.
- Before removing non-core runtime behavior, preserve it in a dedicated branch first.
- Default extraction order:
  - `feature/runtime-tuning-and-diagnostics`
  - `feature/on-device-hil-ble`
- If one parking branch is simpler for now, use `feature/extended-runtime` and cut features from `main` incrementally.
- Offline simulation, tests, and CI validation are not "extra functionality"; they stay in `main` as mandatory validation infrastructure.

## Hard Review Bar

- Safety and security beat convenience.
- In this repo, safety review means first of all operational safety:
  - no unintended motor or stepper actuation
  - no unexpected auto-enable after reboot, reconnect, stale sensor recovery, or button glitches
  - no control-loop behavior that can cause hunting, twitching, or repeated on/off thrash in normal noise conditions
- Reject changes that expand actuation surface without a clear product reason.
- No plaintext secret/code logging, no auth bypasses, no pairing-only-on-paper flows.
- Security must work end-to-end from the shipped app, not only through hidden helper methods or manual command injection.
- Any BLE command added to `main` must justify why it belongs in the release product surface.
- Stability changes must preserve failsafe behavior, reconnect behavior, and GNSS/heading invariants.
- When docs disagree with code, fix docs in the same change.

## Review Deliverables

- In full reviews, lead with P0/P1 findings around safety, security, bricking risk, silent control loss, or runtime instability.
- Separate "keep in `main`" from "move out of `main`" recommendations.
- Call out missing tests for every BLE/security/control behavior change.
- Do not treat "tests pass" as proof of safety when tests only encode the current behavior.

## Execution Flow

- Treat simplicity as the default. Prefer the smallest clear change that reduces code or branching without changing behavior unless the task explicitly requires a behavior change.
- Before refactoring any module, do a targeted external best-practice pass from official/primary sources for that module, then record the applied baseline in `WORKLOG.md` and, when durable, `skills/boatlock/references/external-patterns.md`.
- Do not start module code changes from intuition alone. If no useful external source applies, explicitly record that conclusion before editing.
- Default module cadence is batches of fifteen modules:
  - for each module: external baseline, smallest refactor, local tests, worklog/self-review, commit, and push
  - after every fifteenth module: update the target firmware through the normal phone BLE OTA path when available; use USB flash only for seed/recovery, then run hardware acceptance, serial log scan where reachable, and Android BLE smokes
  - run hardware earlier only for high-risk changes that touch hardware drivers, pinout, flashing/debug wrappers, actuator safety, BLE reconnect/install behavior, or any path where local tests cannot bound the risk
- Fix blockers at the source before normalizing a workaround.
- Do not continue through an alternate path, side probe, fallback data source, or partial workaround until the blocker is fixed or the user explicitly waives that fix.
- Do not start a second path just to go faster while the primary path is still running and making forward progress.
- Do not classify a running build, flash, acceptance run, or device-side check as hung unless a concrete lack of progress is proven.
- Do not run `pio test` and `pio run` in parallel against the same project/build directory unless the build dirs are isolated; PlatformIO shares `.pio/build`.
- Do not leave empty directories under `boatlock/test/` after removing a suite; PlatformIO treats an empty test suite as a full-run error.
- If a tracked wrapper, script, or documented workflow fails, fix that canonical path first unless the user explicitly waives the fix.
- If a tracked wrapper returns wrong, stale, or ambiguous results, fix the wrapper or its guidance first instead of normalizing that defect downstream.
- When a dedicated wrapper exists for BoatLock hardware or Android validation, use it instead of ad-hoc shell glue.
- When validating a tracked deploy/debug/acceptance path, the result must come from that path itself. Do not replace a pending wrapper verdict with a locally reconstructed answer or manual log reading.
- If a tracked validation run is still executing, either wait for terminal status or report that it is still in progress. Extra reads are blocker-debug context only.
- If a detour already created the wrong artifact shape, reconcile that artifact through the canonical path in the same turn instead of leaving both paths alive.
- If `tools/hw/nh02/flash.sh --no-build` fails because expected PlatformIO artifacts are missing, rerun canonical `tools/hw/nh02/flash.sh` instead of hand-copying or reconstructing artifacts.
- Do not treat a cleanup-only or style-only request as permission to change semantics. If a correctness fix requires a behavior change, surface that as a separate change.
- Preserve explicit workflow order when a skill or wrapper documents prerequisites; do not reorder steps just to make a later command pass.
- For live writes to `nh02`, Android devices, or other attached hardware, prove the exact target first and keep the rollback/recovery path explicit before mutating it.
- Prefer checked repo scripts and remote helpers over inline `ssh "..."` write commands or ad-hoc shell chains for hardware-side mutation.
- If a connectivity or host-access failure appears only inside the sandbox, treat it as a sandbox artifact first and rerun the real probe host-side before calling it a target-side issue.

## Durable Learning

- Keep `WORKLOG.md` as the per-task trail for decisions, blockers, validation, and self-review.
- If the user corrects a reusable workflow assumption, scope rule, or proof path, capture that correction immediately before continuing.
- When a task reveals durable workflow knowledge or a corrected assumption, promote it into `AGENTS.md`, repo skills, or references in the same turn.
- If no durable lesson emerged, say so and avoid documentation-only churn.

## Local Commands

- Firmware build:
  - `cd boatlock && pio run -e esp32s3`
- Native firmware tests:
  - `cd boatlock && platformio test -e native`
- Flutter tests:
  - `cd boatlock_ui && flutter test`
- Offline simulation:
  - `python3 tools/sim/test_sim_core.py`
  - `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json`
- NH02 hardware bench:
  - `tools/hw/nh02/install.sh`
  - `tools/hw/nh02/android-run-app-e2e.sh --ota --ota-firmware boatlock/.pio/build/esp32s3_service/firmware.bin --wait-secs 1800`
  - `tools/hw/nh02/flash.sh` (USB seed/recovery)
  - `tools/hw/nh02/acceptance.sh`
  - `tools/hw/nh02/monitor.sh`
  - `tools/hw/nh02/status.sh`
- Android USB + BLE smoke:
  - `tools/android/status.sh`
  - `tools/android/build-smoke-apk.sh`
  - `tools/android/run-smoke.sh`
  - `tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130`
- CI/version helpers:
  - `python3 tools/ci/check_firmware_version.py`
  - `python3 tools/ci/check_config_schema_version.py`
  - `pytest tools/ci/test_*.py`
