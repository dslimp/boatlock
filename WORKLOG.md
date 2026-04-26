# BoatLock Worklog

## Purpose

This file records the actual execution stages for BoatLock work so the repo keeps a durable trail of:

- what changed
- why it changed
- what was validated
- what still looks risky

## Logging Rules

1. Add a new stage entry for each meaningful work slice, not for every tiny command.
2. Record only durable facts: scope, decisions, validation, open risks, next step.
3. Include a short self-review block in each entry:
   - what is still weak
   - what should be simplified next
   - what should be promoted into the repo skill or references
4. Promote only stable, reusable lessons into `skills/boatlock/`; do not copy temporary ideas there.

## Stages

### 2026-04-26 Stage BLE OTA phone bridge

Scope:
- Add a phone-driven BLE firmware update path for ESP32-S3 so debug firmware can be loaded without keeping USB attached after the first seed flash.

External baseline:
- Espressif OTA API writes sequential chunks to an OTA app partition, validates/finalizes the image at the end, and only then selects it for boot.
- Espressif BLE OTA examples use explicit start/stop command packets, firmware chunks, ACK/error reporting, and integrity checks.

Key outcomes:
- Added BLE OTA characteristic `9abc` for binary chunks.
- Added `OTA_BEGIN:<size>,<sha256>`, `OTA_FINISH`, and `OTA_ABORT` on the existing command characteristic.
- `OTA_BEGIN` enters a stopped safe state: Anchor off, Manual stopped, motor/stepper stopped, failsafe HOLD latched.
- Chunks are ignored until a valid command arms OTA; active OTA rejects other runtime commands; disconnect aborts the update.
- Flutter can download a firmware URL, verify an operator-provided SHA-256, and upload over BLE with progress.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_ota_command -f test_ble_command_handler -f test_ble_security` -> PASS (`43/43`).
- `cd boatlock && pio run -e esp32s3` -> PASS, RAM `11.7%`, flash `21.2%`, firmware `710448` bytes, SHA-256 `7fd929225493d73010ea039c4d2d9429e6f6e9d3753a024ef9f33e4db369a527`.
- `cd boatlock_ui && flutter test test/ble_ota_payload_test.dart test/ble_discovery_check_test.dart test/ble_ids_test.dart test/settings_page_test.dart` -> PASS (`9/9`).
- `cd boatlock_ui && flutter analyze` -> PASS.
- `pytest -q tools/ci/test_*.py` -> PASS (`16/16`).
- `tools/hw/nh02/flash.sh --no-build` -> PASS, ESP32-S3 `98:88:e0:03:ba:5c` flashed and verified over USB.
- `tools/hw/nh02/acceptance.sh` -> PASS; boot log includes `[BLE] init ... ota=9abc`, BLE advertising, display, GPS UART, and fresh BNO08x heading events.
- `tools/hw/nh02/android-run-app-e2e.sh --gps --serial 68b657f0 --wait-secs 180` -> FAIL only on `app_gps_timeout`; app received `174` BLE telemetry events at RSSI `-48`, but bench GNSS reported `NO_GPS`, `lat=0`, `gnssQ=0`.
- `tools/hw/nh02/android-run-app-e2e.sh --reconnect --serial 68b657f0 --wait-secs 130` -> PASS, reason `app_telemetry_after_reconnect`.
- Normal debug APK rebuilt and reinstalled on phone `68b657f0` after e2e build; required Android permissions are granted.

Self-review:
- This is deliberately a debug/service path, not a production update system.
- Remaining weakness: SHA-256 protects integrity only if the expected hash comes from a trusted source; production should add signed images/secure boot before exposing OTA broadly.
- Full live BLE OTA of a second firmware image from the phone was not run yet; current hardware proof covers the seed firmware, GATT surface/boot log, app BLE reconnect, and local protocol tests.
- Promote to skill/reference: BLE OTA belongs behind auth and should fail closed without changing the boot partition on partial transfer.

### 2026-04-23 Stage 1: Full review baseline

Scope:
- Full repo review with focus on operational safety, stability, security, and main-branch scope.

Key outcomes:
- Identified high-risk runtime issues around accidental actuation, phone-GPS control fallback, incomplete quiet-state behavior, and too-broad main UI surface.
- Identified separate security issues in pairing/auth flow and app secure-command reachability.
- Wrote/updated `AGENTS.md` to define main-branch scope and stricter review bar.

Validation:
- `platformio test -e native`
- `flutter test`
- `pytest tools/ci/test_*.py`

Self-review:
- Review was correct but too much weight initially went into security before the user clarified that runtime safety mattered more.
- Future reviews in this repo should rank "unexpected movement" above cryptographic rigor unless the user explicitly asks otherwise.

Promote to skill:
- Safety in BoatLock means "no unintended motion" first, then transport/auth hardening.

### 2026-04-23 Stage 2: Runtime safety hardening

Scope:
- Reduce accidental actuation and noisy control behavior in the shipped path.

Key outcomes:
- Removed single-tap BOOT auto-arm behavior.
- Enforced disarm-on-boot.
- Removed phone GPS from anchor control path.
- Made drift-fail and control-loss paths go quiet.
- Removed manual-control surface from the main app path.

Validation:
- Native firmware tests
- ESP32-S3 production build
- Flutter tests and analyze

Self-review:
- This narrowed runtime risk fast without adding much code.
- Remaining weakness after this stage was still monolithic orchestration inside `main.cpp`.

Promote to skill:
- Main must not expose manual actuation as a default operator flow.
- Phone GPS may exist for tooling or UI, but not for the main anchor control loop.

### 2026-04-23 Stage 3: Core modular split

Scope:
- Break core runtime behavior out of `main.cpp` using KISS and smaller interfaces.

Key outcomes:
- Added `boatlock/RuntimeGnss.h` for GNSS state, quality, correction, and bearing cache.
- Added `boatlock/RuntimeMotion.h` for drift, actuation gating, and failsafe application.
- Reduced `main.cpp` toward setup/loop orchestration instead of mixed runtime logic.
- Added native tests for the new runtime modules.

Validation:
- `platformio test -e native`
- `pio run -e esp32s3`

Self-review:
- This was the right cut line: GNSS and motion became independently testable.
- `main.cpp` still retained mode arbitration and some policy decisions, so the split was incomplete.

Promote to skill:
- Prefer module boundaries around stateful runtime domains, not around transport or helper classes.

### 2026-04-23 Stage 4: Narrow control contract

Scope:
- Take the useful idea from Vanchor: one explicit mode arbiter and one compact control input contract.

Key outcomes:
- Added `boatlock/RuntimeControl.h` with `CoreMode` and `RuntimeControlInput`.
- Refactored `main.cpp` to compose one control input and pass it into `RuntimeMotion`.
- Added tests for mode precedence and updated motion tests to the new API.

Validation:
- `platformio test -e native`
- `pio run -e esp32s3`
- `flutter test --no-pub`
- `flutter analyze`
- `pytest tools/ci/test_*.py`

Self-review:
- Good simplification: fewer boolean combinations leaked across modules.
- Next simplification target should be BLE publish/buttons/sim glue, not another abstraction layer.

Promote to skill:
- Prefer one aggregated control contract over many scattered runtime flags.
- Keep mode precedence explicit and test it directly.

### 2026-04-23 Stage 5: ArduPilot-inspired guardrails

Scope:
- Take only the useful concepts from ArduPilot Rover/Boat docs: pre-enable safety gate and containment exit.

Key outcomes:
- `ANCHOR_ON` now requires saved anchor point, onboard heading, and GNSS quality gate.
- Added `NO_HEADING` anchor deny reason.
- Reinterpreted `DriftFail` as `CONTAINMENT_BREACH` that exits anchor and goes quiet.
- Aligned BLE protocol docs and native tests with the new reasons.

Validation:
- `platformio test -e native`
- `pio run -e esp32s3`
- `pytest tools/ci/test_*.py`

Self-review:
- This pulled in the right safety ideas without importing ArduPilot's mode complexity.
- Still missing: a first-class operator-visible `SAFE_HOLD`/`HOLD` mode instead of inferring quiet-state from reason fields.

Promote to skill:
- Use pre-enable gates before enabling anchor.
- Treat containment breach as an exit condition, not as a weaker "just reduce thrust" signal.

### 2026-04-23 Stage 6: External best-practice research

Scope:
- Study OpenCPN anchor watch, Signal K anchor alarm, ArduPilot boat/loiter/failsafe docs, pypilot, and commercial GPS-anchor systems to avoid inventing solved behavior.

Key outcomes:
- Confirmed that OpenCPN is useful mainly for anchor-watch and auto-mark heuristics, not for motorized hold control.
- Confirmed that Signal K uses a stronger architecture for alarms: on-device/server monitoring, track history, multi-channel notifications, and phased anchor workflow.
- Confirmed that ArduPilot's most valuable ideas for BoatLock are pre-enable checks, a true quiet `Hold` state, radius-based correction, and explicit failsafe actions.
- Confirmed that commercial GPS-anchor systems converge on jog/reposition, heading-hold adjacency, distance/bearing-to-spot telemetry, mandatory calibration, and honest GNSS limitations.
- Wrote durable synthesis into `skills/boatlock/references/external-patterns.md`.

Validation:
- Official documentation review with source capture and cross-checking

Self-review:
- The useful patterns are architectural and UX-related; copying full product feature sets would be a mistake.
- The strongest missing piece in BoatLock after this research is still a first-class `SAFE_HOLD` mode and a clearer separation between anchor watch vs motorized anchor hold.

Promote to skill:
- Keep external patterns in a dedicated reference file so research can inform design without polluting canonical runtime docs.

### 2026-04-23 Stage 7: First-class HOLD mode

Scope:
- Turn the inferred quiet state into a real runtime mode with explicit operator-facing semantics.

Key outcomes:
- Added `SAFE_HOLD` to `boatlock/RuntimeControl.h` with shipped label `HOLD`.
- Latched `HOLD` inside `boatlock/RuntimeMotion.h` for emergency stop and stop-style failsafes.
- Cleared `HOLD` only on explicit operator actions: `ANCHOR_ON`, `ANCHOR_OFF`, and manual-mode entry.
- Updated display mode coloring, protocol docs, firmware reference, and native BLE/runtime tests.

Validation:
- native firmware tests
- ESP32-S3 production build

Self-review:
- This is a good cut because it reuses the existing runtime split instead of adding another state machine layer.
- Remaining weakness: `status` still mixes mode and reason strings into one flat BLE field; if that grows further, it should be separated rather than extended.

Promote to skill:
- Quiet-safe fallback should be a first-class runtime mode, not a hidden combination of booleans and reason latches.

### 2026-04-23 Stage 8: Status contract split

Scope:
- Simplify the BLE/UI status contract by separating health summary from detailed reasons.

Key outcomes:
- Added `boatlock/RuntimeStatus.h` as a small, testable status builder instead of keeping status string assembly inside `main.cpp`.
- Changed firmware publish semantics to:
  - `status`: `OK|WARN|ALERT`
  - `mode`: runtime mode only
  - `statusReasons`: comma-separated detail flags
- Updated Flutter parsing and the status panel to show summary and reasons separately.
- Removed the old overloaded `CONNECTED:MODE:ERR,...` shape from the shipping telemetry contract.

Validation:
- native firmware tests
- ESP32-S3 production build
- Flutter tests and analyze
- CI helper tests

Self-review:
- This is simpler than the previous flat `status` string and removes duplicated mode info.
- Remaining weakness: link/transport state is now implicit in connection presence; if it ever needs to be user-facing again, it should return as a separate key, not be merged back into `status`.

Promote to skill:
- Keep `status` as a short health summary and move detail lists into a separate telemetry field.

### 2026-04-23 Stage 9: Button hold controller split

Scope:
- Pull BOOT/STOP long-press behavior out of `main.cpp` into a reusable, testable state machine.

Key outcomes:
- Added `boatlock/HoldButtonController.h` as a generic pressed/hold/release edge tracker.
- Replaced ad-hoc BOOT/STOP button bookkeeping in `main.cpp` with the shared controller.
- Added unit coverage for long-press timing, single-fire behavior, and the zero-timestamp edge case that the old sentinel-style code handled poorly.

Validation:
- native firmware tests
- ESP32-S3 production build

Self-review:
- This is a better fit for the repo than more mock-heavy tests around `digitalRead()` because the real logic now lives in a pure state machine.
- Remaining weakness: BOOT and STOP actions are still wired inline in `main.cpp`; if button surface grows again, the next cut should move action dispatch out as well.

Promote to skill:
- Long-press semantics should live in a dedicated state machine, not in hand-rolled timestamp flags inside `main.cpp`.

### 2026-04-23 Stage 10: Runtime button actions split

Scope:
- Move BOOT/STOP action decisions out of `main.cpp` so button behavior is testable above raw hold timing.

Key outcomes:
- Added `boatlock/RuntimeButtons.h` to translate BOOT/STOP input state into explicit actions like save-anchor, deny-save, emergency-stop, and pairing-window-open.
- Switched `main.cpp` handlers to consume `RuntimeButtonAction` instead of branching directly on controller internals.
- Added native tests for the button action layer, including reset behavior between press cycles.

Validation:
- native firmware tests
- ESP32-S3 production build

Self-review:
- This is a better abstraction boundary than testing full button behavior through `main.cpp`.
- Remaining weakness: BLE param registration/publish is still a large block in `main.cpp` and is the next obvious extraction target.

Promote to skill:
- Separate raw hold timing from higher-level button actions; test both layers independently.

### 2026-04-23 Stage 11: Telemetry contract split

Scope:
- Pull BLE param registration and UI snapshot shaping out of `main.cpp` so telemetry becomes an explicit runtime layer instead of ad-hoc glue.

Key outcomes:
- Added `boatlock/RuntimeBleParams.h` to own the BLE param registry and status notify path.
- Added `boatlock/RuntimeUiSnapshot.h` as a small, pure display-facing snapshot builder with HIL sim overrides.
- Reduced `main.cpp` to orchestration for telemetry: it now wires dependencies into the registry and passes one structured UI snapshot into `display_draw_ui()`.
- Added native tests for sim-to-UI snapshot mapping and fallback-heading behavior.

Validation:
- native firmware tests
- ESP32-S3 production build

Self-review:
- This is better than just moving code blocks because both BLE telemetry and display state now have named contracts.
- Remaining weakness: `publishBleAndUi()` still owns periodic scheduling and mixes UI refresh cadence with BLE cadence; if telemetry keeps growing, that timing policy should be its own thin layer next.

Promote to skill:
- When `main.cpp` grows telemetry glue, extract named runtime contracts first, not just helper functions with the same hidden coupling.

### 2026-04-24 Stage 12: Telemetry cadence split

Scope:
- Remove loose UI/BLE publish timestamps from `main.cpp` and give telemetry timing its own tiny runtime contract.

Key outcomes:
- Added `boatlock/RuntimeTelemetryCadence.h` to own UI refresh cadence and BLE notify cadence.
- Removed `lastUiRefreshMs` and `lastBleNotifyMs` globals from `main.cpp`.
- Added focused native tests for cadence thresholds and UI/BLE independence.

Validation:
- native test `test_runtime_telemetry_cadence`

Self-review:
- This is a valid cut because it removes hidden timing state from `main.cpp` without widening the abstraction.
- Remaining weakness: GPS UART monitoring still keeps multiple timestamp and warning flags inline in `main.cpp`.

Promote to skill:
- When periodic publish logic has separate cadences, encode them as a tiny policy object instead of raw timestamp globals.

### 2026-04-24 Stage 13: GPS UART monitor split

Scope:
- Remove inline GPS UART warning/restart flags from `main.cpp` and move that serial-health policy into a dedicated runtime state machine.

Key outcomes:
- Added `boatlock/RuntimeGpsUart.h` for first-data detection, no-data warning, stale-stream warning, and restart cooldown policy.
- Removed raw GPS UART state flags from `main.cpp` and replaced them with one `RuntimeGpsUart` object.
- Added focused native tests for first-data detection, no-data warning, stale restart, and cooldown behavior.

Validation:
- native test `test_runtime_gps_uart`

Self-review:
- This is a good split because the UART health behavior is now testable without mocking `HardwareSerial`.
- Remaining weakness: sim-result banner state is still stored as loose fields in `main.cpp`.

Promote to skill:
- When serial-health policy needs timers, warnings, and cooldowns, move it into a dedicated runtime state machine before touching IO code.

### 2026-04-24 Stage 14: Simulation badge split

Scope:
- Remove sim-result banner bookkeeping from `main.cpp` and make its lifecycle a tiny runtime contract.

Key outcomes:
- Added `boatlock/RuntimeSimBadge.h` for run-latch, verdict mapping, scenario-id trimming, and expiry handling.
- Removed raw sim badge fields from `main.cpp`.
- Added focused native tests for running/no-badge, verdict mapping, scenario trimming, and expiry behavior.

Validation:
- native test `test_runtime_sim_badge`

Self-review:
- This is the right kind of extraction because it removes another loose state bundle instead of creating a bigger coordinator object.
- Remaining weakness: supervisor config assembly is still inline in `loop()` and is the next obvious policy block if I keep flattening `main.cpp`.

Promote to skill:
- When a UI/banner state depends on a lifecycle transition plus expiry timeout, model it as a tiny state object instead of booleans plus char buffers in `main.cpp`.

### 2026-04-24 Stage 15: Supervisor policy split

Scope:
- Pull anchor-supervisor config/input assembly out of `loop()` so safety policy stops living as inline `settings.get(...)` glue.

Key outcomes:
- Added `boatlock/RuntimeSupervisorPolicy.h` for clamped supervisor config and runtime input builders.
- Simplified `loop()` by replacing manual supervisor field assignment with named builders.
- Added focused native tests for config clamping and action mapping.

Validation:
- native test `test_runtime_supervisor_policy`

Self-review:
- This cut is worthwhile because it extracts real safety policy, not just cosmetic repetition.
- Remaining weakness: `handleSimCommand()` is still a broad command block with parsing and execution mixed together.

Promote to skill:
- When safety config comes from multiple settings with clamps and enum mapping, extract a policy builder instead of repeating `settings.get(...)` in `loop()`.

### 2026-04-24 Stage 16: Simulation command parsing split

Scope:
- Pull `SIM_*` command parsing out of `main.cpp` so HIL command classification stops being mixed with execution side effects.

Key outcomes:
- Added `boatlock/RuntimeSimCommand.h` to classify `SIM_LIST`, `SIM_STATUS`, `SIM_RUN`, `SIM_ABORT`, `SIM_REPORT`, and unknown `SIM_*` commands.
- Removed inline SIM run payload parsing from `main.cpp`.
- Added focused native tests for CSV payloads, object-style payloads, empty payload rejection, and unknown command classification.

Validation:
- native test `test_runtime_sim_command`

Self-review:
- This is a good cut because the command surface is now explicit and testable before it touches `hilSim`.
- Remaining weakness: `handleSimCommand()` still owns execution and report chunk logging, even though parsing is now cleanly separated.

Promote to skill:
- When a command family has multiple verbs and payload shapes, separate parsing/classification from execution before growing the handler further.

### 2026-04-24 Stage 17: Anchor nudge math split

Scope:
- Pull anchor nudge bearing mapping and geodesic projection out of `main.cpp`.

Key outcomes:
- Added `boatlock/RuntimeAnchorNudge.h` for nudge range validation, cardinal direction mapping, and projected anchor-point calculation.
- Simplified `nudgeAnchorBearing()` and `nudgeAnchorCardinal()` in `main.cpp` so they now mostly own safety checks and side effects.
- Added focused native tests for cardinal mapping, range bounds, and projected motion.

Validation:
- native test `test_runtime_anchor_nudge`

Self-review:
- This split is worth keeping because it removes real math and string-mapping logic from `main.cpp`, not just boilerplate.
- Remaining weakness: `preprocessSecureCommand()` is still a broad stateful command block, but that is a separate security-focused cut and not the next cheapest core simplification.

Promote to skill:
- Keep navigation math and direction mapping in dedicated helpers so runtime control paths stay readable and testable.

### 2026-04-24 Stage 18: Control input builder split

Scope:
- Pull mode-to-motion input assembly out of `loop()` so the core control contract stops being rebuilt inline around `RuntimeMotion`.

Key outcomes:
- Added `boatlock/RuntimeControlInputBuilder.h` for `RuntimeControlState` and `buildRuntimeControlState(...)`.
- Removed inline `RuntimeControlInput` field assembly from `main.cpp` and replaced it with one named builder call.
- Added focused native tests for manual-mode passthrough, anchor diff calculation, and missing-heading behavior.

Validation:
- native test `test_runtime_control_input_builder`

Self-review:
- This cut is worth keeping because it extracts real control-path policy and derived fields, not just argument shuffling.
- Remaining weakness: compass retry cadence still left a duplicate timestamp in `main.cpp` after the first extraction and needed one more cleanup pass.

Promote to skill:
- When a runtime loop derives both UI-facing state and actuator input from the same mode/heading/bearing facts, build them once through a named control-state helper.

### 2026-04-24 Stage 19: Compass retry cadence split

Scope:
- Move compass retry timing into a tiny runtime state object and delete the leftover retry timestamp from `main.cpp`.

Key outcomes:
- Added `boatlock/RuntimeCompassRetry.h` to own retry interval gating while the compass is unavailable.
- Removed the now-redundant `lastCompassRetryMs` global from `main.cpp`.
- Added focused native tests for retry interval behavior and ready-state suppression.

Validation:
- native test `test_runtime_compass_retry`

Self-review:
- This is the right-sized cleanup because the retry policy is now explicit and testable instead of hiding behind `millis()` arithmetic.
- Remaining weakness: `handleSimCommand()` still mixes SIM execution with report chunking and logging policy.

Promote to skill:
- If a retry loop only needs cadence gating, keep the timer inside a tiny state object and delete any mirrored timestamps left in the caller.

### 2026-04-24 Stage 20: SIM execution outcome split

Scope:
- Pull `SIM_*` execution outcome, run side-effect flags, and report chunking out of `main.cpp`.

Key outcomes:
- Added `boatlock/RuntimeSimExecution.h` for SIM list/status/run/abort/report execution outcomes and report chunking.
- Simplified `handleSimCommand()` in `main.cpp` so it now applies side effects and logs from one execution outcome instead of mixing execution policy with command parsing and chunk splitting.
- Added focused native tests for list/status payloads, run start/failure behavior, abort handling, report chunking, and unknown command passthrough.

Validation:
- native test `test_runtime_sim_execution`

Self-review:
- This cut is worth keeping because `main.cpp` stopped knowing how to split report payloads and when a SIM run should reset motion-related latches.
- Remaining weakness: `handleSimCommand()` still owns final log formatting; the control policy is now out, but the log surface is still local glue.

Promote to skill:
- When a command family has runtime side effects plus user-visible log variants, return one execution outcome object and keep the caller responsible only for applying side effects and logging.

### 2026-04-24 Stage 21: SIM log mapping split

Scope:
- Remove final `SIM_*` log-string mapping from `main.cpp` so the handler stops owning execution policy and text formatting at the same time.

Key outcomes:
- Added `boatlock/RuntimeSimLog.h` for deterministic mapping from `RuntimeSimExecutionOutcome` to emitted log lines.
- Simplified `handleSimCommand()` in `main.cpp` so it now applies side effects and emits returned lines instead of branching over every log variant.
- Added focused native tests for list/status/run/report/unknown log rendering.

Validation:
- native test `test_runtime_sim_log`

Self-review:
- This is a valid cut because the log surface is now explicit and testable without touching `hilSim` or the command handler.
- Remaining weakness: compass recovery in `loop()` is still a live IO block with retry, reload, and logging side effects combined.

Promote to skill:
- If a command family already returns structured execution outcomes, move final user/log text mapping into a dedicated helper instead of re-branching in the caller.

### 2026-04-24 Stage 22: Anchor enable gate split

Scope:
- Pull the safety gate that decides whether anchor enable is allowed out of `main.cpp`.

Key outcomes:
- Added `boatlock/RuntimeAnchorGate.h` for ordered anchor enable denial resolution.
- Replaced inline anchor enable gate branching in `main.cpp` with one named helper call.
- Added focused native tests for missing anchor point, missing heading, GNSS denial passthrough, and allow case.

Validation:
- native test `test_runtime_anchor_gate`

Self-review:
- This cut is small but worth keeping because it isolates a safety-critical decision that should stay stable and easy to audit.
- Remaining weakness: the loop still owns compass recovery IO/logging, which is the next real policy block if I keep flattening `main.cpp`.

Promote to skill:
- For safety-critical enable paths, keep denial precedence in one tiny helper so reviews can verify ordering without scanning unrelated runtime code.

### 2026-04-24 Stage 23: Compass recovery split

Scope:
- Pull compass retry execution, double-init attempt, and retry log formatting out of `loop()`.

Key outcomes:
- Added `boatlock/RuntimeCompassRecovery.h` for retry attempts, reload-vs-inventory outcome flags, and stable recovery log lines.
- Simplified `main.cpp` so loop now asks for one recovery outcome instead of manually performing the retry flow.
- Added focused native tests for skip, first-try success, second-try success, double-failure inventory path, and log line stability.

Validation:
- native test `test_runtime_compass_recovery`

Self-review:
- This cut is worth keeping because the loop no longer mixes retry cadence with bus restart sequencing and logging details.
- Remaining weakness: `loop()` still does a fair amount of orchestration around supervisor input assembly and control application in one block, though the individual policies are already extracted.

Promote to skill:
- When a hardware recovery path has retry cadence, repeated init attempts, and different follow-up actions on success vs failure, model it as one recovery outcome instead of open-coding the sequence in `loop()`.

### 2026-04-24 Stage 24: NH02 hardware deploy and debug path

Scope:
- Make `nh02` the tracked hardware bench target for ESP32-S3 deploy and serial debugging without touching unrelated host infrastructure.

Key outcomes:
- Added `tools/hw/nh02/` with repo-tracked wrappers for remote install, flash, status, and RFC2217 monitor flows.
- Added the tracked remote helper `tools/hw/nh02/remote/boatlock-flash-esp32s3.sh` so flashing happens on `nh02` with its local USB device while the debug bridge is temporarily stopped and then restored.
- Added the tracked service unit `tools/hw/nh02/boatlock-esp32s3-rfc2217.service` and switched it to `esp_rfc2217_server` instead of the deprecated `.py` launcher.
- Fixed `tools/hw/nh02/install.sh` so refreshing the tracked unit actually restarts the active systemd service instead of leaving the old process running.
- Documented the host boundary and operator workflow in `docs/HARDWARE_NH02.md` and the validation reference.

Validation:
- `./tools/hw/nh02/install.sh`
- `./tools/hw/nh02/status.sh`
- `./tools/hw/nh02/flash.sh`
- `./tools/hw/nh02/monitor.sh`

Self-review:
- This cut is worth keeping because deploy and debug now use one stable, repo-tracked path instead of ad-hoc host commands.
- The real bug found during validation was operational, not cosmetic: updating a systemd unit without a restart left `nh02` on stale runtime even though the installer reported success.
- Follow-up cleanup in the same stage switched the flash helper to current esptool reset flag spellings and revalidated another full flash-cycle.
- Remaining weakness: the current debug path is stable for serial logs, but source-level JTAG/OpenOCD is still not wired into the repo workflow.

Promote to skill:
- For hardware bench automation, keep all host-owned state under one dedicated root and one dedicated service, then validate `install -> status -> flash -> monitor` end to end before calling the path stable.

### 2026-04-24 Stage 25: Hardware acceptance skill and serial boot verdict

Scope:
- Add a repeatable post-flash hardware acceptance path for `nh02`, backed by a repo-local skill instead of ad-hoc monitor reading.

Key outcomes:
- Added `tools/hw/nh02/acceptance.py` and `tools/hw/nh02/acceptance.sh` for boot-log capture, required-marker checks, fatal-pattern scanning, and JSON/log artifact output.
- Added `tools/hw/nh02/remote/boatlock-reset-esp32s3.sh` so acceptance can force a clean reset before reading boot logs.
- Updated `tools/hw/nh02/install.sh` to sync the reset helper, and tightened `acceptance.sh` to fail with a clear message if `install.sh` was not run first.
- Added parser coverage in `tools/hw/nh02/test_acceptance.py`.
- Added the repo-local skill `skills/boatlock-hardware-acceptance/SKILL.md` plus `references/android-ble.md` to describe the current Android USB/BLE capability and the missing automation surface.

Validation:
- `bash -n tools/hw/nh02/install.sh tools/hw/nh02/flash.sh tools/hw/nh02/monitor.sh tools/hw/nh02/status.sh tools/hw/nh02/acceptance.sh tools/hw/nh02/common.sh tools/hw/nh02/remote/boatlock-flash-esp32s3.sh tools/hw/nh02/remote/boatlock-reset-esp32s3.sh`
- `pytest -q tools/hw/nh02/test_acceptance.py`
- `./tools/hw/nh02/install.sh`
- `./tools/hw/nh02/acceptance.sh --seconds 15 --log-out /tmp/boatlock-boot.log --json-out /tmp/boatlock-acceptance.json`
- `./tools/hw/nh02/status.sh`

Self-review:
- This cut is worth keeping because the acceptance verdict is now deterministic and scriptable instead of depending on a human reading serial output.
- The real weakness found during validation was orchestration, not parser logic: acceptance must run after helper sync, so I tightened the wrapper to fail fast with a clear install hint.
- Current acceptance is intentionally serial-only; it proves boot health and device readiness, but not BLE control flow end to end.
- Android path is feasible on this machine, but full automated phone-to-bench BLE smoke still needs a dedicated app-side integration harness.

Promote to skill:
- For real hardware acceptance, separate the verdict logic from raw log capture and persist both the structured result and the raw boot log.

### 2026-04-24 Stage 26: Agent-tooling execution rules imported

Scope:
- Pull the useful execution-discipline rules from `devops/agent-tooling` into BoatLock's canonical notes.

Key outcomes:
- Added blocker-first and no-second-path-while-primary-progresses rules to `AGENTS.md`.
- Added the same execution discipline to `skills/boatlock/SKILL.md` and the hardware acceptance skill so long-running build, flash, acceptance, and phone-smoke flows are not interrupted just because they are slow.
- Kept the imported guidance limited to the parts that match BoatLock work: canonical-path repair, long-running process handling, and durable learning capture.

Validation:
- doc/skill update only

Self-review:
- This cut is worth keeping because the rules match problems already hit in the current turn: wrapper drift, accidental fallback temptation, and long-running Android build steps.
- Remaining weakness: the Android smoke build itself is still waiting on Gradle wrapper/dependency bootstrap, so the new rule is in place before that path is fully proven.

Promote to skill:
- When a shared workflow rule repeatedly changes live behavior in the same task, import the minimal durable version into the target repo instead of relying on memory.

### 2026-04-24 Stage 27: Agent-tooling flow rules strengthened

Scope:
- Pull a second pass of useful `devops/agent-tooling` rules into BoatLock, focused on coding simplicity, canonical-path repair, and validation-result discipline.

Key outcomes:
- Added a repo-level simplicity default: prefer the smallest clear change that reduces code or branching without silently widening behavior.
- Strengthened execution-flow rules to forbid continuing through alternate paths or partial workarounds while the real blocker is still unfixed.
- Added the canonical-path reconciliation rule: if a detour already created the wrong artifact shape, correct that artifact through the canonical path in the same turn.
- Added explicit guidance that cleanup-only requests do not grant semantic changes.
- Added hardware-acceptance guidance that manual serial/logcat inspection is blocker-debug only and must not be reported as the acceptance or phone-smoke verdict.

Validation:
- `sed`/`rg` inspection of imported source rules from `devops/agent-tooling`
- doc/skill update only

Self-review:
- This pass is worth keeping because it imports the parts that actually affect current BoatLock behavior instead of cloning general `agent-tooling` policy wholesale.
- Remaining weakness: Android smoke is still waiting on the first canonical APK build, so the new "do not replace pending verdicts with manual logs" rule is in place before that path is fully proven.

Promote to skill:
- When a repo already has a canonical wrapper or validation path, manual logs are blocker-debug context only; they do not replace the pending wrapper verdict.

### 2026-04-24 Stage 28: Canonical Android smoke build proven

Scope:
- Let the first canonical Android smoke build finish instead of starting a second path, then verify the next blocker honestly.

Key outcomes:
- Waited for the original `tools/android/build-smoke-apk.sh` run to reach terminal status instead of re-running the build through a manual fallback.
- Confirmed the dedicated smoke APK build completes successfully and emits `build/app/outputs/flutter-apk/app-debug.apk`.
- Checked the canonical ADB visibility path afterward; current blocker is now only that no Android device is attached yet.

Validation:
- live `build-smoke-apk.sh` session reached terminal success
- `./tools/android/status.sh`

Self-review:
- This stage is worth logging because it proves the repo-tracked Android build path itself is good; the remaining gap is hardware availability, not wrapper drift.
- No new durable repo rule emerged beyond the execution-discipline import already captured in Stage 27.

Promote to skill:
- none

### 2026-04-24 Stage 29: Agent-tooling rules completed for BoatLock

Scope:
- Finish importing the `agent-tooling` rules that are actually relevant to BoatLock code work, bench mutation, and tracked validation paths.

Key outcomes:
- Added repo-level rules for wrapper-output trust, tracked-validation truth, in-progress verdict handling, documented prerequisite order, and sandbox-artifact classification.
- Added repo-level hardware-write rules: prove the exact target first, keep recovery explicit, and prefer checked scripts/helpers over inline `ssh` mutation chains.
- Mirrored the same rules into the repo skill and the hardware-acceptance skill so the guidance applies both to normal coding work and to `nh02`/Android validation.

Validation:
- `sed` review of `devops/agent-tooling/AGENTS.md`
- repo doc/skill update only

Self-review:
- This pass is worth keeping because it closes the remaining gap between "no detours" guidance and actual bench-operation discipline.
- I still did not import broad infra-only rules like production-host policy or Ansible runtime policy, because they are not the right abstraction for this repo.

Promote to skill:
- Import the narrowest durable subset of shared workflow rules that actually matches the target repo; do not cargo-cult the full source policy.

### 2026-04-24 Stage 30: NH02 remote Android USB path added

Scope:
- Add a tracked Android USB workflow for phones attached to `nh02`, then verify the currently connected phone through that path.

Key outcomes:
- Added tracked `nh02` Android helpers: `tools/hw/nh02/android-install.sh`, `tools/hw/nh02/android-status.sh`, and remote helper `tools/hw/nh02/remote/boatlock-ensure-android-tools.sh`.
- Extended `tools/hw/nh02/install.sh` so the Android helper is synced into `/opt/boatlock-hw/bin` together with the existing flash/reset helpers.
- Updated `docs/HARDWARE_NH02.md`, the hardware-acceptance skill, and Android reference notes so remote Android checks on `nh02` are part of the canonical workflow.
- Verified that `nh02` sees the connected phone physically as `2717:ff40 Xiaomi Inc. Mi/Redmi series (MTP)`.
- Installed `adb` on `nh02` through the tracked helper and verified `adb version` on the host.
- Verified that `adb devices -l` on `nh02` is still empty, so the phone is connected physically but USB debugging is not enabled yet.

Validation:
- `bash -n tools/hw/nh02/install.sh tools/hw/nh02/android-install.sh tools/hw/nh02/android-status.sh tools/hw/nh02/common.sh tools/hw/nh02/remote/boatlock-ensure-android-tools.sh`
- `./tools/hw/nh02/install.sh`
- `./tools/hw/nh02/android-install.sh`
- `./tools/hw/nh02/android-status.sh`

Self-review:
- This is worth keeping because it closes a real workflow gap: before this stage, the repo had Android smoke logic but no canonical way to work with a phone physically attached to `nh02`.
- Current blocker is now correctly isolated to the phone state itself rather than the bench host: the USB path is alive, `adb` exists, but the phone is still exposing only MTP and not a debug transport.

Promote to skill:
- When the phone is attached to `nh02`, check physical USB enumeration and `adb devices -l` on `nh02` itself before assuming anything about the app, BLE, or cable path.

### 2026-04-24 Stage 31: Android install semantics corrected and captured

Scope:
- Record the proven Android install/update behavior on the Xiaomi phone and fix the canonical notes so the install verdict does not stay only in chat history.

Key outcomes:
- Confirmed that the first `adb install` attempt was blocked by MIUI policy with `INSTALL_FAILED_USER_RESTRICTED`, which required one manual install on the phone.
- Confirmed separately that after that first manual install, the tracked `adb install -r` path through `tools/hw/nh02/android-run-smoke.sh` succeeds, so USB APK updates are valid on this phone.
- Added `--no-install` to the documented Android smoke workflow as a temporary debug cut for already-installed builds.
- Captured the more important correction: the current phone-smoke blocker is no longer APK install but BLE telemetry after connect.

Validation:
- `./tools/hw/nh02/android-run-smoke.sh --no-build --wait-secs 90`
- terminal output included `Performing Streamed Install` and `Success`
- later terminal result still failed on `BOATLOCK_SMOKE_RESULT ... timeout_waiting_for_telemetry`

Self-review:
- I missed this in the canonical notes on the first pass even though the terminal wrapper had already separated install success from the later BLE failure.
- That was a workflow error, not a target issue: I corrected the conclusion in chat, but the durable repo notes lagged behind and needed an explicit repair step.

Promote to skill:
- For Android USB smoke, record first-install policy, later `adb install -r` update, app launch, and BLE runtime as separate checkpoints; do not collapse them into one "install failed" or "Android smoke failed" label.

### 2026-04-24 Stage 32: BLE live path replaced with binary v2 frames

Scope:
- Replace the old live BLE data path with a clean GATT-style control point + live observation model.
- Do not keep backward compatibility shims in code; the app is not released and the old JSON/`all` live path was already failing on hardware.

Key outcomes:
- Removed the JSON notify/parameter-read live path and deleted the obsolete `ParamHelpers.h` helper.
- Added `RuntimeBleLiveFrame.h`: one 70-byte little-endian v2 live frame with explicit mode/status codes, flags, scaled numeric fields, security nonce, reject code, and status reason bitmask.
- Replaced firmware string telemetry registration with a typed `RuntimeBleLiveTelemetry` provider in `RuntimeBleParams.h`.
- Updated the Flutter BLE client to send `STREAM_START` + `SNAPSHOT`, decode binary frames through `ble_live_frame.dart`, and use `SNAPSHOT` for auth/pairing refreshes.
- Increased the Android smoke page timeout from `30 s` to `75 s` after real logcat showed the Xiaomi phone could connect and receive telemetry after the old page timeout had already failed.
- Updated BLE protocol docs and BLE/UI skill notes so the canonical contract is binary live frames, not a JSON tunnel.
- Updated Android smoke wrappers so a phone-side `adb install` restriction stages the APK to `Downloads/boatlock-smoke.apk` and reports a clear install-stage blocker.

Validation:
- `cd boatlock && platformio test -e native` -> `169/169`
- `cd boatlock && pio run -e esp32s3` -> success
- `cd boatlock_ui && flutter test --no-pub` -> all tests passed
- `cd boatlock_ui && flutter analyze` -> clean
- `bash -n tools/android/run-smoke.sh tools/hw/nh02/android-run-smoke.sh tools/hw/nh02/remote/boatlock-run-android-smoke.sh tools/hw/nh02/install.sh tools/hw/nh02/common.sh`
- `pytest -q tools/hw/nh02/test_acceptance.py` -> `2 passed`
- `pytest -q tools/ci/test_*.py` -> `9 passed`
- `./tools/hw/nh02/install.sh`
- `./tools/hw/nh02/flash.sh`
- `./tools/hw/nh02/acceptance.sh --seconds 15 --log-out /tmp/boatlock-boot.log --json-out /tmp/boatlock-acceptance.json` -> `PASS`
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","dataEvents":1,...}`

Self-review:
- This change removes a real failure mode instead of layering chunking on top of it: ATT notifications now carry one small fixed frame, and the app no longer waits for a giant JSON snapshot.
- The remaining limitation is intentional: detailed config/debug telemetry is not in the live frame. If it is needed, add an explicit frame type or event instead of reviving ad-hoc parameter reads.

Promote to skill:
- Treat live BLE as a fixed, versioned binary observation stream. Use the control point for stream lifecycle and commands; do not reintroduce JSON snapshots or parameter-read tunnels for live state.

### 2026-04-24 Stage 33: No backward-compatibility rule made global

Scope:
- Promote the corrected product assumption into repo rules: BoatLock is not released, so code must not preserve obsolete behavior for backward compatibility unless the user explicitly asks for it.

Key outcomes:
- Added the global no-compatibility rule to `AGENTS.md` and `skills/boatlock/SKILL.md` for firmware, Flutter, tooling, and docs.
- Added BLE-specific guidance: compatibility-only commands must be removed from firmware, Flutter, tests, and docs together.
- Removed the current compatibility-only `SET_STEP_SPR` command handler, Flutter sender, settings UI row, protocol docs row, and command-handler tests.
- Kept `StepSpr` only as fixed runtime geometry/telemetry, not as a user-facing or protocol command.

Validation:
- `cd boatlock && platformio test -e native -f test_ble_command_handler` -> `29/29`
- `cd boatlock_ui && flutter test --no-pub test/settings_page_test.dart` -> passed
- `cd boatlock_ui && flutter analyze` -> clean
- `git diff --check` -> clean
- `rg 'SET_STEP_SPR|setStepSprFixed|compatibility-only|protocol compatibility|Legacy/general' boatlock boatlock_ui docs skills AGENTS.md` -> no runtime/code hits

Self-review:
- The rule is now enforceable, not just prose: the one existing no-op compatibility command was removed from all touched components.
- Existing historical mentions remain only in `WORKLOG.md`, which is the intended place for removed behavior context.

Promote to skill:
- Until explicitly overridden by the user, delete obsolete compatibility behavior from code across all components and keep the reason only in `WORKLOG.md`.

### 2026-04-24 Stage 34: NH02 post-push BLE smoke verification

Scope:
- Verify the pushed `main` on the real `nh02` bench and the Android phone connected to that host.

Key outcomes:
- Proved targets before mutation:
  - `nh02` RFC2217 bridge active on port `4000`
  - ESP32-S3 USB serial present as Espressif USB JTAG serial debug unit
  - Android phone visible via ADB as `68b657f0`, model `220333QNY`
- Refreshed tracked `nh02` helpers with `./tools/hw/nh02/install.sh`.
- Flashed current `main` with `./tools/hw/nh02/flash.sh`; esptool verified all written segments and hard-reset the ESP32-S3.
- Ran serial boot acceptance and saved logs to `/tmp/boatlock-boot-latest.log` plus JSON verdict to `/tmp/boatlock-acceptance-latest.json`.
- Full Android smoke with install attempted first, but MIUI blocked `adb install` with `INSTALL_FAILED_USER_RESTRICTED`; wrapper staged APK at `/sdcard/Download/boatlock-smoke.apk`.
- BLE runtime was then checked with the already-installed app using `--no-install`; it received live telemetry from the flashed firmware.

Validation:
- `./tools/hw/nh02/acceptance.sh --seconds 15 --log-out /tmp/boatlock-boot-latest.log --json-out /tmp/boatlock-acceptance-latest.json` -> `PASS`
- Acceptance matched I2C inventory, BNO08x ready, display ready, EEPROM loaded, BLE init/advertising, stepper config, STOP button, and GPS UART data.
- Boot log scan for `panic|assert|abort|fatal|guru|backtrace|exception|failed|error|brownout|wdt|watchdog|rst:` -> no matches.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> blocked at install stage by MIUI `INSTALL_FAILED_USER_RESTRICTED`.
- `./tools/hw/nh02/android-run-smoke.sh --no-install --wait-secs 100` -> `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","dataEvents":1,"deviceLogEvents":0,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","secPaired":false,"secAuth":false,"rssi":-33,"lastDeviceLog":""}`
- Android app logcat for the running app showed repeated `GATT_SUCCESS`, `onCharacteristicChanged chr: 34cd`, telemetry `mode=IDLE status=WARN`, and no `AndroidRuntime` or relevant BLE error lines.

Self-review:
- This proves the core BLE live path on the phone: scan/connect/subscription/control-point writes/notifications/binary telemetry decode.
- It does not prove a fresh APK update on this Xiaomi without manual confirmation, because MIUI blocked `adb install`.
- It does not yet cover auth, reconnect loops, secured command writes, or intentional actuation commands from the phone.

Promote to skill:
- Treat `--no-install` phone smoke as BLE-runtime proof only, not as proof that the exact just-built APK was installed.

### 2026-04-24 Stage 35: GNSS quality gate best-practice pass

Scope:
- Start the module-by-module best-practice refactor cycle with the GNSS/quality gate module, because it is the first safety-critical input for `ANCHOR_ON`.

External baseline:
- ArduPilot GPS/pre-arm guidance treats GPS lock, HDOP, and GPS glitches as explicit blockers for modes that require position.
- BoatLock should keep the same principle: missing quality evidence is not a pass.

Key outcomes:
- Made HDOP mandatory for anchor-quality GNSS: missing, non-finite, or zero HDOP now returns `GPS_HDOP_MISSING`.
- Added `GPS_HDOP_MISSING` to anchor-denial mapping and stable status strings.
- Fixed `RuntimeGnss::gnssQualityLevel()` so it re-evaluates the current sample instead of trusting a cached previous `NONE` reason.
- Fixed binary BLE status reason mapping to preserve exact GNSS gate names: `GPS_DATA_STALE`, `GPS_POSITION_JUMP`, and `GPS_HDOP_MISSING`.
- Updated Flutter live-frame decoding and BLE protocol docs for the same reason names.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_gnss -f test_gnss_quality_gate -f test_anchor_reasons -f test_runtime_ble_live_frame` -> `13/13`
- `cd boatlock_ui && flutter test --no-pub test/ble_live_frame_test.dart` -> passed
- `git diff --check` -> clean

Self-review:
- This is a behavior-tightening safety change, not a compatibility shim.
- The remaining GNSS module risk is that source selection and filtering are still mixed inside `RuntimeGnss`; next GNSS pass should consider splitting pure quality/sample building from mutable fix application if it reduces code without widening behavior.

Promote to skill:
- For GNSS pre-enable checks, missing quality evidence must fail closed; do not let absent HDOP or stale cached pass state enable anchor control.

### 2026-04-24 Stage 36: Android install blocker narrowed

Scope:
- Stop the Android validation path on the real install blocker instead of continuing through `--no-install`.

Key outcomes:
- Verified the phone on `nh02` is reachable over ADB as Xiaomi `220333QNY`.
- Verified `development_settings_enabled=1`, `adb_enabled=1`, and `verifier_verify_adb_installs=0`.
- Found the remaining phone-side policy signal: the active user restrictions include `no_install_unknown_sources`.
- Classified `INSTALL_FAILED_USER_RESTRICTED` as an exact APK install/update blocker, not a BLE, cable, or firmware blocker.

Validation:
- `ssh root@192.168.88.61 'adb devices -l ... settings/dumpsys checks ...'` -> phone visible, ADB enabled, install policy still restricted.

Self-review:
- The earlier `--no-install` path is useful only for BLE runtime debugging. It must not be used as acceptance for the just-built APK unless explicitly waived.

Promote to skill:
- Full Android smoke must fail closed on `INSTALL_FAILED_USER_RESTRICTED`; resolve the phone-side install policy before claiming app validation.

### 2026-04-24 Stage 37: Xiaomi SIM prerequisite recorded

Scope:
- Clarify the remaining phone-side blocker for exact APK install/update over USB.

Key outcomes:
- After Xiaomi account requirements, MIUI also requested an inserted SIM card before enabling the install-over-USB path.
- Classified this as a hard phone-side prerequisite for canonical `adb install -r`, not something to bypass in BoatLock scripts.

Validation:
- User-observed MIUI prompt after account flow: `insert SIM card`.

Self-review:
- For a stable hardware bench, the best fix is to satisfy the phone policy or use a bench Android device that permits ADB installs without vendor account/SIM gates. Repo wrappers should stay strict and fail closed.

Promote to skill:
- MIUI `Install via USB` may require Xiaomi account plus SIM; do not call the Android smoke path green until exact APK install succeeds through the tracked wrapper.

### 2026-04-24 Stage 38: Android exact install and BLE smoke restored

Scope:
- Recheck the Xiaomi phone after SIM insertion and MIUI `Install via USB` approval.

Key outcomes:
- Confirmed the phone remains visible on `nh02` as ADB device `68b657f0`.
- Proved exact APK update through the tracked wrapper: `Performing Streamed Install` -> `Success`.
- First full smoke after install reached the app but timed out waiting for BLE telemetry; logcat showed repeated scans with no BoatLock match.
- Ran hardware acceptance to reset and verify the firmware advertisement path; boot log matched `[BLE] init ...` and `[BLE] advertising started`.
- Re-ran the full Android wrapper with install and received BLE telemetry from the just-installed app.

Validation:
- `./tools/hw/nh02/android-status.sh` -> phone visible over ADB.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> install success, then `timeout_waiting_for_telemetry`.
- `./tools/hw/nh02/acceptance.sh --seconds 12 --log-out /tmp/boatlock-ble-debug-boot.log --json-out /tmp/boatlock-ble-debug-acceptance.json` -> `PASS`, including BLE advertising.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","dataEvents":1,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","rssi":-30,...}`.

Self-review:
- The install path is now usable without bypasses, but a single transient scan miss happened immediately before the passing run. Keep future BLE acceptance strict and track repeated scan misses as a BLE reliability bug if they recur.

Promote to skill:
- On this Xiaomi bench phone, canonical ADB update requires MIUI account/SIM/install-over-USB approval; after that, use `android-run-smoke.sh` with install as the proof path.

### 2026-04-24 Stage 39: BLE reconnect is first-class app behavior

Scope:
- Make the Flutter BLE client automatically recover while the app is running after disconnect, Bluetooth off/on, heartbeat write failure, or app resume.
- Add a real phone reconnect smoke path instead of relying only on startup connect.

Key outcomes:
- Added `BleReconnectPolicy` as a small pure decision module for reconnect state.
- `BleBoatLock` now observes Bluetooth adapter state and app lifecycle:
  - adapter not ready stops scan and clears stale GATT state
  - adapter ready schedules scan
  - app resume schedules scan unless a usable data/control link already exists
  - disconnect and heartbeat write failure clear stale link state and retry
- Added reconnect smoke mode to `main_smoke.dart` and `BleSmokePage`.
- Added wrapper support:
  - `tools/android/build-smoke-apk.sh --mode reconnect`
  - `tools/android/run-smoke.sh --reconnect`
  - `tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130`
  - remote helper `--cycle-bluetooth`
- Reconnect smoke passes only after first telemetry, a telemetry gap, and telemetry returning without app restart.

Validation:
- `dart format ...` -> clean.
- `bash -n tools/android/build-smoke-apk.sh tools/android/run-smoke.sh tools/hw/nh02/android-run-smoke.sh tools/hw/nh02/remote/boatlock-run-android-smoke.sh` -> clean.
- `cd boatlock_ui && flutter analyze` -> clean.
- `cd boatlock_ui && flutter test --no-pub` -> `26/26` passed after rerun outside sandbox; the first sandbox run failed only because the test runner could not bind `127.0.0.1`.
- `./tools/hw/nh02/install.sh` -> refreshed tracked remote helpers.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","dataEvents":6,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","rssi":-31,...}`.

Self-review:
- This covers the user requirement for an already-running app. If Android kills the process after the user closes/swipes it away, no app code can reconnect until the process is launched again; the startup path is still covered by the same smoke wrapper.
- The reconnect smoke toggles phone Bluetooth through ADB and is intentionally read-only against the boat: it does not send actuation commands.

Promote to skill:
- BLE reconnect changes require both policy/unit coverage and `android-run-smoke.sh --reconnect --wait-secs 130` on `nh02` when the phone is available.

### 2026-04-24 Stage 40: GNSS closure blocked at Android install

Scope:
- Close the GNSS quality-gate slice with full local, firmware, hardware, and Android validation before moving to the next module.

Key outcomes:
- Reverted the briefly-started `RuntimeMotion` helper refactor so the GNSS closure is not mixed with the next module.
- Full native firmware regression passed.
- Full Flutter regression passed.
- ESP32-S3 firmware build passed.
- Offline simulation passed.
- Current firmware was flashed to the `nh02` ESP32-S3 and serial acceptance passed.
- Android basic smoke with exact APK install stopped on MIUI `INSTALL_FAILED_USER_RESTRICTED`; no `--no-install` bypass was used.

Validation:
- `cd boatlock && platformio test -e native` -> `170/170` passed.
- `cd boatlock_ui && flutter test --no-pub` -> `26/26` passed after host-side rerun; sandbox run cannot bind localhost for Flutter test runner.
- `cd boatlock && pio run -e esp32s3` -> success, flash size `724729` bytes.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `./tools/hw/nh02/install.sh` -> remote helpers refreshed.
- `./tools/hw/nh02/flash.sh` -> flashed and verified ESP32-S3 `98:88:e0:03:ba:5c`.
- `./tools/hw/nh02/acceptance.sh --seconds 15 --log-out /tmp/boatlock-gnss-closure-boot.log --json-out /tmp/boatlock-gnss-closure-acceptance.json` -> `PASS`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> blocked at install: `INSTALL_FAILED_USER_RESTRICTED`.

Self-review:
- GNSS firmware/hardware closure is green through serial acceptance, but Android validation is not closed because the exact APK install/update path was denied by MIUI.
- Do not continue to the next module or substitute `--no-install`; phone-side install approval must be restored first.

Promote to skill:
- If MIUI returns `INSTALL_FAILED_USER_RESTRICTED` after a previously successful setup, treat it as a fresh phone-side install approval blocker and stop the acceptance path.

### 2026-04-24 Stage 41: BLE advertising watchdog for post-install scan misses

Scope:
- Investigate and fix the Android smoke failure after exact install succeeded but the app could not find `BoatLock` in BLE scan results.

Key outcomes:
- Logcat showed the phone repeatedly scanning but seeing only unnamed devices and no advertised `BoatLock` name or `12ab` service.
- Added `BleAdvertisingWatchdog.h`, a pure decision helper for stale BLE advertising/server state.
- `BLEBoatLock.loop()` now checks once per second:
  - if server has clients but local state is not connected, mark connected
  - if local state says connected but server has no clients, clear stale stream/subscription state and restart advertising
  - if no clients and advertising is stopped, restart advertising
- Deliberately did not enable continuous advertising while connected. Multi-client BLE needs an explicit design: multiple read-only subscribers, one control owner/lease, and per-client auth/session or write rejection.

Validation:
- `cd boatlock && platformio test -e native -f test_ble_advertising_watchdog -f test_runtime_ble_live_frame -f test_ble_command_handler` -> `36/36` passed.
- `cd boatlock && pio run -e esp32s3` -> success, flash size `724917` bytes.
- `cd boatlock && platformio test -e native` -> `175/175` passed.
- `./tools/hw/nh02/flash.sh` -> built, flashed, and verified ESP32-S3 `98:88:e0:03:ba:5c`.
- `./tools/hw/nh02/acceptance.sh --seconds 15 --log-out /tmp/boatlock-ble-watchdog-boot.log --json-out /tmp/boatlock-ble-watchdog-acceptance.json` -> `PASS`, including BNO08x, display, EEPROM, BLE advertising, stepper config, STOP button, and GPS UART.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> exact APK install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","dataEvents":1,...}`.

Self-review:
- This fixes self-recovery when advertising actually stopped or BLE state went stale, without widening the control surface to multiple simultaneous clients.
- The next BLE module should explicitly design multi-client behavior instead of treating "advertise while connected" as a safe toggle.
- The previously blocked GNSS Android acceptance path is no longer blocked in this run because the exact APK install/update path succeeded again.

Promote to skill:
- Do not enable always-advertising/multi-central behavior until control ownership and per-client session semantics are defined and tested.
- For Xiaomi install flakiness, one repeat of the exact install after phone-side approval is acceptable; if it fails again, stop and fix phone policy instead of using `--no-install`.

### 2026-04-24 Stage 42: Android smoke covers ESP32 reboot recovery

Scope:
- Add a hardware acceptance scenario for the phone staying connected/recovering when the ESP32-S3 reboots while the app is running.

Key outcomes:
- Reused the existing reconnect smoke APK state machine: first telemetry, telemetry gap, telemetry after reconnect.
- Added `tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130`.
- The `nh02` wrapper now triggers the gap with the tracked `/opt/boatlock-hw/bin/boatlock-reset-esp32s3.sh` helper instead of phone Bluetooth cycling.
- Kept this as a separate acceptance target from Bluetooth off/on so failures point at the correct side of the link.

Validation:
- `bash -n tools/hw/nh02/android-run-smoke.sh tools/hw/nh02/remote/boatlock-run-android-smoke.sh tools/android/build-smoke-apk.sh tools/android/run-smoke.sh` -> clean.
- `./tools/hw/nh02/install.sh` -> refreshed the tracked remote helper on `nh02`; RFC2217 service returned active.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact APK install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","dataEvents":4,...}` after phone Bluetooth cycle.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact APK install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","dataEvents":4,...}` after ESP32 reset.

Self-review:
- This proves recovery while the app process is alive. If Android kills the app process after the user closes it, automatic BLE reconnect starts only when the app is launched again.
- The test remains read-only: it does not send anchor or manual actuation commands.

Promote to skill:
- ESP32 reboot recovery must be tested with `android-run-smoke.sh --esp-reset --wait-secs 130` after BLE reconnect changes or firmware changes that affect advertising/connection lifecycle.

### 2026-04-24 Stage 43: BNO08x I2C/event-stream failure isolated

Scope:
- Investigate the observed real-hardware compass freeze and `Wire.cpp requestFrom(): i2cRead returned Error -1`.

Key outcomes:
- Confirmed the original acceptance wrapper missed Arduino `[E]` logs; it now fails on Arduino error logs, `COMPASS lost`, `COMPASS retry ready=0`, and `I2C address not found`.
- Added `RuntimeCompassHealth` so stale/missing BNO08x events fail closed as no heading instead of leaving the screen/control path with a frozen last heading.
- Reduced BNO08x polling to a bounded cadence and added `wasReset()` handling to re-enable BNO08x reports after SH2 reset, matching the Adafruit example pattern.
- Current `nh02` evidence after those fixes:
  - I2C scan sees `0x4B(BNO08x)`
  - firmware logs `[COMPASS] reset detected reports=1`
  - no first sensor event arrives
  - firmware logs `[COMPASS] lost reason=FIRST_EVENT_TIMEOUT`
  - retry then prints `I2C address not found` and `[COMPASS] retry ready=0`
- Conclusion for this bench state: raw I2C address visibility is not enough; the BNO08x SH2/report stream is unusable. This is now fail-closed in software, but the remaining root cause is likely hardware/power/reset-line/bus integrity unless a full sensor power-cycle restores it.

Validation:
- `pytest tools/hw/nh02/test_acceptance.py` -> `4/4` passed.
- `cd boatlock && platformio test -e native -f test_runtime_compass_health -f test_runtime_telemetry_cadence` -> `7/7` passed after host-side rerun.
- `cd boatlock && pio run -e esp32s3` -> success, flash size `725393` bytes.
- `./tools/hw/nh02/flash.sh --no-build` -> flashed and verified ESP32-S3 `98:88:e0:03:ba:5c`.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-compass-reset-reports-boot.log --json-out /tmp/boatlock-compass-reset-reports-acceptance.json` -> `FAIL` by design on `compass_lost`, `i2c_address_not_found`, and `compass_retry_failed`.

Self-review:
- The safety behavior is better: stale compass no longer remains trusted.
- The hardware bench is not green; do not proceed to anchor/control validation until BNO08x produces fresh events again.

Promote to skill:
- Compass acceptance must require live BNO08x event health, not only an I2C address and a boot-time `ready=1` line.

### 2026-04-24 Stage 44: Compass heading-event hardening and long acceptance

Scope:
- Finish the BNO08x compass slice after the real bench showed the screen heading could freeze and serial logged `Wire.cpp requestFrom(): i2cRead returned Error -1`.

Key outcomes:
- Researched official BNO08x guidance and captured the durable conclusion in `docs/COMPASS_BNO08X.md`: BNO08x I2C is known-problematic with ESP32/ESP32-S3, so I2C remains bring-up/diagnostic only; stable production wiring should move to UART-RVC or SPI with reset wiring.
- Changed `BNO08xCompass::read()` to check `wasReset()` before reading events, clear cached event state on reset, re-enable reports, and return explicit `anyEvent` vs `headingEvent`.
- Runtime freshness now uses only rotation-vector heading events via `lastHeadingEventAgeMs`; gyro/mag events no longer keep heading alive.
- Added `[COMPASS] heading events ready ...` and made hardware acceptance require that marker.
- Fixed reset grace handling: after a BNO reset and successful report re-enable, the first-heading-event timeout starts from the reset/reconfigure time, not from original boot.
- `SET_ANCHOR` and cardinal nudge now use `headingAvailable()` instead of raw `compassReady`, so stale heading is not saved during reset or dropout windows.

Validation:
- `pytest tools/hw/nh02/test_acceptance.py` -> `5/5` passed.
- `cd boatlock && platformio test -e native -f test_runtime_compass_health -f test_runtime_telemetry_cadence -f test_ble_command_handler` -> `38/38` passed.
- `cd boatlock && pio run -e esp32s3` -> success, flash size `725665` bytes.
- `./tools/hw/nh02/flash.sh --no-build` -> flashed and verified ESP32-S3 `98:88:e0:03:ba:5c`.
- First short hardware acceptance -> `PASS`, but a subsequent 180 s run failed around 65 s on `Wire.cpp`, `COMPASS lost`, `I2C address not found`, and retry failure. This confirmed short boot acceptance was insufficient for compass stability.
- After reset-grace and heading-gate fixes, `./tools/hw/nh02/acceptance.sh --seconds 180 --log-out /tmp/boatlock-compass-final-180s.log --json-out /tmp/boatlock-compass-final-180s.json` -> `PASS`, with `[COMPASS] heading events ready age=4` and no fatal errors.
- `cd boatlock && platformio test -e native` -> `181/181` passed.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> blocked at exact APK install with `INSTALL_FAILED_USER_RESTRICTED: Install canceled by user`; no `--no-install` fallback was used.
- `git diff --check` -> clean.

Self-review:
- Software now fails closed on stale heading and acceptance can catch both no-event and later Wire/I2C failure cases.
- The bench passed 180 seconds, but official vendor guidance still makes ESP32-S3+BNO08x I2C a weak production choice. Do not treat this as final hardware architecture for alpha if compass stability matters on water.
- Android BLE smoke is not closed for this slice because MIUI blocked exact APK install; rerun the canonical wrapper after phone-side install approval is restored.
- Next hardware improvement should be wiring BNO08x UART-RVC or SPI plus reset, then adding an acceptance mode that proves heading continuity on that transport.

Promote to skill:
- Compass acceptance needs a long enough capture to catch delayed BNO08x I2C failures; a boot-only pass is not sufficient for this module.
- Fresh heading means rotation-vector events only.

### 2026-04-24 Stage 45: BNO08x GPIO13 reset trial is not sufficient

Scope:
- Verify whether the newly wired BNO08x reset candidate on GPIO13 can make the current ESP32-S3 I2C compass path stable enough for repeated hardware acceptance.

Key outcomes:
- Used the tracked `gpio_probe` sketch to identify the unknown accessible header pin:
  - GPIO15 toggled first but is the STOP button path, so it is not usable for compass reset.
  - GPIO13 toggled cleanly and was selected for the BNO08x reset trial.
  - GPIO17 showed expected GPS RX activity/noise and must not be reused.
- Added and flashed the `esp32s3_rst13` firmware environment.
- Strengthened BNO08x recovery to close the SH2 session, pulse the configured reset pin, restart I2C, and require fresh rotation-vector heading events before heading is trusted.
- Repeated 180 s hardware acceptance through the canonical `nh02` path:
  - `/tmp/boatlock-compass-rst13-hardreset-1.log` passed with `[COMPASS] reset pin=13 pulse=1` and `[COMPASS] heading events ready age=4`.
  - `/tmp/boatlock-compass-rst13-hardreset-2.log` failed with `[COMPASS] lost reason=FIRST_EVENT_TIMEOUT`, repeated `[COMPASS] reset pulse source=recovery pin=13 pulse=1`, repeated `[COMPASS] retry ready=0 source=BNO08x`, while I2C inventory still saw `0x4B(BNO08x)`.
  - `/tmp/boatlock-compass-rst13-hardreset-3.log` failed from boot with `[COMPASS] ready=0 source=BNO08x`, repeated reset pulses, repeated retry failures, and persistent `0x4B(BNO08x)` inventory.
- Conclusion: GPIO13 toggling is real at the ESP32 side, but the current BNO08x I2C path remains unstable. A visible I2C ACK and `pulse=1` do not prove the BNO08x report stream recovered.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_compass_recovery -f test_runtime_compass_health` -> `9/9` passed before the final flash.
- `./tools/hw/nh02/flash.sh --env esp32s3_rst13` -> build `SUCCESS`, flash verified.
- `./tools/hw/nh02/acceptance.sh --seconds 180 --log-out /tmp/boatlock-compass-rst13-hardreset-1.log --json-out /tmp/boatlock-compass-rst13-hardreset-1.json` -> `PASS`.
- `./tools/hw/nh02/acceptance.sh --seconds 180 --log-out /tmp/boatlock-compass-rst13-hardreset-2.log --json-out /tmp/boatlock-compass-rst13-hardreset-2.json` -> `FAIL`.
- `./tools/hw/nh02/acceptance.sh --seconds 180 --log-out /tmp/boatlock-compass-rst13-hardreset-3.log --json-out /tmp/boatlock-compass-rst13-hardreset-3.json` -> `FAIL`.
- `pytest tools/hw/nh02/test_acceptance.py` -> `5/5` passed after documenting the failed trial.
- `cd boatlock && platformio test -e native -f test_runtime_compass_recovery -f test_runtime_compass_health` -> `9/9` passed after documenting the failed trial.
- `git diff --check` -> clean.

Self-review:
- The safety behavior remains correct because heading fails closed and anchor cannot rely on stale compass data.
- The hardware path is not acceptable for anchor validation: repeated reset pulses do not restore BNO08x heading events once the SH2/report stream is absent.
- The next hardware cut should not be more I2C timing tweaks. If the module wiring is accessible, move to UART-RVC first for heading-only operation, or SPI if full SH2 report control is required.

Promote to skill:
- Do not treat `[COMPASS] reset pulse ... pulse=1` as proof that the BNO08x recovered. Recovery is proven only by fresh `[COMPASS] heading events ready` after the pulse and a clean long acceptance capture.
- For this bench, `esp32s3_rst13` is a reset trial environment, not a green production compass transport.

### 2026-04-24 Stage 46: BNO08x production migration to UART-RVC

Scope:
- Move the main firmware compass path from ESP32-S3 I2C/SH2 to BNO08x UART-RVC on the confirmed `nh02` wiring, with no backward compatibility path.

Key outcomes:
- Replaced the production `BNO08xCompass` implementation with a small UART-RVC reader on `HardwareSerial2`, `RX=12`, `baud=115200`, reset `GPIO13`.
- Added `BnoRvcFrame` parser coverage for checksum, little-endian decode, scaling, and stream resync.
- Removed production I2C/SH2 compass recovery code, old I2C debug sketches, Adafruit BNO08x/BusIO dependencies, and the `esp32s3_rst13` trial environment.
- Updated hardware acceptance so compass readiness is `[COMPASS] ready=1 source=BNO08x-RVC rx=12 baud=115200` plus fresh `[COMPASS] heading events ready`.
- Captured the user correction that active development targets one default ESP32-S3 bench board; do not add new `BOATLOCK_BOARD_JC4832W535` runtime branches unless explicitly requested.

Validation:
- `pytest tools/hw/nh02/test_acceptance.py` -> `5/5` passed.
- `cd boatlock && platformio test -e native -f test_bno_rvc_frame -f test_runtime_compass_health` -> `7/7` passed.
- `cd boatlock && platformio test -e native` -> `179/179` passed after deleting the empty removed-suite directory.
- `cd boatlock && pio run -e esp32s3` -> success, flash size `694941` bytes.
- `git diff --check` -> clean.
- `./tools/hw/nh02/flash.sh --no-build` initially failed because the expected canonical artifact shape was missing; reran `./tools/hw/nh02/flash.sh`, which rebuilt and flashed production firmware successfully.
- `./tools/hw/nh02/acceptance.sh --seconds 180 --log-out /tmp/boatlock-rvc-production-180s.log --json-out /tmp/boatlock-rvc-production-180s.json` -> `PASS`, matched RVC compass ready, heading events, display, EEPROM, BLE advertising, stepper, STOP, and GPS UART with no missing checks or errors.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> exact APK install succeeded after the wrapper retried the MIUI `USER_RESTRICTED` first attempt; `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.

Self-review:
- This is a real architecture simplification: production no longer contains a second compass transport or SH2 reset/reconfigure branch.
- `Wire` still appears in the PlatformIO dependency graph through the display library, but the production compass code no longer includes or uses `Wire`.
- RVC gives stable heading/acceleration for anchor needs, but it does not expose SH2 calibration quality or RV accuracy; those fields remain neutral placeholders and should not be used as acceptance criteria.

Promote to skill:
- Current BoatLock development is single-board on the default `nh02` bench. Do not add new alternate-board runtime branches for future changes unless the user explicitly asks for that board.
- If `flash.sh --no-build` fails because artifacts are missing, do not hand-copy or reconstruct artifacts; run the canonical `flash.sh` so build and deploy stay in one tracked path.

### 2026-04-24 Stage 47: GNSS quality sample simplification

Scope:
- Continue module-by-module refactor with the GNSS/source/quality gate layer after the compass migration.

Key outcomes:
- Split `RuntimeGnss` quality evaluation into two explicit builders: `qualityConfigFromSettings()` and `qualitySample()`.
- Removed duplicated `controlGpsAvailable()` checks from the quality gate path.
- Added coverage that phone GPS fallback does not reuse previous hardware HDOP, speed, accel, satellite, or sentence metrics as control quality.
- Promoted the invariant that control GNSS is hardware-only; phone GPS is UI/telemetry fallback, not anchor quality input.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_gnss -f test_gnss_quality_gate -f test_ble_command_handler` -> `40/40` passed.
- `cd boatlock && platformio test -e native` -> `180/180` passed.
- `cd boatlock && pio run -e esp32s3` -> success, flash size `694985` bytes.
- `git diff --check` -> clean.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-gnss-refactor-60s.log --json-out /tmp/boatlock-gnss-refactor-60s.json` -> `PASS`, including RVC compass, display, EEPROM, BLE advertising, stepper, STOP, and GPS UART.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> exact APK install `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.

Self-review:
- This is intentionally a small KISS refactor: behavior stays the same, but the safety boundary is now testable as data instead of being implicit in repeated conditionals.
- Hardware acceptance proves the boot/GPS UART path and BLE telemetry still work; it does not prove outdoor GPS fix quality because the bench currently reports `NO_GPS` in the Android smoke result.
- I accidentally ran `pio test` and `pio run` concurrently against the same project; it did not fail this time, but that should not be normalized because PlatformIO shares `.pio/build`.

Promote to skill:
- GNSS quality samples must be built from hardware control fixes only. Phone GPS fallback should not carry over hardware quality fields.
- Do not parallelize PlatformIO build and test in the same project/build directory unless build dirs are isolated.

### 2026-04-24 Stage 48: Motion/Motor refactor with explicit external baseline

Scope:
- Continue the module-by-module KISS refactor with actuator quieting and anchor thrust state.
- Correct the workflow after the user pointed out that each module pass must start from external best practices, not only local code intuition.

External baseline:
- ArduPilot Rover `Hold` mode is a dedicated quiet state where vehicle outputs go to safe trims and failsafes commonly switch into that mode: <https://ardupilot.org/rover/docs/hold-mode.html>.
- ArduPilot Rover failsafe recovery is explicit: after link recovery, operator action is still required before control resumes: <https://ardupilot.org/rover/docs/rover-failsafes.html>.
- ArduPilot Rover boat `Loiter` is zone-based: drift inside radius, bounded correction outside radius, with deadband/throttle considerations: <https://ardupilot.org/rover/docs/loiter-mode.html>.
- ArduPilot Rover motor configuration treats throttle type, deadband, direction testing, and mechanically safe low-throttle tests as first-class setup: <https://ardupilot.org/rover/docs/rover-motor-and-servo-configuration.html>.
- OpenCPN Anchor Watch fails closed on GPS loss for anchor monitoring, reinforcing that missing position evidence is a safety event: <https://opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:manual_advanced:features:auto_anchor>.

Key outcomes:
- Added an explicit repo rule: before refactoring any module, do a targeted external best-practice pass from official/primary sources and record the applied baseline.
- Updated the BoatLock skill so the baseline check is part of the normal working pattern and self-review loop.
- Added `RuntimeMotion::quietAutoOutputs()` and `quietAllOutputs()` to centralize safe actuator quieting instead of duplicating partial stop blocks.
- Made hard `MotorControl::stop()` clear hidden anchor-auto state, filtered distance/rate state, ramp state, and PWM state.
- Kept normal anchor-controller idle separate from hard stop via `stopAnchorOutput()`, so anti-hunt min on/off windows are preserved during ordinary zone-based control.
- Added tests proving hard stop clears auto-thrust state and loss of control GNSS clears auto thrust through `RuntimeMotion`.

Validation:
- `cd boatlock && platformio test -e native -f test_motor_control -f test_runtime_motion -f test_anchor_supervisor -f test_anchor_safety` initially caught a real regression in anti-hunt timing; fixed by separating hard stop from controller idle output.
- `cd boatlock && platformio test -e native -f test_motor_control -f test_runtime_motion -f test_anchor_supervisor -f test_anchor_safety` -> `28/28` passed.
- `cd boatlock && platformio test -e native` -> `182/182` passed.
- `cd boatlock && pio run -e esp32s3` -> success, flash size `695045` bytes.
- `git diff --check` -> clean.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-motion-refactor-60s.log --json-out /tmp/boatlock-motion-refactor-60s.json` -> `PASS`, including RVC compass, display, EEPROM, BLE advertising, stepper, STOP, heading events, and GPS UART.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> exact APK install eventually `Success` after two MIUI `USER_RESTRICTED` retries, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.

Self-review:
- This is a small safety simplification, not a broad controller rewrite: the public behavior stays zone-based and bounded, while hard stop now removes hidden actuator state.
- The targeted test failure was valuable because it prevented turning a safety hard-stop rule into ordinary anti-hunt instability.
- Hardware acceptance proves boot/device health and BLE telemetry after this firmware; it does not prove loaded thrust behavior on a real prop/boat.

Promote to skill:
- External best-practice review is mandatory per module before refactor. Use official/primary sources first and record the baseline before or with the worklog entry.
- Keep hard actuator stop distinct from ordinary controller idle output; failsafe/STOP should clear hidden state, while zone-control idle should preserve anti-hunt timing.

### 2026-04-24 Stage 49: Runtime supervisor fail-closed simplification

Scope:
- Continue the module-by-module refactor with `AnchorSupervisor`, `RuntimeSupervisorPolicy`, and the related settings/schema surface.
- Remove optional behavior that can widen the actuation surface during faults.

External baseline:
- ArduPilot Rover failsafes switch to deterministic actions such as `Hold` or disarm, and after link recovery the operator must explicitly retake control: <https://ardupilot.org/rover/docs/rover-failsafes.html>.
- ArduPilot pre-arm checks block movement until the fault is resolved and warn against disabling arming checks except for bench testing: <https://ardupilot.org/rover/docs/common-prearm-safety-checks.html>.
- ArduPilot `Hold` mode is the quiet mode used for arming/disarming and failsafe behavior: <https://ardupilot.org/rover/docs/hold-mode.html>.
- PX4 safety configuration also treats data-link loss and telemetry loss as explicit failsafe triggers with configured hold/return/land-style actions, not silent recovery: <https://docs.px4.io/main/en/config/safety>.

Key outcomes:
- Removed legacy `AnchorSafety` and its test suite; `AnchorSupervisor` is the single runtime safety decision module now.
- Removed `SafeAction::MANUAL` and the `FailAct` / `NanAct` settings. Runtime failsafes now always stop motion and latch `HOLD`; manual control requires a new explicit command.
- Removed unused `MaxThrustS` from settings and config docs.
- Bumped `Settings::VERSION` and `docs/CONFIG_SCHEMA.md` from `0x15` to `0x16`; `nh02` migrated EEPROM to `ver=22`.
- Replaced zero-as-sentinel GPS/sensor timers with explicit active flags so failures starting at `millis()==0` still time out correctly.
- Made containment breach priority explicit over a simultaneous GPS weak grace expiry.
- Added regression tests for zero-time GPS/sensor faults, containment priority, and fail-closed NaN handling.

Validation:
- `cd boatlock && platformio test -e native -f test_anchor_supervisor -f test_runtime_supervisor_policy -f test_runtime_motion -f test_settings -f test_ble_command_handler -f test_runtime_control` -> `66/66` passed.
- `python3 tools/ci/check_config_schema_version.py` -> `config schema version OK: 0x16`.
- First full native run caught an empty deleted `test_anchor_safety` suite; removed the empty directory and reran.
- `cd boatlock && platformio test -e native` -> `181/181` passed.
- `cd boatlock && pio run -e esp32s3` -> success, flash size `694625` bytes.
- `git diff --check` -> clean.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-supervisor-refactor-60s.log --json-out /tmp/boatlock-supervisor-refactor-60s.json` -> `PASS`, including EEPROM `ver=22`, RVC compass, display, BLE advertising, stepper, STOP, GPS UART, and heading events.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> exact APK install `Success` after one MIUI `USER_RESTRICTED` retry, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.

Self-review:
- This is a deliberate behavior tightening: no automatic manual mode on faults. That matches the safety baseline and the current product scope.
- Manual BLE commands still exist as a separate service/dev control surface; they were not removed in this supervisor slice.
- Hardware acceptance and Android smoke prove boot, migration, BLE telemetry, and sensor presence, but not real loaded thrust behavior.

Promote to skill:
- Supervisor/failsafe logic must not auto-enter manual. Fault recovery should require an explicit operator command after the quiet state is reached.
- Do not keep empty PlatformIO test suite directories after removing a module; full `pio test` treats them as errors.

### 2026-04-24 Stage 50: Hardware button debounce pass

Scope:
- Continue the module-by-module refactor with `HoldButtonController` and `RuntimeButtons`.
- Reduce accidental physical-input triggers for BOOT anchor save, STOP emergency stop, and STOP long-press pairing.

External baseline:
- ArduPilot pre-arm checks treat safety state and input/config faults as blockers before movement: <https://ardupilot.org/rover/docs/common-prearm-safety-checks.html>.
- ArduPilot safety switch behavior keeps physical safety controls explicit instead of hidden in normal control flow: <https://ardupilot.org/rover/docs/common-safety-switch-pixhawk.html>.
- ESP-IDF GPIO guidance treats GPIO as raw hardware input; firmware must configure and interpret input state deliberately: <https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gpio.html>.

Key outcomes:
- Added 40 ms debounce to `HoldButtonController` before emitting press/release edges.
- BOOT anchor save still requires stable long press; a short GPIO bounce no longer starts a valid hold cycle.
- STOP emergency action now fires on a debounced press edge; STOP pairing window still requires stable 3 s hold.
- Added button tests for debounce timing, zero-time press, release after debounce, and short bounce suppression.
- Promoted a durable rule: raw physical button input must be debounced before state-changing actions.

Validation:
- `cd boatlock && platformio test -e native -f test_hold_button_controller -f test_runtime_buttons -f test_ble_command_handler` -> `39/39` passed.
- `cd boatlock && platformio test -e native` -> `183/183` passed.
- `cd boatlock && pio run -e esp32s3` -> success, flash size `694677` bytes.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-buttons-refactor-60s.log --json-out /tmp/boatlock-buttons-refactor-60s.json` -> `PASS`, including RVC compass, display, EEPROM `ver=22`, BLE advertising, stepper, STOP, heading events, and GPS UART.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> exact APK install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.

Self-review:
- Debounce delays hardware STOP by 40 ms. That is an acceptable tradeoff to suppress raw GPIO bounce without making STOP a long-press action.
- Hardware acceptance proves normal boot and BLE still work, but it does not physically inject button bounce on `nh02`; bounce behavior is covered by native unit tests.

Promote to skill:
- Physical inputs are raw and unsafe until debounced. State-changing actions need stable edges or stable long-press, not single-sample GPIO reads.

### 2026-04-24 Stage 51: BLE manual control standardization

Scope:
- Continue BLE module refactor after correcting the assumption that manual control should be cut from `main`.
- Keep manual control as core scope for phone and future BLE joystick/remotes, but replace split commands with a safer standardized input model.

External baseline:
- Bluetooth GATT write requests return a write response or error for a single characteristic value and are bounded by ATT MTU; this favors small explicit command payloads over serial-style command streams: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.
- Android `BluetoothGatt.writeCharacteristic(..., writeType)` reports completion through `onCharacteristicWrite`; use that as transport status, not as a substitute for an application-level safety contract: <https://developer.android.com/reference/android/bluetooth/BluetoothGatt>.
- Android exposes write types including default acknowledged write and no-response write; control commands should keep acknowledged writes where the client needs a result: <https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic>.
- Bluetooth HID over GATT is the standard direction for future generic BLE joystick/remotes, but BoatLock should map any such input into one internal manual-control state instead of adding a second actuator path: <https://www.bluetooth.com/specifications/specs/hid-over-gatt-profile/>.

Key outcomes:
- Corrected repo scope: manual control is core product functionality, not a feature to remove.
- Added `ManualControl` with source, atomic steer/throttle input, valid ranges, and a short deadman TTL.
- Replaced split BLE commands `MANUAL`, `MANUAL_DIR`, and `MANUAL_SPEED` with `MANUAL_SET:<steer>,<throttlePct>,<ttlMs>` plus `MANUAL_OFF`.
- `MANUAL_SET` disables Anchor on entry, so timeout, reconnect, or app crash cannot resume Anchor unexpectedly.
- `ANCHOR_ON`, `ANCHOR_OFF`, STOP/failsafe paths now clear manual state and quiet manual outputs.
- Flutter now builds/sends the atomic manual command and exposes `manualOff()`.
- Added Android `--manual` smoke mode: installs exact APK, sends zero-throttle `MANUAL_SET`, observes `MANUAL`, sends `MANUAL_OFF`, and observes mode exit.
- Updated BLE protocol docs, repo notes, validation references, and hardware acceptance skill.

Validation:
- `cd boatlock && platformio test -e native -f test_manual_control -f test_ble_command_handler -f test_runtime_motion -f test_runtime_control` -> `49/49` passed.
- `cd boatlock_ui && flutter test --no-pub` with `HOME=/tmp XDG_CACHE_HOME=/tmp` -> all tests passed.
- `cd boatlock && pio run -e esp32s3` -> success, flash size `695133` bytes.
- `cd boatlock && platformio test -e native` -> `189/189` passed.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-manual-standard-60s.log --json-out /tmp/boatlock-manual-standard-60s.json` -> `PASS`, including RVC compass, display, EEPROM `ver=22`, BLE advertising, stepper, STOP, heading events, and GPS UART.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> exact APK install eventually `Success` after two MIUI `USER_RESTRICTED` retries, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/android/build-smoke-apk.sh --mode manual` initially failed in sandbox with Gradle `SocketException: Operation not permitted`; rerun outside sandbox succeeded.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact APK install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip","dataEvents":4,...,"mode":"IDLE"}`.

Self-review:
- This intentionally changes the protocol because there is no alpha-release compatibility requirement. Old split commands now fall through as unhandled and are covered by a regression test.
- Manual smoke is zero-throttle only. It proves phone-to-ESP32 BLE command flow and mode transition, not powered thrust or steering under load.
- Future BLE joystick work should reuse `ManualControl` and add a control-owner/lease model before supporting multiple simultaneous controllers.

Promote to skill:
- Manual control is core scope for BoatLock, but must be atomic, deadman-protected, and source-agnostic.
- Use zero-throttle Android manual smoke after manual BLE protocol changes; basic smoke is telemetry-only and is not enough for this module.

### 2026-04-24 Stage 52: Phone manual-control UI completion

Scope:
- Complete the phone side of manual control after standardizing the BLE command.
- Keep the UI consistent with operational safety: no one-tap latched motion from the main map.

External baseline:
- Material FAB guidance recommends one primary floating action per screen and warns against using FABs for minor/destructive/control actions: <https://m1.material.io/components/buttons-floating-action-button.html>.
- The BLE/manual baseline from Stage 51 still applies: manual actuation must be atomic and deadman-protected.

Key outcomes:
- Added `ManualControlSheet`, opened from an app-bar joystick icon instead of adding another main-map FAB.
- Manual UI sends `MANUAL_SET` only while a directional/throttle button is held.
- Manual UI refreshes every `250 ms` with a `500 ms` firmware TTL.
- Releasing all controls or pressing STOP sends `MANUAL_OFF`.
- Widget disposal also sends `MANUAL_OFF` if a manual command was active.
- Added widget tests proving hold-to-send and release-to-off behavior.

Validation:
- `cd boatlock_ui && flutter test --no-pub` with `HOME=/tmp XDG_CACHE_HOME=/tmp` -> `29/29` passed.
- `cd boatlock_ui && flutter build apk --debug --no-pub` with `HOME=/tmp XDG_CACHE_HOME=/tmp` -> success, built `build/app/outputs/flutter-apk/app-debug.apk`.

Self-review:
- This validates the UI command contract locally and the BLE manual roundtrip on phone was already proven by Stage 51 manual smoke.
- I did not run interactive phone UI automation for this sheet; current hardware proof is the zero-throttle smoke app, not a tap-through of the production map UI.

Promote to skill:
- Phone manual control should remain press-and-hold/deadman UI. Do not add one-tap latched manual actuation to the main map.

### 2026-04-24 Stage 53: Settings EEPROM write-policy pass

Scope:
- Continue module-by-module refactor with `Settings` and EEPROM persistence.
- Reduce flash write amplification without changing the stored schema or widening runtime behavior.

External baseline:
- Arduino-ESP32 documents `Preferences`/NVS as the ESP32 replacement for Arduino EEPROM and recommends it for retained small values: <https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html>.
- ESP-IDF NVS is append-oriented, includes flash wear levelling, and requires explicit commit for guaranteed persistence: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html>.
- Espressif FAQ says NVS has internal wear levelling and is designed to resist accidental power loss, but flash writes remain a bounded-lifetime/runtime-constrained operation: <https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/storage/nvs.html>.

Key outcomes:
- Kept the storage schema unchanged at `Settings::VERSION = 0x16`; no EEPROM/NVS migration in this slice.
- Added dirty-state guarding to `Settings::save()`, so no-op `set()` calls and clean post-load state do not commit flash.
- Kept forced persistence for version migration, CRC recovery, and boot-time value normalization.
- Made `Settings::set()` reject non-finite runtime values instead of allowing NaN to enter RAM and later EEPROM.
- Extended native EEPROM mock with `commitCount` and reset support so tests can prove write policy, not only values.
- Updated config docs and repo skill references with the new write-policy invariant.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_settings -f test_ble_command_handler -f test_runtime_motion -f test_motor_control` -> `59/59` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `192/192` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `695257` bytes.
- `python3 tools/ci/check_config_schema_version.py` -> `config schema version OK: 0x16`.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `bash -n tools/android/build-smoke-apk.sh tools/android/run-smoke.sh tools/hw/nh02/android-run-smoke.sh` -> clean.
- `git diff --check` -> clean.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-settings-dirty-60s.log --json-out /tmp/boatlock-settings-dirty-60s.json` -> `PASS`, including RVC compass, display, EEPROM `ver=22`, BLE advertising, stepper, STOP, GPS UART, and heading events.
- Acceptance log check found `[EEPROM] settings loaded (ver=22)` and no `[EEPROM] settings saved`, proving clean boot did not rewrite settings.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> exact APK install `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact APK install `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact APK install `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.

Self-review:
- This is intentionally not a Preferences/NVS migration. The safe cut is to remove unnecessary commits first while preserving the known CRC/versioned image.
- Dirty guarding depends on every write path using `Settings::set()` or `setStrict()` before `save()`. Current audited call sites do that.
- The next deeper storage pass should consider moving settings and security records to typed NVS namespaces, but only as a separate migration with rollback/acceptance planning.

Promote to skill:
- Settings persistence should be dirty-state guarded. No-op saves must not commit flash.
- Non-finite settings values should fail closed before they reach persisted storage.

### 2026-04-24 Stage 54: Anchor point persistence pass

Scope:
- Continue module-by-module refactor with `AnchorControl` and BLE/BOOT/nudge anchor-point save paths.
- Make the anchor point a validated explicit state transition instead of a helper that can silently arm Anchor by default.

External baseline:
- OpenCPN Auto Anchor Mark creates anchor marks only after conservative conditions and deduping to avoid spurious waypoints: <https://opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:manual_advanced:features:anchor_auto>.
- Signal K Anchor Alarm separates anchor drop, rode settling, armed state, and supports moving the anchor location after setup: <https://demo.signalk.org/documentation/Guides/Anchor_Alarm.html>.
- ArduPilot Rover boat configuration treats position holding as a deliberate Loiter capability for boats, not as a side effect of saving coordinates: <https://ardupilot.org/rover/docs/boat-configuration.html>.

Key outcomes:
- Removed the default `enableAnchor=true` from `AnchorControl::saveAnchor`; all callers must explicitly state whether saving the point should keep Anchor enabled.
- `AnchorControl` now rejects non-finite, out-of-range, and `0,0` anchor points instead of letting `Settings::set()` clamp bad coordinates into persisted state.
- Heading is normalized to `[0,360)` before persistence.
- `SET_ANCHOR` still saves only the point and cannot enable Anchor; `ANCHOR_ON` remains the explicit enable gate.
- BOOT button save and nudge paths now check the `saveAnchor()` result instead of logging success unconditionally.
- Anchor configured detection now uses `AnchorControl::validAnchorPoint()`; duplicated GNSS helper was removed.
- BLE protocol docs and firmware reference were updated with the explicit anchor-save behavior.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_anchor_control -f test_ble_command_handler -f test_runtime_anchor_gate -f test_runtime_gnss` -> `45/45` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `195/195` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `695869` bytes.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-anchor-control-60s.log --json-out /tmp/boatlock-anchor-control-60s.json` -> `PASS`, including RVC compass, display, EEPROM `ver=22`, BLE advertising, stepper, STOP, GPS UART, and heading events.
- Acceptance log scan found no panic/assert/Arduino `[E]`, compass loss, compass retry failure, or unexpected `ANCHOR_REJECTED`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> exact APK install `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact APK install `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> first install attempt hit MIUI `USER_RESTRICTED`, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.

Self-review:
- This is a small semantic tightening: invalid anchor inputs are rejected rather than clamped. That is safer for a physical hold point.
- `0,0` remains reserved as "no anchor point", matching existing runtime behavior. If a real deployment ever needs the Gulf of Guinea coordinate, that requires a schema/state change rather than overloading default coordinates.
- I did not add an on-device BLE test that sends invalid `SET_ANCHOR` over the phone smoke app; native BLE tests cover the parser path and Android smoke covers transport health.

Promote to skill:
- Anchor point saving must be explicit and validated. Do not add default arguments or helpers that can arm Anchor as a side effect of storing coordinates.

### 2026-04-24 Stage 55: Anchor nudge/jog protocol simplification

Scope:
- Continue module-by-module refactor with `RuntimeAnchorNudge`, BLE command parsing, and Flutter command builders.
- Remove arbitrary nudge distance from the live protocol before alpha; keep reposition as a small explicit jog action.

External baseline:
- Minn Kota Advanced GPS Navigation exposes Jog as a fixed small move around the locked point, not an arbitrary-distance free-form command: <https://minnkota-help.johnsonoutdoors.com/hc/en-us/articles/23607178243991-Using-Advanced-GPS-Navigation-Features-and-Manual-2023-present>.
- Signal K Anchor Alarm treats moving the anchor location as a distinct action after setup: <https://demo.signalk.org/documentation/Guides/Anchor_Alarm.html>.
- Commercial GPS-anchor UX generally favors small explicit reposition steps because the operator can repeat them and observe the result instead of sending a large single correction.

Key outcomes:
- `NUDGE_DIR` now accepts only `NUDGE_DIR:<FWD|BACK|LEFT|RIGHT>`.
- `NUDGE_BRG` now accepts only `NUDGE_BRG:<bearingDeg>`.
- Firmware applies a fixed `1.5 m` jog step and rejects stale old comma-distance payloads.
- Nudge math is isolated in `RuntimeAnchorNudge`, validates source/target anchor coordinates, normalizes bearing/lon, and uses bounded `fmodf` math instead of loop-based wrapping.
- Flutter builders emit the same simplified protocol; no backward-compatibility shim was kept.
- BLE protocol docs, firmware reference, and external-pattern notes were updated.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_runtime_anchor_nudge -f test_ble_command_handler -f test_runtime_anchor_gate -f test_anchor_control` -> `45/45` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub test/ble_boatlock_test.dart` -> passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `197/197` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `695985` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `696352` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-nudge-fixed-v2-60s.log --json-out /tmp/boatlock-nudge-fixed-v2-60s.json` -> `PASS`, including RVC compass, display, EEPROM `ver=22`, BLE advertising, stepper, STOP, heading events, and GPS UART.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `Wire.cpp`, `i2cRead`, compass loss, or compass retry failure.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> first install attempt hit MIUI `USER_RESTRICTED`, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.

Self-review:
- This is a deliberate protocol break, which is acceptable before alpha and removes app/firmware distance validation duplication.
- Fixed `1.5 m` may need field tuning, but the protocol should not expose arbitrary distance until real-water tests prove a need.
- Android smoke proves transport, reconnect, and zero-throttle manual command roundtrip; it does not yet send an actual nudge command from the phone smoke app.
- During an earlier hardware check the RFC2217 port returned connection refused after flashing; I used `status.sh` and reran the canonical acceptance wrapper instead of accepting a manual workaround.

Promote to skill:
- Anchor jog/nudge should remain a fixed small step. Do not add arbitrary nudge distances back into firmware or Flutter without product evidence and tests.
- For MIUI phones, a first install-policy rejection followed by canonical retry success is a recorded retry, not a blocker; only terminal wrapper failure blocks acceptance.

### 2026-04-24 Stage 56: Motor output legacy PID removal

Scope:
- Continue module-by-module refactor with `MotorControl`, actuator settings, and EEPROM schema.
- Remove unused hidden self-adaptive PID code from the motor path; keep deterministic bounded anchor thrust behavior.

External baseline:
- ArduPilot Rover throttle tuning keeps PID tuning explicit and telemetry-based; it also treats acceleration limits and throttle slew as separate output constraints: <https://ardupilot.org/rover/docs/rover-tuning-throttle-and-speed.html>.
- ArduPilot Rover tuning process starts from calibration and manual proof before tuning controllers, not from hidden runtime gain changes: <https://ardupilot.org/rover/docs/rover-tuning-process.html>.
- PX4 actuator configuration treats slew rate as an explicit actuator output limit for hardware protection and stability: <https://docs.px4.io/main/en/config/actuators>.

Key outcomes:
- Removed unused `MotorControl::applyPID()`, PID filters, integral state, runtime gain adaptation, and PID auto-save code.
- Removed boot-time `motor.loadPIDfromSettings()` call.
- Removed obsolete settings keys `Kp`, `Ki`, `Kd`, `PidILim`, and `DistTh`.
- Bumped `Settings::VERSION` to `0x17`; docs now reflect schema `0x17`.
- Kept active anchor motor behavior on the already-tested bounded path: hold/deadband, max thrust, approach damping, anti-hunt windows, and thrust ramp limiting.
- Updated firmware/external-pattern/validation references so PID auto-persistence is not treated as a current write path.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_motor_control -f test_runtime_motion -f test_settings -f test_anchor_profiles -f test_ble_command_handler` -> `61/61` passed.
- `python3 tools/ci/check_config_schema_version.py` -> `config schema version OK: 0x17`.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `196/196` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `695541` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `695904` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-motor-no-pid-60s.log --json-out /tmp/boatlock-motor-no-pid-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, heading events, and GPS UART.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `Wire.cpp`, `i2cRead`, compass loss, or compass retry failure.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> first install attempt hit MIUI `USER_RESTRICTED`, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.

Self-review:
- This removes code rather than moving it. The active actuator path was already `driveAnchorAuto`; the removed PID path had no production callers and could only create future confusion or unsafe reactivation.
- Removing settings keys intentionally resets EEPROM to schema `0x17` on deployed benches; acceptable before alpha and proven on `nh02`.
- The remaining motor path still needs future water testing for real thrust tuning. Current proof is deterministic behavior, simulation, zero-throttle phone roundtrip, and boot/hardware health.

Promote to skill:
- Motor output should stay deterministic and bounded. Do not add hidden runtime gain adaptation or actuator-path settings persistence.
- Obsolete config fields should be removed before alpha; bump schema and prove migration on hardware instead of carrying dead compatibility fields.

### 2026-04-24 Stage 57: Stepper output stability pass

Scope:
- Continue module-by-module refactor with `StepperControl`.
- Keep rudder/stepper behavior deterministic and quiet on cancel/idle without widening the command surface.

External baseline:
- AccelStepper documents `disableOutputs()` as setting motor pins low to save power and `run()` as a non-blocking call that must be polled frequently: <https://www.airspayce.com/mikem/arduino/AccelStepper/classAccelStepper.html>.
- PX4 actuator guidance treats slew/trim/neutral behavior as explicit actuator configuration and uses disarmed/neutral output states to avoid unintended surface motion: <https://docs.px4.io/main/en/config/actuators>.
- Stepper motor guidance consistently flags idle holding current as a heat/power risk; when holding torque is not required, outputs should be disabled or current reduced.

Key outcomes:
- Replaced loop-based `normalize180()` wrapping with bounded `fmodf` math.
- `startManual(0)` now fails closed and does not enable stepper outputs.
- `cancelMove()` now starts the idle-release timer even if there was no pending target, so STOP/failsafe/cancel paths deterministically proceed toward coil release.
- Added native tests for bounded normalization, cancel-triggered idle release, and neutral manual input.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_stepper_control -f test_runtime_motion -f test_ble_command_handler` -> `46/46` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `198/198` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `695545` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `695904` bytes.
- First `./tools/hw/nh02/acceptance.sh ...` after flash hit RFC2217 `Connection refused`; `status.sh` showed the canonical service active and listening, then the same acceptance wrapper was rerun.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-stepper-stability-60s.log --json-out /tmp/boatlock-stepper-stability-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, heading events, and GPS UART.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `Wire.cpp`, `i2cRead`, compass loss, or compass retry failure.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.

Self-review:
- This is intentionally a narrow safety cleanup, not a new steering model.
- It does not prove powered mechanical rudder behavior under load; it proves parser/runtime/bench health and zero-throttle manual BLE path.
- The repeated RFC2217 refusal immediately after flash is a tooling race worth fixing in a future wrapper pass, but it was resolved through the canonical `status.sh` + same-wrapper retry path.

Promote to skill:
- Stepper code should use bounded angle math and fail closed on neutral/invalid manual input.
- Cancel/STOP/failsafe paths should deterministically enter the idle coil-release path, not rely on incidental later movement state.

### 2026-04-24 Stage 58: GPS UART watchdog rollover pass

Scope:
- Continue module-by-module refactor with `RuntimeGpsUart`.
- Keep GPS UART no-data/stale detection reliable over long uptime and early boot edge cases.

External baseline:
- Arduino serial guidance exposes `available()`/`read()` as the byte-availability/read primitives for receive loops: <https://docs.arduino.cc/language-reference/en/functions/communication/serial/available/> and <https://docs.arduino.cc/language-reference/en/functions/communication/serial/read/>.
- TinyGPS++ `FullExample` continuously feeds incoming serial bytes with `gps.encode(ss.read())` and separately checks for no GPS data: <https://github.com/mikalhart/TinyGPSPlus/blob/master/examples/FullExample/FullExample.ino>.
- Embedded timer/watchdog practice relies on unsigned elapsed-time subtraction rather than direct `now > then` comparisons so wraparound does not break timeouts.

Key outcomes:
- Replaced direct `nowMs > lastMs` checks with unsigned elapsed-time helpers.
- Stale detection now works even if the first GPS byte arrives at timestamp `0`.
- Added native tests for first-byte-at-zero stale restart and unsigned `millis()` rollover behavior.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_runtime_gps_uart -f test_runtime_gnss -f test_gnss_quality_gate -f test_anchor_supervisor` -> `28/28` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `200/200` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `695541` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `695904` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-gps-uart-rollover-60s.log --json-out /tmp/boatlock-gps-uart-rollover-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, heading events, and GPS UART data.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `Wire.cpp`, `i2cRead`, compass loss, compass retry failure, GPS UART stale, or GPS no-data warning.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 100` -> two MIUI `USER_RESTRICTED` install retries, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.

Self-review:
- This is a correctness hardening, not a behavior expansion.
- Native rollover tests run on host `unsigned long`, so they test language-defined unsigned wrap rather than ESP32-specific 32-bit width; still catches the broken direct-comparison pattern.
- Hardware acceptance proves normal GPS UART traffic still appears; it does not simulate a 49-day ESP32 uptime rollover on hardware.

Promote to skill:
- Runtime watchdogs must use unsigned elapsed-time checks and cover timestamp-zero/rollover edge cases in native tests.

### 2026-04-24 Stage 59: Compass event-age rollover pass

Scope:
- Continue module-by-module refactor with `BNO08xCompass`, `RuntimeCompassHealth`, and `RuntimeCompassRetry`.
- Remove timestamp-zero ambiguity and keep compass loss detection stable across long uptime.

External baseline:
- CEVA documents UART-RVC as a simplified BNO08X mode that transmits heading/sensor data at `100 Hz` and exposes `NRST` as a host- or board-driven reset line: <https://www.ceva-ip.com/wp-content/uploads/2019/10/BNO080_085-Datasheet.pdf>.
- Adafruit's BNO085 UART-RVC guide confirms the simple one-way UART wiring and `115200` serial demo path: <https://learn.adafruit.com/adafruit-9-dof-orientation-imu-fusion-breakout-bno085/uart-rvc-for-arduino>.
- Adafruit's pinout guide confirms `RST` is active low and `P1=Low/P0=High` selects UART-RVC: <https://learn.adafruit.com/adafruit-9-dof-orientation-imu-fusion-breakout-bno085/pinouts>.
- Arduino's non-blocking timing pattern uses `currentMillis - previousMillis >= interval`, which is the pattern we want for sensor watchdogs: <https://docs.arduino.cc/built-in-examples/digital/BlinkWithoutDelay/>.

Key outcomes:
- `BNO08xCompass` now separates "event seen" state from timestamp values, so a first heading frame at `millis()==0` is valid.
- Heading and any-event age calculations now use unsigned subtraction and survive `millis()` wrap.
- Hardware reset clears event-seen state, so recovery proof requires fresh RVC frames after reset.
- `RuntimeCompassHealth` first-event timeout now uses the same elapsed-time helper instead of direct timestamp comparisons.
- Added native tests for BNO08x UART-RVC frame age, timestamp-zero, reset clearing, and compass health/retry rollover behavior.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_bno08x_compass -f test_bno_rvc_frame -f test_runtime_compass_health -f test_runtime_compass_retry -f test_anchor_supervisor` -> `26/26` passed after fixing a test-time expectation around `begin()` delay.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `205/205` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `695561` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `./tools/hw/nh02/status.sh` -> target ESP32-S3 `98:88:E0:03:BA:5C` visible and RFC2217 service active.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `695920` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 180 --log-out /tmp/boatlock-compass-acceptance.log --json-out /tmp/boatlock-compass-acceptance.json` -> `PASS`, including EEPROM `ver=23`, RVC compass on `rx=12 baud=115200`, heading events, display, BLE advertising, stepper, STOP, and GPS UART data.
- Acceptance JSON had `errors=[]` and `warnings=[]`; log scan found no panic/assert/Guru, Arduino `[E]`, `COMPASS lost`, `COMPASS retry ready=0`, `Wire.cpp`, `i2cRead`, `error`, or `FAIL`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> two MIUI `USER_RESTRICTED` install retries, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> one MIUI `USER_RESTRICTED` install retry, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- This is a correctness hardening, not a protocol or actuation behavior expansion.
- Native tests cover language-defined unsigned wrap and timestamp-zero behavior; the hardware run proves normal RVC traffic and long capture health, but not an actual 49-day uptime rollover on ESP32.
- UART-RVC remains the correct KISS choice for the current bench: one data line, fixed heading stream, reset line for recovery, and no I2C bus failure mode on the compass path.

Promote to skill:
- Compass event freshness must use explicit event-valid flags; never use timestamp `0` as "no event".
- Compass health/retry timers must use unsigned elapsed-time checks and include rollover tests.

### 2026-04-24 Stage 60: Settings flash commit failure handling

Scope:
- Continue module-by-module refactor with `Settings` and EEPROM persistence.
- Keep the current blob+CRC format but make flash commit failure explicit and retryable.

External baseline:
- Arduino-ESP32 documents `Preferences` as the replacement for EEPROM and notes it stores retained data in ESP32 NVS: <https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html>.
- ESP-IDF NVS documents key-value storage, read/write status, and power-loss recovery expectations; it also states values are not durable until the commit path succeeds: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html>.
- ESP-IDF NVS internals include append-oriented storage and wear levelling, but still expose writes as fallible operations that callers must check.

Key outcomes:
- `Settings::save()` now returns `bool`.
- Clean no-op saves still return success without a flash commit.
- Failed `EEPROM.commit()` now logs `CONFIG_SAVE_FAILED` and leaves `dirty_` set so the next `save()` retries instead of silently dropping the change.
- Native EEPROM mock now models commit success/failure.
- Added a settings test that forces a failed commit, verifies failure is reported, then verifies a later retry commits.
- Docs and repo references now state that failed commits are not "saved" and must retain dirty state.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_settings -f test_anchor_control -f test_anchor_profiles -f test_runtime_motion -f test_ble_command_handler` -> `58/58` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `206/206` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `695589` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `./tools/hw/nh02/status.sh` -> target ESP32-S3 `98:88:E0:03:BA:5C` visible and RFC2217 service active.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `695952` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-settings-commit-60s.log --json-out /tmp/boatlock-settings-commit-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, heading events, and GPS UART data.
- Acceptance JSON had `errors=[]` and `warnings=[]`; log scan found no `CONFIG_SAVE_FAILED`, `CONFIG_CRC_FAIL`, panic/assert/Guru, Arduino `[E]`, compass loss, `Wire.cpp`, `i2cRead`, `error`, or `FAIL`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> one MIUI `USER_RESTRICTED` install retry, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- This is a narrow reliability fix. It does not change EEPROM layout, schema version, command surface, or boot migration semantics.
- Moving Settings from EEPROM emulation to `Preferences`/NVS may be a future cleanup, but doing it here would be a larger storage migration without a current failure. The current blob+CRC+dirty guard is simpler and already covered.
- Hardware acceptance proves normal boot/load and no real-flash commit failure in the boot path. Native tests cover the failure branch because inducing NVS commit failure safely on the bench is not part of the canonical hardware wrapper.

Promote to skill:
- Treat storage commits as fallible. Do not clear dirty state or log saved until the commit result is successful.

### 2026-04-24 Stage 61: GNSS source-transition baseline isolation

Scope:
- Continue module-by-module refactor with `RuntimeGnss`.
- Keep phone GPS fallback from contaminating hardware GNSS control state, and remove timestamp-zero ambiguity from speed/accel sampling.

External baseline:
- ArduPilot GPS glitch diagnostics tie autonomous position trust to quality evidence such as HDOP and satellite count, not just fresh coordinates: <https://ardupilot.org/rover/docs/common-diagnosing-problems-using-logs.html>.
- ArduPilot pre-arm safety checks treat GPS-related failures as enable blockers, which matches our hard `ANCHOR_ON` quality gate: <https://ardupilot.org/rover/docs/common-prearm-safety-checks.html>.
- OpenCPN Anchor Watch alarms on GPS loss, reinforcing that lost/stale GNSS is a first-class runtime condition rather than a silent fallback: <https://opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:manual_advanced:features:auto_anchor>.

Key outcomes:
- Hardware speed/acceleration sampling now uses an explicit `hasHardwareSpeedSample_` flag, so a valid first hardware sample at `millis()==0` is not discarded.
- Phone GPS fallback now resets hardware motion and filter baselines instead of seeding them with phone-derived coordinates or speed.
- `clearFix()` resets hardware motion and filter baselines, so hardware GNSS reacquisition starts from a fresh baseline instead of being jump-locked against stale coordinates.
- Angle normalization now uses bounded `fmodf` math instead of repeated loops.
- Added native tests for zero-timestamp acceleration, phone-to-hardware source transition, no-fix hardware reacquisition, and bounded angle wrapping.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_runtime_gnss -f test_gnss_quality_gate -f test_anchor_supervisor -f test_runtime_control_input_builder` -> `29/29` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `210/210` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `695609` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `./tools/hw/nh02/status.sh` -> target ESP32-S3 `98:88:E0:03:BA:5C` visible and RFC2217 service active.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `695968` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-gnss-source-60s.log --json-out /tmp/boatlock-gnss-source-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, heading events, and GPS UART data.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `CONFIG_SAVE_FAILED`, `CONFIG_CRC_FAIL`, GPS UART stale/no-data warning, compass loss, compass retry failure, `Wire.cpp`, `i2cRead`, `error`, or `FAIL`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> two MIUI `USER_RESTRICTED` install retries, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> one MIUI `USER_RESTRICTED` install retry, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- This is a source-state isolation fix, not a new GNSS estimator.
- Native tests cover the intended source-transition and timestamp-zero behavior. Hardware acceptance proves normal boot/GPS UART/BLE health, but it does not synthesize a real phone-fallback-to-hardware jump on the bench.
- Accepting the first hardware sample after no-fix or phone fallback is deliberate: without a trusted recent hardware baseline, rejecting it as a jump would leave the system stuck. Anchor control still requires the normal quality gate before enabling.

Promote to skill:
- Phone GPS fallback must never seed hardware GNSS filter, jump baseline, speed baseline, or acceleration baseline.
- GNSS motion freshness must use explicit sample-valid flags. Timestamp `0` is a valid sample time, not a sentinel.

### 2026-04-24 Stage 62: Motor manual-to-auto state isolation

Scope:
- Continue module-by-module refactor with `MotorControl` and the actuator output path.
- Remove hidden coupling between manual throttle and anchor-auto thrust ramp so mode transitions start quiet and bounded.

External baseline:
- ArduPilot Rover failsafe behavior maps loss/fault conditions to explicit Hold/quiet actions, and requires explicit operator action before returning to normal control: <https://ardupilot.org/rover/docs/rover-failsafes.html>.
- ArduPilot Rover Manual Mode still applies throttle slew, so manual control is not a bypass around actuator protection: <https://ardupilot.org/rover/docs/manual-mode.html>.
- ArduPilot Rover speed/throttle tuning treats throttle baseline and slew limiting as first-class controller behavior: <https://ardupilot.org/rover/docs/rover-tuning-throttle-and-speed.html>.
- PX4 actuator testing guidance requires disarmed/minimum output checks and prevents sudden motor movement until an explicit slider move: <https://docs.px4.io/main/en/config/actuators>.

Key outcomes:
- `driveManual()` no longer seeds `autoPwmRaw` or `autoRampUpdatedMs`; anchor-auto now starts from its own zero ramp after manual mode.
- Manual zero command now writes an idle output instead of toggling direction pins with PWM `0`.
- `stop()` and auto idle now set direction pins low with PWM `0`, reducing latent H-bridge state.
- Motor distance-rate timing now uses unsigned elapsed-time math and treats timestamp `0` as a valid sample.
- Shared reset/idle helpers removed repeated partial resets in the motor path.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_motor_control -f test_runtime_motion -f test_ble_command_handler` -> `51/51` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `214/214` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `695725` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `./tools/hw/nh02/status.sh` -> target ESP32-S3 `98:88:E0:03:BA:5C` visible and RFC2217 service active.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `696096` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-motor-state-60s.log --json-out /tmp/boatlock-motor-state-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, heading events, and GPS UART data.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `CONFIG_SAVE_FAILED`, `CONFIG_CRC_FAIL`, GPS UART stale/no-data warning, compass loss, compass retry failure, `Wire.cpp`, `i2cRead`, `error`, or `FAIL`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> one MIUI `USER_RESTRICTED` install retry, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- This is a safety behavior change: leaving manual mode can no longer carry a high manual PWM into anchor-auto's ramp state.
- The change is intentionally narrow and does not alter protocol, max thrust limits, deadband, or anchor enable rules.
- Bench acceptance and Android manual smoke prove the firmware/app path still runs and zero-throttle manual roundtrip still exits. They do not prove powered thrust under load; that still needs a mechanically safe motor test fixture before non-zero manual or anchor thrust acceptance.

Promote to skill:
- Manual and anchor-auto actuator state must stay isolated. Manual PWM, timestamps, or ramp state must not seed anchor-auto output.
- Any motor stop/zero path should drive PWM to zero and direction pins to a known idle state.

### 2026-04-24 Stage 63: Drift monitor threshold and freshness hardening

Scope:
- Continue module-by-module refactor with `DriftMonitor`.
- Keep drift alert/fail behavior deterministic when settings are invalid and keep drift-speed telemetry from staying stale after data gaps.

External baseline:
- OpenCPN Anchor Watch treats GPS loss as an alarm condition, not as a silent degraded state: <https://opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:manual_advanced:features:auto_anchor>.
- ArduPilot Rover fences declare breach and execute configured actions, with Rover falling back to HOLD for hard boundary failures: <https://ardupilot.org/rover/docs/common-geofencing-landing-page.html>.
- ArduPilot Cylindrical Fence guidance warns that fence safety depends on GPS/EKF health checks and should not run when position truth is unavailable: <https://ardupilot.org/rover/docs/common-ac2_simple_geofence.html>.

Key outcomes:
- `DriftMonitor` now sanitizes non-finite drift thresholds to the same safe defaults used by Settings (`6 m` alert, `12 m` fail).
- Fail threshold remains at least `alert + 0.5 m`, preserving ordered alert-before-fail behavior even with invalid input.
- Drift speed timing now uses an explicit unsigned elapsed-time helper.
- Long sample gaps clear drift speed to `0` instead of keeping stale speed telemetry alive.
- Added native tests for non-finite thresholds, `millis()` rollover, and stale speed reset.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_drift_monitor -f test_anchor_supervisor -f test_runtime_motion` -> first run caught a test interval below the rate filter minimum; after correcting the test to a valid interval, `25/25` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `217/217` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `695805` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `./tools/hw/nh02/status.sh` -> target ESP32-S3 `98:88:E0:03:BA:5C` visible and RFC2217 service active.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `696176` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-drift-monitor-60s.log --json-out /tmp/boatlock-drift-monitor-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, heading events, and GPS UART data.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `CONFIG_SAVE_FAILED`, `CONFIG_CRC_FAIL`, GPS UART stale/no-data warning, compass loss, compass retry failure, `Wire.cpp`, `i2cRead`, `error`, or `FAIL`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- This is defensive runtime hardening, not new drift behavior for valid settings.
- Threshold fallback is deliberately conservative and mirrors current persisted defaults instead of inventing new product thresholds.
- Hardware acceptance proves normal boot/sensor/BLE health. It does not inject a live drift breach on the bench; native tests cover the threshold and freshness branches.

Promote to skill:
- Drift/containment thresholds must sanitize non-finite input before comparisons.
- Drift-speed telemetry must not survive long data gaps as stale motion evidence.

### 2026-04-24 Stage 64: Manual control source and deadman hardening

Scope:
- Continue module-by-module refactor with `ManualControl`.
- Keep phone and future BLE remote/joystick manual control behind explicit source validation and a rollover-safe deadman.

External baseline:
- ArduPilot Rover Manual Mode keeps manual control direct but still applies throttle slew, so manual input is not a bypass around actuator protection: <https://ardupilot.org/rover/docs/manual-mode.html>.
- ArduPilot Rover failsafes require explicit operator action to retake manual control after link recovery: <https://ardupilot.org/rover/docs/rover-failsafes.html>.
- PX4 manual control loss failsafe is based on time since the last valid setpoint and warns that timeout must stay short because the vehicle otherwise continues on the last stick position: <https://docs.px4.io/main/en/config/safety.html>.

Key outcomes:
- `ManualControl::apply()` now rejects `ManualControlSource::NONE`; only `BLE_PHONE` and `BLE_REMOTE` can activate manual mode.
- Rejected manual packets do not refresh or replace an existing command, so malformed input cannot extend a live deadman lease.
- Added native coverage for zero timestamp, unsigned `millis()` rollover, source rejection, and invalid-command non-refresh behavior.
- No protocol compatibility shim was added; `MANUAL_SET` remains the one atomic manual command surface.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_manual_control -f test_runtime_control -f test_runtime_motion -f test_ble_command_handler` -> `53/53` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `220/220` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `695805` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `./tools/hw/nh02/status.sh` -> target ESP32-S3 `98:88:E0:03:BA:5C` visible and RFC2217 service active.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `696176` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-manual-control-60s.log --json-out /tmp/boatlock-manual-control-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, GPS UART data, and heading events.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `CONFIG_SAVE_FAILED`, `CONFIG_CRC_FAIL`, GPS UART stale/no-data warning, compass loss, compass retry failure, `Wire.cpp`, `i2cRead`, `error`, or `FAIL`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- This is a small contract hardening, not a new multi-controller arbitration model.
- Current policy remains "latest valid manual source wins"; if phone and BLE remote are both active later, we may need explicit priority/lease ownership, but that should be designed with the actual remote protocol.
- Hardware smoke proves zero-throttle manual roundtrip only; non-zero manual actuation still needs a mechanically safe powered fixture.

Promote to skill:
- Manual control activation must require an explicit non-NONE source.
- Invalid manual packets must not refresh the deadman lease.
- Manual deadman tests must cover timestamp zero and unsigned rollover.

### 2026-04-24 Stage 65: Anchor supervisor fail-closed timeout floors

Scope:
- Continue module-by-module refactor with `AnchorSupervisor`.
- Make the core failsafe arbiter fail closed even if a caller passes zero timeout values directly instead of using the runtime settings builder.

External baseline:
- ArduPilot Rover failsafes map link/GCS loss to explicit actions such as Hold and require operator action to retake manual control after recovery: <https://ardupilot.org/rover/docs/rover-failsafes.html>.
- ArduPilot Rover geofence behavior treats boundary breach as a failsafe action trigger, not as an advisory state: <https://ardupilot.org/rover/docs/common-geofencing-landing-page.html>.
- PX4 manual-control and data-link failsafes are timeout-based, and PX4 warns that old setpoints remain active until the timeout triggers, so the timeout must stay bounded: <https://docs.px4.io/main/en/config/safety.html>.

Key outcomes:
- Added core minimum floors for comm timeout, control-loop timeout, sensor timeout, and GPS-weak grace.
- A zero timeout in `AnchorSupervisor::Config` no longer disables the corresponding failsafe.
- `controlActivitySeen` in `AnchorSupervisor::Input` now refreshes the supervisor deadline directly, matching the field's contract.
- Timeout comparisons now use a single unsigned elapsed-time helper.
- Added native tests for zero-timeout floors, exact comm-deadline behavior, and input-side control activity refresh.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_anchor_supervisor -f test_runtime_supervisor_policy -f test_runtime_motion -f test_ble_command_handler` -> `59/59` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `225/225` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `695801` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `./tools/hw/nh02/status.sh` -> target ESP32-S3 `98:88:E0:03:BA:5C` visible and RFC2217 service active.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `696160` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-supervisor-failsafe-60s.log --json-out /tmp/boatlock-supervisor-failsafe-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, heading events, and GPS UART data.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `CONFIG_SAVE_FAILED`, `CONFIG_CRC_FAIL`, GPS UART stale/no-data warning, compass loss, compass retry failure, `Wire.cpp`, `i2cRead`, `error`, or `FAIL`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> one MIUI `USER_RESTRICTED` install retry, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> one MIUI `USER_RESTRICTED` install retry, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- This deliberately removes "zero disables timeout" semantics from the supervisor core. That is consistent with the current pre-alpha/no-compatibility policy and with the product safety goal.
- Runtime behavior through `buildRuntimeSupervisorConfig()` is effectively unchanged for normal settings because it already clamps to the same minimums.
- Hardware acceptance proves boot/sensor/BLE health after the change. The exact failsafe-floor branches are native-tested, not injected on the live bench.

Promote to skill:
- Core failsafe modules must not rely only on upstream settings clamps; apply local fail-closed floors before timeout comparisons.
- Input fields that claim to report current control activity must refresh the relevant deadline in the core module, or be removed.

### 2026-04-24 Stage 66: Runtime control input numeric gate

Scope:
- Continue module-by-module refactor with `RuntimeControlInputBuilder`.
- Keep mode/control glue simple and make invalid numeric sensor/controller values unavailable before they reach motion code.

External baseline:
- ArduPilot Rover failsafes map loss/fault conditions to explicit safe actions instead of continuing on ambiguous inputs: <https://ardupilot.org/rover/docs/rover-failsafes.html>.
- ArduPilot Rover Hold mode is the explicit quiet state used to stop movement while keeping the vehicle ready for later operator action: <https://ardupilot.org/rover/docs/hold-mode.html>.
- PX4 safety guidance treats manual-control/data-link loss as timeout-based invalid input and warns against continuing stale setpoints: <https://docs.px4.io/main/en/config/safety.html>.

Key outcomes:
- Non-finite heading now clears `hasHeading`, zeroes exported heading, and prevents `diffDeg` calculation.
- Non-finite bearing now clears `hasBearing`, zeroes exported bearing, and prevents `diffDeg` calculation.
- Non-finite or negative distance now exports as `0` before reaching runtime motion input.
- Mode precedence remains unchanged by design: `SIM > MANUAL > ANCHOR > HOLD > IDLE`.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_runtime_control_input_builder -f test_runtime_control -f test_runtime_motion -f test_anchor_supervisor` -> `36/36` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `228/228` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `695953` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `./tools/hw/nh02/status.sh` -> target ESP32-S3 `98:88:E0:03:BA:5C` visible and RFC2217 service active.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `696320` bytes.
- First `./tools/hw/nh02/acceptance.sh ...` attempt hit RFC2217 `Connection refused`; canonical `status.sh` showed the target path healthy, then the same acceptance wrapper was rerun instead of substituting manual logs.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-control-input-60s.log --json-out /tmp/boatlock-control-input-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, heading events, and GPS UART data.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `CONFIG_SAVE_FAILED`, `CONFIG_CRC_FAIL`, GPS UART stale/no-data warning, compass loss, compass retry failure, `Wire.cpp`, `i2cRead`, `error`, or `FAIL`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> one MIUI `USER_RESTRICTED` install retry, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> one MIUI `USER_RESTRICTED` install retry, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- This is a boundary hardening change, not a controller behavior rewrite.
- The guard prevents bad numeric state from reaching motion code; supervisor still owns runtime `INTERNAL_ERROR_NAN` containment if invalid values appear deeper in the loop.
- Hardware acceptance proves boot/sensor/BLE health after the change. The invalid-number paths are covered by native tests rather than injected into the live bench.

Promote to skill:
- Mode/control-input builders must validate numeric availability at the boundary.
- Non-finite sensor or bearing values should become unavailable and zeroed before motion code sees them.

### 2026-04-24 Stage 67: Settings RAM defaults and type normalization

Scope:
- Continue module-by-module refactor with `Settings`.
- Reduce storage risk by making settings safe before `load()`/`reset()` and keeping no-op flash saves as real no-ops.

External baseline:
- Arduino-ESP32 documents `Preferences` as the ESP32 replacement for Arduino EEPROM and recommends it for retained small values: <https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html>.
- ESP-IDF NVS uses key-value storage with namespaces, wear levelling by design, and updates become durable only after commit: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html>.
- ESP-IDF storage docs treat flash writes as bounded resources; write paths should avoid unnecessary commits even when the backend wear-levels: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/wear-levelling.html>.

Key outcomes:
- `Settings` now initializes RAM defaults and key map in its constructor, so accidental use before `load()`/`reset()` reads safe defaults instead of uninitialized memory.
- The constructor stays flash-clean: `save()` on a fresh default object does not call `EEPROM.commit()`.
- Default loading is centralized through one helper used by constructor, `reset()`, and `load()`.
- `set()` now normalizes integer-style settings before assigning/dirtying, matching `setStrict()` and load-time normalization.
- EEPROM image layout and `Settings::VERSION` are unchanged.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_settings -f test_anchor_control -f test_anchor_profiles -f test_runtime_supervisor_policy -f test_ble_command_handler -f test_runtime_motion -f test_runtime_gnss` -> `73/73` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_settings` after self-review test rename -> `16/16` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `231/231` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `695985` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `./tools/hw/nh02/status.sh` -> target ESP32-S3 `98:88:E0:03:BA:5C` visible and RFC2217 service active.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `696352` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-settings-60s.log --json-out /tmp/boatlock-settings-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, heading events, and GPS UART data.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `CONFIG_SAVE_FAILED`, `CONFIG_CRC_FAIL`, GPS UART stale/no-data warning, compass loss, compass retry failure, `Wire.cpp`, `i2cRead`, `error`, or `FAIL`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- This does not migrate storage backend from EEPROM emulation to Preferences/NVS yet; doing that would be a separate storage-format change with explicit migration and acceptance plan.
- The EEPROM image format remains stable, so no version bump was needed.
- The only runtime behavior change is safer normalization for integer-style `set()` values and safe defaults before load.
- Hardware acceptance proves boot load still reads EEPROM `ver=23` cleanly and no save/CRC error appears in logs.

Promote to skill:
- `Settings` must be safe immediately after construction: RAM defaults and key map ready, no flash write from constructor.
- Settings write paths must normalize values by declared type before dirtying/persisting.
- A clean settings object must never commit flash on `save()`.

### 2026-04-24 Stage 68: BLE connected advertising watchdog

Scope:
- Continue module-by-module refactor with `BleAdvertisingWatchdog` and the production `BLEBoatLock` callback glue.
- Prepare the BLE transport for phone + external remote/joystick discovery without adding a second control protocol or compatibility shim.

External baseline:
- Silicon Labs BLE multi-central guidance states that advertising stops when a connection opens and must be restarted after the opened event to allow more centrals: <https://docs.silabs.com/bluetooth/6.0.0/bluetooth-fundamentals-connections/multi-central-topology>.
- Silicon Labs BLE role docs define the phone as central and the ESP32 as peripheral; centrals initiate connections to advertising peripherals: <https://docs.silabs.com/bluetooth/3.2/bluetooth-general-connections/>.
- Bluetooth's LE multi-central article states that a peripheral may keep a connection while also advertising and may maintain more than one central connection: <https://www.bluetooth.com/zh-cn/blog/how-one-wearable-can-connect-with-multiple-smartphones-or-tablets-simultaneously/>.

Key outcomes:
- `BleAdvertisingWatchdog` now has an explicit `START_CONNECTED_ADVERTISING` action when clients exist but advertising stopped.
- `BLEBoatLock::maintainAdvertising()` restarts advertising without clearing queues/subscription state while at least one client remains connected.
- `onConnect()` only clears stream/notify state for the first client, so a second central connect does not drop the active client's telemetry stream.
- `onDisconnect()` only clears stream/notify state when the last client disconnects; if another client remains, status stays `CONNECTED`.
- Single-client reconnect behavior remains covered by the existing Android reconnect and ESP-reset smokes.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_ble_advertising_watchdog -f test_ble_command_handler -f test_runtime_ble_live_frame` -> `41/41` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `232/232` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `696145` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `./tools/hw/nh02/status.sh` -> target ESP32-S3 `98:88:E0:03:BA:5C` visible and RFC2217 service active.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `696512` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-ble-advertising-60s.log --json-out /tmp/boatlock-ble-advertising-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, heading events, and GPS UART data.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `CONFIG_SAVE_FAILED`, `CONFIG_CRC_FAIL`, GPS UART stale/no-data warning, compass loss, compass retry failure, `Wire.cpp`, `i2cRead`, `error`, or `FAIL`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- This is transport availability hardening, not multi-controller arbitration. Manual-control ownership/priority for simultaneous phone + remote still needs a separate product rule before a real remote is added.
- The hardware bench currently has one Android phone, so connected advertising for a second central is unit-tested and production-compiled, but not proven with a second central device.
- The existing app reconnect paths still pass after the advertising policy change, including ESP reset recovery.

Promote to skill:
- BLE advertising should be kept/restarted while connected when future phone + remote discovery is required.
- Do not clear active stream/notify state on a second central connect or on disconnect while another central remains connected.
- Multi-central support in BLE transport does not replace explicit control-source arbitration in `ManualControl`.

### 2026-04-24 Stage 69: Anchor diagnostics timer hardening

Scope:
- Continue module-by-module refactor with `AnchorDiagnostics`.
- Keep diagnostics deterministic around boot-time timestamps and invalid actuator limits without changing the event API.

External baseline:
- ArduPilot Rover failsafes require a fault condition to persist for a timeout before action, and recovery does not silently restore the old mode: <https://ardupilot.org/rover/docs/rover-failsafes.html>.
- ArduPilot onboard logs keep typed error/event subsystems instead of ambiguous free text only: <https://ardupilot.org/rover/docs/logmessages.html>.
- PX4 safety guidance treats failsafe conditions as explicit vehicle-safety states with configured actions, not as convenience telemetry: <https://docs.px4.io/main/en/config/safety.html>.

Key outcomes:
- `AnchorDiagnostics` no longer uses timestamp `0` as a sentinel for sensor-bad or saturation timing.
- Sensor timeout and control saturation now work correctly when the first bad sample is observed at `millis()==0`.
- Saturation diagnostics ignore invalid non-positive motor limits instead of treating `0 >= 0` as saturated forever.
- Added native coverage for zero-timestamp sensor timeout, zero-timestamp saturation, and invalid motor limit handling.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_anchor_diagnostics -f test_anchor_supervisor -f test_runtime_motion -f test_motor_control` -> `41/41` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `235/235` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `696185` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `./tools/hw/nh02/status.sh` -> target ESP32-S3 `98:88:E0:03:BA:5C` visible and RFC2217 service active.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `696544` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-anchor-diagnostics-60s.log --json-out /tmp/boatlock-anchor-diagnostics-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, GPS UART data, and heading events.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `CONFIG_SAVE_FAILED`, `CONFIG_CRC_FAIL`, GPS UART stale/no-data warning, compass loss, compass retry failure, `Wire.cpp`, `i2cRead`, `error`, or `FAIL`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> one MIUI `USER_RESTRICTED` install retry, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> one MIUI `USER_RESTRICTED` install retry, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- This is diagnostic correctness; it does not change control output, failsafe action, BLE protocol, or UI.
- The module now follows the same explicit-seen-flag pattern already used in compass/GNSS watchdogs.
- Hardware acceptance proves no boot/runtime regressions and clean sensor/BLE startup. The exact zero-timestamp diagnostic branches are native-tested, not live-injected on the bench.

Promote to skill:
- Diagnostic timers must track event presence separately from timestamp values; `0` is a valid `millis()` sample.
- Diagnostic saturation checks must reject invalid non-positive actuator limits before comparing output against the limit.

### 2026-04-24 Stage 70: Anchor control loop timer hardening

Scope:
- Continue module-by-module refactor with `AnchorControlLoop`.
- Keep the HIL/simulation controller deterministic around boot-time timestamps without changing the public controller API.
- Keep simulation in `main` as mandatory regression infrastructure, not as extra product scope.

External baseline:
- ArduPilot Rover failsafes map persistent fault conditions to explicit safe actions: <https://ardupilot.org/rover/docs/rover-failsafes.html>.
- ArduPilot log messages keep typed error/event records so failures are inspectable after the fact: <https://ardupilot.org/rover/docs/logmessages.html>.
- PX4 safety guidance treats failsafe conditions as explicit safety states/actions rather than permissive runtime convenience: <https://docs.px4.io/main/en/config/safety.html>.

Key outcomes:
- `AnchorControlLoop` no longer treats timestamp `0` as "timer not started".
- Added explicit seen flags for GNSS good/bad windows, sensor timeout, control-loop tick timing, thrust ramp timing, and max continuous thrust timing.
- GNSS jump/speed baseline timing now uses unsigned elapsed-time math.
- Added dedicated native coverage for zero-timestamp entry, sensor timeout, control-loop timeout, and bad-GNSS exit.

Validation:
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native -f test_anchor_control_loop -f test_hil_sim -f test_runtime_motion -f test_anchor_supervisor` -> `39/39` passed.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio platformio test -e native` -> `239/239` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && env PIO_HOME_DIR=/tmp/boatlock-pio pio run -e esp32s3` -> success, flash size `696273` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `./tools/hw/nh02/status.sh` -> target ESP32-S3 `98:88:E0:03:BA:5C` visible and RFC2217 service active.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `696640` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-anchor-control-loop-60s.log --json-out /tmp/boatlock-anchor-control-loop-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, compass heading events, and GPS UART data.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `CONFIG_SAVE_FAILED`, `CONFIG_CRC_FAIL`, GPS UART stale/no-data warning, compass loss, compass retry failure, `Wire.cpp`, `i2cRead`, `error`, or `FAIL`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- This is internal timing correctness for HIL/simulation, not a product command-surface expansion.
- Scenario tests still pass, but direct native tests now cover timer boundary branches that scenario-level tests can miss.
- Hardware acceptance proves production boot/sensor/BLE health after the change; injected HIL failure branches are native/offline-tested rather than live-injected on the bench.

Promote to skill:
- HIL/control-loop timers must use explicit seen flags and unsigned elapsed-time math; timestamp `0` is a valid sample.
- HIL core modules need direct unit tests for boundary timing behavior; scenario-level tests alone are not enough.

### 2026-04-24 Stage 71: Anchor persistence save/load hardening

Scope:
- Continue module-by-module refactor with `AnchorControl`.
- Keep anchor-point persistence strict: a point is not considered saved if flash commit fails.
- Keep `SET_ANCHOR` as save-only; arming remains the explicit `ANCHOR_ON` path.

External baseline:
- OpenCPN Auto Anchor Mark only creates anchor marks under conservative conditions and avoids spurious persisted marks: <https://opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:manual_advanced:features:anchor_auto>.
- OpenCPN Anchor Watch requires GPS context and proximity to the mark before enabling watch behavior: <https://opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:manual_advanced:features:auto_anchor>.
- ESP-IDF NVS requires a commit for changes to be guaranteed persistent, and commit returns an explicit success/error result: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html>.
- ESP-IDF wear levelling docs treat flash writes as bounded operations with power-loss mode tradeoffs: <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/wear-levelling.html>.

Key outcomes:
- `AnchorControl::saveAnchor()` now checks `settings.save()` and returns false on failed commit.
- On settings write or save failure, `AnchorControl` restores previous settings values and keeps the live anchor point unchanged.
- `loadAnchor()` normalizes persisted heading and clears invalid/default anchor pairs instead of leaving stale live anchor coordinates.
- Added native coverage for failed commit rollback and persisted load normalization/clear behavior.

Validation:
- `cd boatlock && platformio test -e native -f test_anchor_control -f test_settings -f test_ble_command_handler` -> `54/54` passed.
- `cd boatlock && platformio test -e native` -> `241/241` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `696645` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `./tools/hw/nh02/status.sh` -> target ESP32-S3 `98:88:E0:03:BA:5C` visible and RFC2217 service active.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `697008` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-anchor-control-60s.log --json-out /tmp/boatlock-anchor-control-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, heading events, and GPS UART data.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `CONFIG_SAVE_FAILED`, `CONFIG_CRC_FAIL`, GPS UART stale/no-data warning, compass loss, compass retry failure, `Wire.cpp`, `i2cRead`, `error`, or `FAIL`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> one MIUI `USER_RESTRICTED` install retry, canonical retry succeeded, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- This is a persistence correctness change, not a control-mode or BLE command expansion.
- Failed flash commit now leaves both live anchor RAM and settings RAM on the previous safe point; a later retry can still save the previous values because `Settings` keeps dirty state after failed commit.
- Hardware acceptance proves boot storage load remains clean and no config CRC/save errors appear. Commit-failure behavior is native-tested because it cannot be safely injected on the live bench.

Promote to skill:
- Anchor-point persistence must be all-or-reject at the runtime boundary; do not log or return saved when the settings commit fails.
- Loaded anchor coordinates must be validated as a pair and invalid/default coordinates must clear the live anchor point.

### 2026-04-24 Stage 72: BLE live-frame scaling hardening

Scope:
- Continue module-by-module refactor with `RuntimeBleLiveFrame`.
- Keep the fixed 70-byte binary v2 live telemetry frame unchanged.
- Harden numeric scaling at the firmware BLE boundary so malformed finite telemetry values clamp before integer conversion.

External baseline:
- Bluetooth Core GATT defines characteristic values as profile/application data and supports read/notify delivery of characteristic values: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.
- Android `BluetoothGattCharacteristic` integer helpers exist for characteristic values but are deprecated; the app already uses explicit byte decoding, so firmware must keep its byte layout deterministic: <https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic>.
- Android `BluetoothGatt` is the client runtime path for BLE characteristic read/notify operations: <https://developer.android.com/reference/android/bluetooth/BluetoothGatt>.

Key outcomes:
- `runtimeBleScaleSigned()` now clamps the scaled `double` before `lround()` and integer cast.
- `runtimeBleScaleUnsigned()` now clamps large scaled values before `lround()` and integer cast.
- Non-finite input still maps to neutral `0`, while finite out-of-range input maps to the field's min/max.
- Added native coverage for extreme finite signed/unsigned values and `NaN` handling.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_live_frame -f test_ble_command_handler` -> `36/36` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub test/ble_boatlock_test.dart` -> passed.
- `cd boatlock && platformio test -e native` -> `242/242` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `696869` bytes.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter build apk --debug --no-pub` -> success.
- `./tools/hw/nh02/status.sh` -> target ESP32-S3 `98:88:E0:03:BA:5C` visible and RFC2217 service active.
- `./tools/hw/nh02/flash.sh` -> rebuilt and flashed ESP32-S3 `98:88:e0:03:ba:5c`; app image write `697232` bytes.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-ble-live-frame-60s.log --json-out /tmp/boatlock-ble-live-frame-60s.json` -> `PASS`, including EEPROM `ver=23`, RVC compass, display, BLE advertising, stepper, STOP, GPS UART data, and heading events.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `CONFIG_SAVE_FAILED`, `CONFIG_CRC_FAIL`, GPS UART stale/no-data warning, compass loss, compass retry failure, `Wire.cpp`, `i2cRead`, `error`, or `FAIL`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- This does not change the BLE frame version, layout, fields, or Flutter decoder.
- The fix is boundary hardening against undefined/implementation-dependent numeric conversion, not a protocol feature.
- Hardware and Android BLE validation were run before the new batch cadence was requested; from the next modules, hardware is batched every three modules unless risk requires earlier acceptance.

Promote to skill:
- BLE binary encoders must clamp in floating-point space before integer rounding/casting.
- Fixed binary frame changes need firmware encoder, Flutter decoder, protocol docs, and tests together; pure range-hardening inside existing fields does not require a frame-version bump.

### 2026-04-24 Workflow correction: hardware cadence

Decision:
- User changed the default cadence from hardware acceptance after every module to hardware acceptance after every three modules.
- For each module, still run external baseline, local validation, worklog/self-review, commit, and push.
- Run `nh02` flash/acceptance/log scan/Android BLE smokes after the third module in a batch, unless the change touches hardware drivers, pinout, deploy/debug wrappers, actuator safety, BLE reconnect/install behavior, or another path where local tests cannot bound the risk.

Promote to skill:
- Batch low-risk refactors in groups of three for hardware acceptance.

### 2026-04-24 Stage 73: BLE telemetry snapshot hardening

Scope:
- Continue module-by-module refactor with `RuntimeBleParams`.
- Keep the live telemetry frame contract unchanged while hardening the typed snapshot that feeds it.
- This is module `2/3` in the current low-risk batch; hardware acceptance is deferred until module `3/3` unless risk changes.

External baseline:
- Signal K treats position as an object-valued field because latitude and longitude only make sense as a pair: <https://signalk.org/specification/1.7.0/doc/data_model.html>.
- Signal K invalid/missing data is represented explicitly instead of sending a misleading partial value: <https://signalk.org/specification/1.7.0/doc/data_model.html>.
- Bluetooth Core GATT treats characteristic values as application/profile data; deterministic encoding remains the application responsibility: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.

Key outcomes:
- Added `setRuntimeBleAnchorTelemetry()` to validate anchor latitude/longitude as a pair before publishing them.
- Invalid/default anchor coordinates now clear `anchorLat`, `anchorLon`, and `anchorHeading` in the BLE snapshot.
- Non-finite persisted anchor heading now publishes as neutral `0`, while valid heading is normalized.
- `compassReady()` is sampled once per telemetry snapshot so compass fields come from a consistent readiness state.
- Added native coverage for valid anchor telemetry, invalid anchor-pair clearing, and non-finite heading clearing.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_params -f test_runtime_ble_live_frame -f test_runtime_status` -> `9/9` passed.
- First targeted run failed because Unity double precision assertions were disabled; tests were corrected to use float-within assertions, then the same targeted suite passed.
- `cd boatlock && platformio test -e native` -> `245/245` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `696785` bytes.
- Hardware acceptance deferred by the new three-module cadence; this is module `2/3`.

Self-review:
- This does not change the BLE frame layout, Flutter decoder, command surface, or runtime control behavior.
- The new native suite required small BLE/NimBLE mocks because `RuntimeBleParams.h` previously had no direct unit coverage.
- The snapshot builder now follows the same pair-validation rule as `AnchorControl` load/save and the UI map logic.

Promote to skill:
- BLE telemetry snapshots must not publish partial coordinate pairs; invalid position-like pairs clear the whole pair.
- Snapshot builders should sample volatile readiness predicates once per frame, not once per field.

### 2026-04-24 Stage 74: Telemetry cadence timer hardening

Scope:
- Continue module-by-module refactor with `RuntimeTelemetryCadence`.
- This is module `3/3` in the current low-risk batch; `nh02` hardware and Android BLE acceptance is due after this module is committed and pushed.
- Keep configured UI, BLE, and compass polling intervals unchanged.

External baseline:
- Arduino's Blink Without Delay pattern uses non-blocking elapsed-time checks instead of blocking delays: <https://docs.arduino.cc/built-in-examples/digital/BlinkWithoutDelay/>.
- Bluetooth Core GATT treats characteristic values and notifications as application/profile data, so BoatLock owns the telemetry cadence policy above BLE transport: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.

Key outcomes:
- Replaced three duplicated elapsed-time checks with one shared `shouldRun()` helper.
- Preserved existing non-zero interval behavior for UI refresh, BLE notify, and compass polling.
- Made `interval=0` behavior explicit: every call runs and updates the matching timestamp.
- Added native coverage for zero interval and unsigned `millis()` rollover.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_telemetry_cadence -f test_runtime_ble_params -f test_runtime_ble_live_frame` -> `12/12` passed.
- `cd boatlock && platformio test -e native` -> `247/247` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> `29/29` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `696761` bytes.

Self-review:
- This is a KISS cleanup with direct boundary tests; it does not change BLE protocol, telemetry frame layout, or configured runtime intervals.
- The only newly documented edge behavior is `interval=0`; current production settings do not rely on zero, but tests now prevent accidental ambiguity.
- Hardware acceptance is intentionally batched after this third module per the current cadence.

Promote to skill:
- Runtime cadence timers must share unsigned elapsed-time logic and include direct tests for zero interval and rollover.

### 2026-04-24 Stage 75: Batch hardware acceptance after modules 1-3

Scope:
- Run the scheduled `nh02` hardware and Android BLE acceptance after the three-module low-risk batch:
  - `RuntimeBleLiveFrame`
  - `RuntimeBleParams`
  - `RuntimeTelemetryCadence`
- Validate the just-pushed `main` firmware and Android smoke APK through canonical wrappers only.

Validation:
- `./tools/hw/nh02/status.sh` -> ESP32-S3 `98:88:E0:03:BA:5C` visible, RFC2217 service enabled and active.
- `./tools/hw/nh02/flash.sh` -> build success, flash success, app image write `697120` bytes, hard reset via RTS.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-batch-telemetry-cadence-60s.log --json-out /tmp/boatlock-batch-telemetry-cadence-60s.json` -> `PASS`, including EEPROM `ver=23`, `BNO08x-RVC rx=12 baud=115200`, heading events, display, BLE advertising, stepper, STOP button, and GPS UART data.
- Acceptance log scan found no panic/assert/Guru, Arduino `[E]`, `CONFIG_SAVE_FAILED`, `CONFIG_CRC_FAIL`, GPS UART stale/no-data warning, compass loss, compass retry failure, `Wire.cpp`, `i2cRead`, error, or fail markers.
- `./tools/hw/nh02/android-status.sh` -> Xiaomi `220333QNY` attached as adb `device`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> first install attempt hit MIUI `USER_RESTRICTED`, canonical retry reached `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- This acceptance covered the flashed ESP32 boot path, sensor readiness, BLE advertising, Android install/update, telemetry receive, manual zero-throttle roundtrip, phone reconnect, and ESP32 reboot recovery.
- The manual run's first MIUI install prompt did not block acceptance because the canonical retry succeeded and the wrapper returned terminal pass.
- No new durable workflow lesson emerged beyond the existing MIUI retry rule already captured in the hardware acceptance skill.

### 2026-04-24 Stage 76: Runtime status severity split

Scope:
- Start the next low-risk three-module batch with `RuntimeStatus`.
- Keep `statusReasons` tokens and BLE frame layout unchanged.
- Stop informational operator acknowledgements from becoming health warnings.

External baseline:
- Signal K notifications model separates states such as `alert`, `alarm`, and `emergency`, and devices/servers monitor and raise only real alarm conditions: <https://signalk.org/specification/1.7.0/doc/notifications.html>.
- Signal K data model keeps semantic values structured so consumers do not infer severity from unrelated fields: <https://signalk.org/specification/1.7.0/doc/data_model.html>.

Key outcomes:
- Added explicit `runtimeStatusReasonIsInformational()` classification.
- `NUDGE_OK` remains visible in `statusReasons`, but no longer elevates `status` from `OK` to `WARN` by itself.
- Unknown safety reasons still elevate to `WARN` by default so new unclassified runtime notes fail visible, not hidden.
- Added native coverage for informational and unknown safety reason summary behavior.
- Added Android smoke mode `status` for phone-visible status/severity changes. It sends safe `STOP`, verifies `ALERT/STOP_CMD`, then clears through zero-throttle `MANUAL_SET` + `MANUAL_OFF`.
- Updated the module cadence rule: every module must explicitly decide phone-smoke coverage; phone-visible behavior changes get a module-specific Android smoke path.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_status -f test_runtime_ble_live_frame -f test_runtime_ble_params` -> `11/11` passed.
- `cd boatlock && platformio test -e native` -> `249/249` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub test/ble_smoke_logic_test.dart test/ble_live_frame_test.dart` -> passed.
- `./tools/android/build-smoke-apk.sh --mode status` -> first sandbox run failed with Gradle `Operation not permitted`; reran host-side and built `app-debug.apk` successfully.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> full suite passed (`30/30`).
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `696841` bytes.
- `./tools/hw/nh02/flash.sh` -> build success, flash success, app image write `697200` bytes, hard reset via RTS.
- `./tools/hw/nh02/android-run-smoke.sh --status --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"status_stop_alert_roundtrip",...}`.
- Full `nh02` acceptance remains deferred by the three-module cadence; this is module `1/3`, but module-specific phone smoke ran because status semantics are phone-visible.

Self-review:
- This is a status semantics fix, not a transport or command change.
- It reduces false operator warning state after successful nudge without hiding unknown future reasons.
- The new phone smoke uses only safe STOP and zero-throttle manual recovery; it does not prove powered thrust behavior.
- Status smoke exposed NUL padding in `lastDeviceLog`; that is a separate BLE log parsing hygiene issue and should be handled as a follow-up module, not hidden in this status severity change.

Promote to skill:
- Keep health status severity separate from informational status reason tokens; status reasons are not automatically warnings.
- Phone-visible BLE/status/UI changes need module-specific Android smoke coverage, not only the batch smoke after three modules.

### 2026-04-24 Stage 77: BLE log text length hardening

Scope:
- Continue the current low-risk batch with BLE log text delivery.
- Fix the NUL padding exposed by the Stage 76 phone `--status` smoke in `lastDeviceLog`.
- This is module `2/3`; full `nh02` acceptance remains due after module `3/3`, but module-specific phone smoke is required because the behavior is phone-visible.

External baseline:
- Bluetooth Core GATT treats characteristic values as byte values owned by the application/profile: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.
- Android BLE clients receive characteristic values as byte arrays; app code must decode the application payload, not assume C-string semantics: <https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic>.

Key outcomes:
- Added `RuntimeBleLogText.h` with bounded C-string length handling for firmware log payloads.
- `BLEBoatLock::processQueuedLogs()` now sets the log characteristic with exact string length instead of a padded queue buffer.
- Flutter `BleBoatLock.decodeLogLine()` now decodes only up to the first NUL byte and allows malformed UTF-8 replacement instead of throwing.
- Added native and Flutter coverage for padded and unterminated log values.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_log_text -f test_runtime_status -f test_runtime_ble_live_frame` -> `11/11` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub test/ble_boatlock_test.dart test/ble_smoke_logic_test.dart` -> passed.
- `cd boatlock && platformio test -e native` -> `252/252` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> full suite passed (`31/31`).
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `git diff --check` -> clean.
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `696961` bytes.
- `./tools/hw/nh02/flash.sh` -> build success, flash success, app image write `697328` bytes, hard reset via RTS.
- `./tools/hw/nh02/android-run-smoke.sh --status --wait-secs 130` -> exact install `Success`, then `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"status_stop_alert_roundtrip",...,"lastDeviceLog":"[EVENT] FAILSAFE_TRIGGERED reason=STOP_CMD"}` with no NUL padding.

Self-review:
- This is a diagnostic transport cleanup, not a command or actuator behavior change.
- The app-side defensive decoder protects against any future padded log value even if firmware changes regress.
- Phone smoke proves the actual log value consumed by Android is now clean.

Promote to skill:
- BLE text/log characteristic payloads must be length-bounded at firmware and defensively decoded at the client boundary.

### 2026-04-24 Stage 78: Simulation badge timer hardening

Scope:
- Finish the current low-risk three-module batch with `RuntimeSimBadge`.
- Keep simulation result badge text and HIL behavior unchanged.
- Remove absolute-expiry timer math from the badge display path.
- This is module `3/3`; full `nh02` acceptance and Android smoke batch are due after this module is committed and flashed.

External baseline:
- Arduino's non-blocking timer examples use `currentMillis - previousMillis >= interval` instead of delay-style or absolute expiry checks: <https://docs.arduino.cc/built-in-examples/digital/BlinkWithoutDelay/>.
- Arduino `millis()` guidance notes that rollover-safe logic should compare elapsed time, not `millis() >= previous + interval`: <https://arduinogetstarted.com/reference/arduino-millis>.

Key outcomes:
- `RuntimeSimBadge` now stores badge start time plus duration and checks unsigned elapsed time.
- Duration `0` explicitly hides the badge instead of relying on signed expiry arithmetic.
- Added native coverage for zero-duration badges and unsigned `millis()` rollover.
- Phone-smoke decision: no module-specific Android smoke added because this module does not change BLE protocol, commands, telemetry keys, app parsing, reconnect, install, or UI behavior. It remains covered by local native tests and the full batch Android smokes after module `3/3`.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_sim_badge` -> initially exposed a bad 32-bit-only rollover test assumption in native; after changing the test to use `~0UL`, `6/6` passed.
- `git diff --check` -> clean.
- `cd boatlock && platformio test -e native` -> `254/254` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `696969` bytes.
- Hardware acceptance and Android smoke batch pending after commit/push because this closes module `3/3`.

Self-review:
- This is a timer-boundary cleanup, not a user-visible protocol change.
- The refactor reduces timer ambiguity without adding new branches outside the module.
- The native test failure was in the test's fixed 32-bit rollover assumption, not production logic; using `~0UL` now verifies the actual unsigned type semantics on both native and embedded builds.

Promote to skill:
- One-shot diagnostic/UI timers should follow the same start-plus-duration unsigned elapsed-time rule as runtime watchdog and cadence timers.

### 2026-04-24 Stage 79: Batch hardware acceptance after modules 1-3

Scope:
- Close the three-module batch:
  - `RuntimeStatus`
  - BLE log text length handling
  - `RuntimeSimBadge`
- Prove the pushed `main` firmware and Android smoke APK on the `nh02` bench.

Bench validation:
- `./tools/hw/nh02/status.sh` -> RFC2217 service enabled/active, ESP32-S3 USB device present at `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:88:E0:03:BA:5C-if00`, listener on port `4000`.
- `./tools/hw/nh02/flash.sh` -> build success, flash success, app image write `697328` bytes, hard reset via RTS.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-batch-status-log-badge-60s.log --json-out /tmp/boatlock-batch-status-log-badge-60s.json` -> `[ACCEPT] PASS lines=76`.
- Acceptance matched BNO08x-RVC ready, display ready, EEPROM loaded, security state, BLE init/advertising, stepper config, STOP button, GPS UART data, and fresh compass heading events.
- Error scan over the 60-second log for panic/assert/Guru/config save or CRC errors/GPS stale/no data/compass loss/I2C errors/fail tokens -> no matches.

Android BLE smoke validation:
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> first install attempt hit `INSTALL_FAILED_USER_RESTRICTED`, canonical retry returned `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --status --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"status_stop_alert_roundtrip",...,"lastDeviceLog":"[EVENT] FAILSAFE_TRIGGERED reason=STOP_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- Phone-smoke policy is now explicit: module-specific smoke is required for phone-visible behavior, and the full Android batch runs after every third low-risk module.
- The MIUI install gate can still show a first-attempt `USER_RESTRICTED`, but the canonical retry succeeded and the wrapper produced terminal passing results.
- This batch does not prove powered thrust, real on-water hold quality, or auth/session behavior.

Promote to skill:
- No new durable rule; existing hardware acceptance and phone-smoke cadence rules covered this batch.

### 2026-04-24 Stage 80: HIL command parser narrowing

Scope:
- Start the next three-module HIL/simulation batch with `RuntimeSimCommand`.
- Keep on-device simulation as required validation infrastructure.
- Remove undocumented parser forms instead of preserving compatibility for a pre-alpha protocol.

External baseline:
- ArduPilot SITL runs the real autopilot code as a native executable and feeds it simulated sensor data from a flight dynamics model: <https://ardupilot.org/dev/docs/sitl-simulator-software-in-the-loop.html>.
- ArduPilot SITL is used for environment changes, failure-mode simulation, and optional component testing before real hardware risk: <https://ardupilot.org/dev/docs/using-sitl-for-ardupilot-testing.html>.
- ArduPilot Simulation-on-Hardware reuses the SITL model on the autopilot hardware and is used to check mission structure, output movement, physical failsafes, and communication traffic; it explicitly warns to disable dangerous actuators before allowing outputs to move: <https://ardupilot.org/dev/docs/sim-on-hardware.html>.
- PX4 separates SITL/HITL, treats simulation as the safe pre-real-world test layer, and documents headless SIH plus lockstep/faster-than-realtime operation as part of the simulator contract: <https://docs.px4.io/main/en/simulation/>.
- BoatLock decision from that baseline: keep HIL in `main`, exercise the real control code path, keep simulated faults typed, keep `SIM_*` commands deterministic, and do not let failed simulation commands mutate live runtime state.

Key outcomes:
- `RuntimeSimCommand` now accepts only documented exact command names.
- `SIM_RUN` now accepts only `SIM_RUN:<scenario_id>[,<speedup>]`.
- `speedup` is strictly `0` or `1`; malformed values no longer collapse to `0` through `atoi`.
- Removed undocumented JSON/space payload support and prefix lookalikes such as `SIM_RUNNER` or `SIM_REPORT_NOW`.
- Updated `docs/BLE_PROTOCOL.md` to match current `SIM_REPORT` behavior: no scenario-id parameter.
- Phone-smoke decision: no new Android smoke in this module slice because the change is parser-only and no app flow sends `SIM_*`; the HIL BLE surface will get a module-specific smoke when the batch reaches execution/log behavior or when a phone-visible SIM flow is added.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_sim_command -f test_runtime_sim_execution -f test_ble_command_handler` -> `48/48` passed.
- `git diff --check` -> clean.
- `cd boatlock && platformio test -e native` -> `257/257` passed.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `696469` bytes.
- Full `nh02` acceptance remains deferred by the three-module cadence; this is module `1/3`.

Self-review:
- This removes code and narrows the command surface without changing documented commands.
- The remaining risk is that there is no Android SIM smoke yet; that belongs with the execution/log module work so the smoke can assert an end-to-end result, not just parser acceptance.

Promote to skill:
- HIL parser behavior should stay strict and documented; do not add alternate payload encodings without an explicit product reason and tests.

### 2026-04-24 Stage 81: HIL execution side-effect hardening

Scope:
- Continue the HIL/simulation batch with `RuntimeSimExecution`.
- Apply the deeper simulation baseline from ArduPilot/PX4: simulation is core safety validation, but the command interface must be deterministic and fail-closed.
- This is module `2/3`.

External baseline:
- ArduPilot SITL and Simulation-on-Hardware prove real control paths against simulated sensors/faults before vehicle risk: <https://ardupilot.org/dev/docs/sitl-simulator-software-in-the-loop.html> and <https://ardupilot.org/dev/docs/sim-on-hardware.html>.
- PX4 simulation separates SITL/HITL and treats simulator speed/lockstep behavior as part of the test contract: <https://docs.px4.io/main/en/simulation/>.

Key outcomes:
- `SIM_RUN` side effects (`stopAllMotion`, failsafe clear, anchor-denial clear, wall-clock reset) now happen only after a simulation run actually starts.
- Invalid payloads and failed starts are reported without mutating live runtime state.
- Report chunking now clears stale output before validating chunk size; `chunkSize=0` produces `REPORT_UNAVAILABLE` instead of leaving ambiguous empty chunks.
- Added Android smoke mode `sim`: exact APK install, BLE telemetry, `SIM_RUN:S0_hold_still_good,1`, observed `SIM` mode, `SIM_ABORT`, observed mode exit, then zero-throttle manual recovery so the bench does not remain in `HOLD/ALERT`.
- Phone-smoke decision: module-specific Android smoke is required because this changes the BLE-visible `SIM_*` execution path.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_sim_execution -f test_runtime_sim_command -f test_runtime_sim_log` -> `19/19` passed.
- `cd boatlock && platformio test -e native -f test_runtime_sim_execution -f test_runtime_sim_command -f test_runtime_sim_log -f test_ble_command_handler` -> `52/52` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub test/ble_smoke_logic_test.dart` -> passed.
- `./tools/android/build-smoke-apk.sh --mode sim` -> sandbox Gradle run failed with `java.net.SocketException: Operation not permitted`; reran host-side and built `app-debug.apk` successfully.
- `cd boatlock && platformio test -e native` -> `258/258` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> full suite passed (`31/31`) before and after SIM smoke cleanup.
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `696485` bytes.
- `./tools/hw/nh02/flash.sh` -> build success, flash success, app image write `696848` bytes, hard reset via RTS.
- First `./tools/hw/nh02/android-run-smoke.sh --sim --wait-secs 130` -> exact install `Success` after one canonical retry, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"sim_run_abort_roundtrip",...,"mode":"HOLD","status":"ALERT","lastDeviceLog":"[SIM] ABORTED"}`. This proved run/abort but exposed that the smoke left the bench in `HOLD/ALERT`.
- After adding zero-throttle manual recovery, `./tools/hw/nh02/android-run-smoke.sh --sim --no-build --wait-secs 130` -> exact install `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"sim_run_abort_roundtrip","dataEvents":6,"deviceLogEvents":2,"mode":"IDLE","status":"WARN","statusReasons":"","secPaired":false,"secAuth":false,"rssi":-35,"lastDeviceLog":"[SIM] ABORTED"}`.
- Full `nh02` serial acceptance remains deferred by the three-module cadence; this is module `2/3`, but module-specific phone smoke ran because `SIM_*` execution is phone-visible BLE behavior.

Self-review:
- This reduces accidental live-state mutation from malformed test commands.
- Realtime `S0` in phone smoke keeps SIM mode observable and then aborts it; it is not a powered thrust test.
- The first SIM smoke pass revealed a cleanup problem in the smoke flow, not in the firmware execution change; the final accepted smoke now leaves the bench out of `SIM`, out of `MANUAL`, and out of `ALERT`.

Promote to skill:
- Failed simulation commands must not mutate live runtime state; SIM smoke should prove run/abort end-to-end once execution behavior changes.

### 2026-04-24 Stage 82: HIL log sanitizer

Scope:
- Finish the HIL/simulation batch with `RuntimeSimLog`.
- Keep SIM log messages human-readable and compatible with the BLE log characteristic.
- This is module `3/3`; full `nh02` serial acceptance and Android smoke batch are due after this module.

External baseline:
- OWASP Logging Cheat Sheet recommends validating/sanitizing event data from other trust zones, neutralizing CR/LF/delimiters, bounding field lengths, and testing logs for injection resistance: <https://cheatsheetseries.owasp.org/cheatsheets/Logging_Cheat_Sheet.html>.
- OWASP Log Injection documents forged log entries through untrusted input containing line separators: <https://owasp.org/www-community/attacks/Log_Injection>.
- CWE/OWASP CRLF guidance treats CR/LF as record separators that must be neutralized when user-controlled input reaches logs: <https://owasp.org/www-community/vulnerabilities/CRLF_Injection>.

Key outcomes:
- Added `sanitizeRuntimeSimLogField()` for SIM outcome fields before they are forwarded to log lines.
- `RuntimeSimLog` now neutralizes CR/LF/TAB/NUL/control characters and bounds command-derived fields.
- Added native coverage for forged multi-line input and bounded unknown-command logs.
- Phone-smoke decision: no new standalone phone smoke for this module because Stage 81 already added and ran `--sim`; the full batch Android smoke will include `--sim` again after this module.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_sim_log -f test_runtime_sim_execution -f test_runtime_sim_command` -> `21/21` passed.
- `git diff --check` -> clean.
- `cd boatlock && platformio test -e native` -> `260/260` passed.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> full suite passed (`31/31`).
- `python3 tools/sim/test_sim_core.py` -> `4/4` passed.
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> all scenarios `PASS`.
- `pytest tools/ci/test_*.py` -> `9/9` passed.
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `696933` bytes.
- Hardware acceptance and Android smoke batch pending below because this closes module `3/3`.

Self-review:
- This is log hygiene for the HIL interface, not a control-loop behavior change.
- Report chunk content is still emitted as text chunks, but control characters are neutralized and chunks are bounded to the existing report chunk size.

Promote to skill:
- SIM logs are part of the test API and must stay single-line, bounded, and injection-resistant.

### 2026-04-24 Stage 83: Batch hardware acceptance after HIL modules 1-3

Scope:
- Close the HIL/simulation three-module batch:
  - `RuntimeSimCommand`
  - `RuntimeSimExecution`
  - `RuntimeSimLog`
- Prove the firmware and Android smoke app on `nh02`, including the new SIM smoke path.

Bench validation:
- `./tools/hw/nh02/flash.sh` -> build success, flash success, app image write `697296` bytes, hard reset via RTS.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-batch-hil-log-60s.log --json-out /tmp/boatlock-batch-hil-log-60s.json` -> `[ACCEPT] PASS lines=73`.
- Acceptance matched BNO08x-RVC ready, display ready, EEPROM loaded, security state, BLE init/advertising, stepper config, STOP button, GPS UART data, and fresh compass heading events.
- Error scan over the 60-second log for panic/assert/Guru/config save or CRC errors/GPS stale/no data/compass loss/I2C errors/fail tokens -> no matches.

Android BLE smoke validation:
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received",...,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --status --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"status_stop_alert_roundtrip",...,"lastDeviceLog":"[EVENT] FAILSAFE_TRIGGERED reason=STOP_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --sim --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"sim_run_abort_roundtrip","dataEvents":7,"deviceLogEvents":2,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[SIM] ABORTED"}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect",...}`.

Self-review:
- The batch now has end-to-end coverage for the HIL BLE surface through Android, not just native parser/execution tests.
- SIM smoke uses realtime `S0` only long enough to observe `SIM`, then aborts and recovers through zero-throttle manual control; it does not prove powered actuator behavior.
- The remaining HIL gap is scenario richness: the existing untracked `tools/sim/research/environment_inputs.*` looks relevant for future environmental scenario inputs and should be triaged as a separate research artifact, not mixed into this execution/log safety batch.

Promote to skill:
- No new durable rule beyond the SIM smoke requirement and single-line bounded SIM logs already promoted.

### 2026-04-24 Stage 84: HIL environment input research triage

Scope:
- Triage the existing untracked `tools/sim/research/environment_inputs.*` artifacts after the HIL batch.
- Keep them separate from the execution/log safety commits because they are scenario-input research, not runtime behavior.

Key outcomes:
- Kept `tools/sim/research/environment_inputs.md` as a human-readable source-backed research note for future HIL environment and motor classes.
- Kept `tools/sim/research/environment_inputs.raw.json` as machine-readable raw data, explicitly marked as `raw_research_not_sim_schema`.
- The artifact includes candidate future scenario seeds such as normal river current, Volga spring flow, Rybinsk fetch, Ladoga storm abort, and Baltic gulf drift.

Validation:
- `python3 -m json.tool tools/sim/research/environment_inputs.raw.json` -> parsed successfully.
- Markdown review confirmed it is research input only, not a current simulator schema or runtime source of truth.

Self-review:
- This does not change firmware, Flutter, BLE, or acceptance behavior.
- It should not be used to tune control constants directly until converted into explicit scenarios with tests.

Promote to skill:
- No new rule; keep raw environmental research separate from executable simulator schema until normalized.

### 2026-04-25 Stage 81: Environmental input research for future HIL scenarios

Scope:
- Collect raw source-backed inputs for future BoatLock simulation and hardware-in-the-loop scenarios while code refactoring continues elsewhere.
- Cover trolling motor thrust/current classes, water-body classes, Volga/Oka/Ladoga/Onega/reservoir/Baltic examples, wind/current/wave ranges, and candidate scenario seeds.

Key outcomes:
- Added `tools/sim/research/environment_inputs.md` as a human-readable research note with source links and confidence notes.
- Added `tools/sim/research/environment_inputs.raw.json` as a machine-readable raw dataset for later normalization.
- Marked unresolved current data gaps explicitly for Oka, Kama, Neva, and several reservoirs instead of flattening them into false precision.
- Kept the output as research data only; no simulator behavior, thresholds, firmware, BLE, or test code changed.

Validation:
- `python3 -m json.tool tools/sim/research/environment_inputs.raw.json` -> valid JSON.

Self-review:
- The useful durable point is that river/current data must be modeled by water-body class, reach, and condition rather than as a single per-river constant.
- The weakest areas are Oka/Kama/Neva reach-specific current data and localized reservoir wave/current data; those need hydropost, navigation-lotsiya, or operator forecast sources before final scenario normalization.

Promote to skill:
- No skill change yet; this is a task-specific research artifact until the data is normalized into simulator/HIL inputs.

### 2026-04-25 Stage 85: Android smoke coverage audit and anchor safety smoke

Scope:
- Audit recent module refactors for missed Android phone smokes.
- Close the gap for phone-visible anchor command behavior without changing firmware or widening the product command surface.

Key outcomes:
- Found that earlier anchor persistence/BLE-visible anchor work had batch Android smokes, but no targeted phone smoke for anchor command delivery and safety-gate denial.
- Added Android smoke mode `anchor`.
- `anchor` smoke sends `ANCHOR_ON`, waits for `[EVENT] ANCHOR_DENIED`, sends `ANCHOR_OFF`, and verifies the device did not enter `ANCHOR`.
- Added local smoke-logic tests for anchor denied log detection and safe mode check.
- Updated `tools/android/*`, `tools/hw/nh02/android-run-smoke.sh`, and the hardware acceptance skill so future anchor-command/safety-gate changes have a canonical phone smoke path.

Validation:
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub test/ble_smoke_logic_test.dart` -> passed (`5/5`).
- `./tools/android/build-smoke-apk.sh --mode anchor` -> sandbox Gradle run failed with `java.net.SocketException: Operation not permitted`; reran host-side and built `app-debug.apk` successfully.
- `./tools/hw/nh02/android-run-smoke.sh --anchor --no-build --wait-secs 130` -> exact APK install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"anchor_denied_roundtrip","dataEvents":4,"deviceLogEvents":2,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[EVENT] ANCHOR_OFF reason=BLE_CMD"}`.
- `cd boatlock_ui && env HOME=/tmp XDG_CACHE_HOME=/tmp flutter test --no-pub` -> full suite passed (`32/32`).
- `git diff --check` -> clean.

Self-review:
- This is a missing acceptance path fix, not a behavior change.
- The smoke deliberately proves denial on the bench safety gate and cleanup, not real on-water hold quality.
- It still does not cover `SET_ANCHOR` persistence over Android because sending that command would mutate saved anchor state; that should be a separate controlled smoke only if we add explicit setup/restore semantics.

Promote to skill:
- Added the durable `--anchor` smoke rule to `skills/boatlock-hardware-acceptance/SKILL.md`.

### 2026-04-25 Stage 86: GPS UART watchdog restart window

Scope:
- Start the next three-module refactor batch with `RuntimeGpsUart`.
- Keep the GPS UART watchdog small and deterministic while preserving current wiring/runtime behavior.
- This is module `1/3`; hardware acceptance remains due after module `3/3`.

External baseline:
- PX4 position-loss failsafe is based on timeout since the last aiding sensor data was fused, not on the existence of an old position value: <https://docs.px4.io/main/en/config/safety#position-estimation-failsafes>.
- ArduPilot pre-arm checks separate GPS configuration/data/fix/HDOP failures and keep GPS-dependent modes behind explicit checks: <https://ardupilot.org/copter/docs/common-prearm-safety-checks.html>.
- u-blox UART guidance documents that UART bandwidth and message availability are real constraints; if output exceeds port bandwidth, messages can be dropped, so host-side byte-flow monitoring is valid: <https://content.u-blox.com/sites/default/files/products/documents/u-blox7-V14_ReceiverDescriptionProtocolSpec_%28GPS.G7-SW-12001%29_Public.pdf>.

Key outcomes:
- Replaced `lastRestartMs_ == 0` sentinel behavior with an explicit `restartSeen_` flag.
- A GPS UART restart now resets the no-data baseline, so silence after a restart gets a fresh grace window instead of immediately reusing boot time.
- Added native tests for restart grace-window behavior and restart cooldown when the first restart timestamp is `0`.
- Promoted the durable watchdog rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added for this module because it does not change BLE protocol, Flutter parsing, app flow, or phone-visible command behavior. Hardware batch remains scheduled after module `3/3`.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_gps_uart -f test_runtime_gnss` -> passed (`18/18`).
- `cd boatlock && platformio test -e native` -> passed (`262/262`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `696897` bytes.
- `git diff --check` -> clean.

Self-review:
- The change reduces sentinel ambiguity without widening the GPS feature surface.
- It does alter warning timing after a GPS UART restart: this is intended because the restart is a new observation window.
- Remaining risk is hardware timing under real GPS silence/recovery; that belongs in the three-module hardware acceptance batch unless a later module touches GPS driver setup directly.

Promote to skill:
- GPS UART restart must reset the no-data baseline and use explicit restart-seen state.

### 2026-04-25 Stage 87: BNO08x UART-RVC frame boundary validation

Scope:
- Continue the three-module refactor batch with `BnoRvcFrame`.
- Keep compass input fail-closed before data reaches `BNO08xCompass` live heading state.
- This is module `2/3`; hardware acceptance remains due after module `3/3`.

External baseline:
- The BNO08x datasheet defines UART-RVC as a 100 Hz 19-byte message with `0xAAAA` header, checksum over index/orientation/accel/reserved bytes, yaw range `+/-180 deg`, pitch `+/-90 deg`, and roll `+/-180 deg`: <https://docs.sparkfun.com/SparkFun_VR_IMU_Breakout_BNO086_QWIIC/assets/component_documentation/BNO080_085-Datasheet_v1.16.pdf>.
- Adafruit's UART-RVC guide and Arduino library use 115200 baud, read the 19-byte packet, and validate header plus checksum before exposing heading data: <https://learn.adafruit.com/adafruit-9-dof-orientation-imu-fusion-breakout-bno085/uart-rvc-for-arduino> and <https://github.com/adafruit/Adafruit_BNO08x_RVC>.

Key outcomes:
- Added angle-range validation to `bnoRvcParseFrame()` after checksum and before mutating the output sample.
- Out-of-range yaw/pitch/roll packets are rejected even if their checksum is internally consistent.
- Added native tests for out-of-range rejection and inclusive datasheet boundaries.
- Promoted the parser-boundary rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added because this module changes sensor-frame validation only, not BLE/app behavior. Hardware batch remains scheduled after module `3/3`.

Validation:
- `cd boatlock && platformio test -e native -f test_bno_rvc_frame -f test_bno08x_compass` -> passed (`8/8`).
- `cd boatlock && platformio test -e native` -> passed (`264/264`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `696981` bytes.
- `git diff --check` -> clean.

Self-review:
- This is a boundary check, not a transport rewrite.
- It reduces the chance of a single corrupted but checksummed frame producing a large compass jump.
- It does not validate acceleration ranges because the datasheet exposes accel as signed mg and our current safety decision only depends on heading freshness and heading angle.

Promote to skill:
- BNO08x UART-RVC frames must pass header/checksum and datasheet orientation ranges before updating live heading state.

### 2026-04-25 Stage 88: Compass health timeout floors

Scope:
- Close the three-module sensor watchdog batch with `RuntimeCompassHealth`.
- Make compass event-loss detection fail closed even if timeout inputs are accidentally zero or too small.
- This is module `3/3`; hardware and Android acceptance ran after the module.

External baseline:
- ArduPilot pre-arm safety checks treat compass health as an explicit blocker and do not allow dependent modes to proceed when the compass is not healthy: <https://ardupilot.org/copter/docs/common-prearm-safety-checks.html>.
- PX4 safety configuration treats position/sensor-aiding loss through timeout-based failsafes rather than assuming stale data remains valid: <https://docs.px4.io/main/en/config/safety#position-estimation-failsafes>.
- BNO08x UART-RVC emits heading/accel packets at 100 Hz, so missing heading events are a watchdog condition, not a normal steady state: <https://docs.sparkfun.com/SparkFun_VR_IMU_Breakout_BNO086_QWIIC/assets/component_documentation/BNO080_085-Datasheet_v1.16.pdf>.

Key outcomes:
- Added local floors for first-heading-event timeout and stale-heading-event timeout.
- Timeout values `0` or `1` no longer disable compass loss detection.
- Added native coverage for zero/tiny timeout inputs and adjusted rollover coverage to test rollover above the floor.
- Promoted the fail-closed compass timeout rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no new Android smoke mode needed because this module changes onboard sensor health only, not BLE/app command behavior. Full Android batch ran because this was module `3/3` and firmware was flashed.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_compass_health -f test_bno08x_compass -f test_bno_rvc_frame` -> passed (`15/15`).
- `cd boatlock && platformio test -e native` -> passed (`266/266`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `696977` bytes.
- `git diff --check` -> clean.

Self-review:
- This narrows failure behavior without changing normal configured timings (`3000 ms` first event, `750 ms` stale event).
- The chosen local floors are below current production constants, so they protect against bad inputs without making healthy runtime stricter.
- Remaining risk is real sensor timing after the parser/health changes; covered by the hardware acceptance batch below.

Promote to skill:
- Compass health timeouts must apply local fail-closed floors; zero/tiny input must not disable heading-event loss detection.

### 2026-04-25 Stage 89: Batch hardware acceptance after sensor watchdog modules

Scope:
- Close the three-module sensor/watchdog batch:
  - `RuntimeGpsUart`
  - `BnoRvcFrame`
  - `RuntimeCompassHealth`
- Prove firmware and Android BLE smoke paths on the `nh02` bench after flashing the new firmware.

Bench validation:
- `./tools/hw/nh02/flash.sh` -> build success, flash success, app image write `697344` bytes, hard reset via RTS.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-batch-sensor-60s.log --json-out /tmp/boatlock-batch-sensor-60s.json` -> `[ACCEPT] PASS lines=77`.
- Acceptance matched BNO08x-RVC ready, display ready, EEPROM loaded, security state, BLE init/advertising, stepper config, STOP button, GPS UART data, and fresh compass heading events.
- Error scan over the 60-second log for panic/assert/Guru/config save or CRC errors/GPS stale/no data/compass loss/I2C errors/fail/error tokens -> no matches.

Android BLE smoke validation:
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> exact install `Success` after one canonical MIUI retry; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --status --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"status_stop_alert_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[EVENT] FAILSAFE_TRIGGERED reason=STOP_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --anchor --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"anchor_denied_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[EVENT] ANCHOR_OFF reason=BLE_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --sim --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"sim_run_abort_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[SIM] ABORTED"}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success` after one canonical MIUI retry; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.

Self-review:
- This batch proves the new compass parser/health behavior still boots, receives fresh heading frames, and keeps BLE telemetry/control smokes stable on real hardware.
- It does not prove real on-water hold quality or powered thrust behavior.
- MIUI still occasionally requires the canonical retry path, but the wrapper terminal verdicts were successful and exact install was proven for each smoke.

Promote to skill:
- No new durable rule beyond the compass/GPS watchdog rules already promoted.

### 2026-04-25 Stage 90: Motor auto-thrust finite input gate

Scope:
- Start the next actuator/control batch with `MotorControl`.
- Keep motor output deterministic and quiet when runtime tuning inputs are invalid.
- This is module `1/3`, but hardware acceptance ran immediately because this touches actuator safety.

External baseline:
- ArduPilot pre-arm checks block propulsion when configuration, calibration, or safety prerequisites are invalid: <https://ardupilot.ardupilot.org/copter/docs/common-prearm-safety-checks.html>.
- ArduPilot motor range/calibration guidance treats motor start thresholds and low-throttle tests as explicit safety/tuning concerns, not incidental behavior: <https://ardupilot.org/copter/docs/set-motor-range.html>.
- PX4 safety configuration treats actuator/failsafe behavior as explicit safety policy and supports preventing accidental vehicle use during safety/maintenance states: <https://docs.px4.io/main/en/config/safety.html>.

Key outcomes:
- `driveAnchorAuto()` now fails closed if `holdRadiusMeters`, `deadbandMeters`, or `rampPctPerSec` are non-finite.
- This prevents `NaN` tuning values from reaching clamp/ramp math and producing undefined motor output.
- Added native coverage proving non-finite tuning stops auto-thrust, writes zero PWM, and leaves direction pins in idle state.
- Promoted the actuator finite-input rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no new Android smoke mode needed because BLE protocol/app behavior did not change. Existing manual/status/anchor/sim/reconnect/reset smokes ran after flashing because actuator firmware changed.

Validation:
- `cd boatlock && platformio test -e native -f test_motor_control -f test_runtime_motion -f test_ble_command_handler` -> passed (`52/52`).
- `cd boatlock && platformio test -e native` -> passed (`267/267`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697005` bytes.
- `./tools/hw/nh02/flash.sh` -> build success, flash success, app image write `697376` bytes, hard reset via RTS.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-motor-safety-60s.log --json-out /tmp/boatlock-motor-safety-60s.json` -> `[ACCEPT] PASS lines=80`.
- Error scan over the 60-second log for panic/assert/Guru/config save or CRC errors/GPS stale/no data/compass loss/I2C errors/fail/error tokens -> no matches.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --status --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"status_stop_alert_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[EVENT] FAILSAFE_TRIGGERED reason=STOP_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --anchor --wait-secs 130` -> exact install `Success` after one canonical MIUI retry; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"anchor_denied_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[EVENT] ANCHOR_OFF reason=BLE_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --sim --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"sim_run_abort_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[SIM] ABORTED"}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `git diff --check` -> clean.

Self-review:
- This is a small fail-closed input gate, not a retune of the controller.
- Normal configured values are unchanged; only non-finite tuning values now force quiet output.
- The bench smokes are zero-throttle/safety-path checks and do not prove powered thrust tuning or on-water hold behavior.

Promote to skill:
- Motor auto-thrust must reject non-finite distance/tuning input before clamp/ramp math and drive quiet output instead.

### 2026-04-25 Stage 91: Stepper idle-release timer state

Scope:
- Continue actuator/control refactor with `StepperControl`.
- Fix idle coil release when the idle timer starts at `millis()==0`; zero is a valid timestamp and must not behave as "timer not started".
- User updated the default cadence from three to five modules before hardware acceptance. `AGENTS.md`, `skills/boatlock/SKILL.md`, and `skills/boatlock-hardware-acceptance/SKILL.md` now say hardware runs after every fifth normal module.
- This is actuator safety work, so hardware acceptance and Android BLE smokes still ran immediately instead of waiting for module five.

External baseline:
- AccelStepper documents `disableOutputs()`/`enableOutputs()` as the intended mechanism to disable motor pin outputs during low-power/idle states and re-enable before stepping: <https://www.airspayce.com/mikem/arduino/AccelStepper/classAccelStepper.html>.
- The SureStep drive manual treats idle-current reduction as a normal stepper-drive feature to reduce power and heat while accepting reduced holding torque: <https://cdn.automationdirect.com/static/manuals/surestepmanual/ch5.pdf>.

Key outcomes:
- Replaced `idleSinceMs == 0` sentinel behavior with explicit `idleTimerActive` state.
- Idle/cancel paths now release coils after `COIL_RELEASE_DELAY_MS` even when the first idle sample is timestamp `0`.
- Motion/manual/start config paths clear the idle timer deliberately before driving outputs.
- Added native tests for idle-at-zero and cancel-at-zero coil release.
- Promoted the explicit idle-timer-state rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no new Android smoke mode needed because BLE protocol/app behavior did not change. Existing smokes ran after flashing because stepper actuator firmware changed.

Validation:
- `cd boatlock && platformio test -e native -f test_stepper_control -f test_runtime_motion -f test_ble_command_handler` -> passed (`48/48`).
- `cd boatlock && platformio test -e native` -> passed (`269/269`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697057` bytes.
- `./tools/hw/nh02/status.sh` -> target confirmed, RFC2217 service active, ESP32-S3 USB serial `98:88:e0:03:ba:5c`.
- `./tools/hw/nh02/flash.sh` -> build success, flash success, app image write `697424` bytes, hard reset via RTS.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-stepper-idle-60s.log --json-out /tmp/boatlock-stepper-idle-60s.json` -> `[ACCEPT] PASS lines=79`.
- Acceptance matched BNO08x-RVC ready, display ready, EEPROM loaded, security state, BLE init/advertising, stepper config, STOP button, GPS UART data, and fresh compass heading events.
- Error scan over the 60-second log for panic/assert/Guru/config save or CRC errors/GPS stale/no data/compass loss/I2C errors/fail/error tokens -> no matches.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --status --wait-secs 130` -> first install hit `INSTALL_FAILED_USER_RESTRICTED`, canonical retry `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"status_stop_alert_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[EVENT] FAILSAFE_TRIGGERED reason=STOP_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --anchor --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"anchor_denied_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[EVENT] ANCHOR_OFF reason=BLE_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --sim --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"sim_run_abort_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[SIM] ABORTED"}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `git diff --check` -> clean.

Self-review:
- This is a state-machine correction, not a retune of stepper speed, acceleration, or coil-release delay.
- It removes a timestamp sentinel bug without widening command surface or changing normal non-zero runtime behavior.
- The bench smokes prove boot, sensor readiness, BLE command safety paths, reconnect, and reset recovery; they do not measure stepper coil temperature or powered mechanical load.

Promote to skill:
- Default hardware cadence is five modules, with immediate hardware validation still required for actuator, driver, pinout, deploy/debug, BLE reconnect/install, and other high-risk paths.
- Stepper idle timers must use explicit active state; do not use `0` as a timestamp sentinel.

### 2026-04-25 Stage 92: Runtime motion finite tuning gate

Scope:
- Continue the actuator/control batch with `RuntimeMotion`.
- Keep mode arbitration fail-closed when auto-control tuning values are non-finite.
- This is module `3/5` under the updated five-module cadence. Hardware acceptance is deferred because this cut changes software policy only, not drivers, pinout, BLE protocol, install/reconnect, or hardware wrappers.

External baseline:
- ArduPilot pre-arm checks block operation when configuration, sensors, storage, or safety prerequisites are invalid, and the docs warn against skipping those checks instead of fixing the source issue: <https://ardupilot.org/copter/docs/common-prearm-safety-checks.html>.
- PX4 failsafe configuration treats safety as an explicit state machine with Hold/Disarm/Terminate actions and short manual-control/data-link loss timeouts rather than continuing on stale or invalid control assumptions: <https://docs.px4.io/main/en/config/safety>.

Key outcomes:
- `RuntimeMotion::applyControl()` now reads auto-control settings through one finite gate before heading alignment or thrust policy uses them.
- Non-finite `HoldRadius`, `DeadbandM`, `MaxThrustA`, `ThrRampA`, or `AngTol` now quiets auto outputs and returns.
- Valid settings still clamp to the existing schema ranges and preserve normal behavior.
- Added native coverage proving a previously active anchor-auto thrust state is cleared when runtime tuning becomes non-finite.
- Promoted the runtime tuning finite-gate rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run because BLE protocol, app UI, reconnect, install, and telemetry shape did not change.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_motion -f test_runtime_control -f test_motor_control -f test_runtime_control_input_builder` -> passed (`32/32`).
- `cd boatlock && platformio test -e native` -> passed (`270/270`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697241` bytes.
- `git diff --check` -> clean.

Self-review:
- This is not a controller retune; it only prevents invalid settings from feeding alignment or actuation.
- Settings load/set paths already reject or sanitize invalid values, but this adds a local runtime guard at the actuation boundary.
- Remaining risk is powered mechanical behavior under valid but poor tuning; that belongs to simulation/on-water tuning, not this finite-gate cut.

Promote to skill:
- Runtime motion arbitration must reject non-finite auto-control settings before heading alignment or thrust policy uses them.

### 2026-04-25 Stage 93: Manual control source-owned lease

Scope:
- Continue the actuator/control batch with `ManualControl`.
- Standardize manual mode for phone plus future BLE remote/joystick sources.
- This is module `4/5`; hardware and Android acceptance are deferred to module five because this changes firmware lease arbitration only and the current phone app still uses the same single `BLE_PHONE` source.

External baseline:
- PX4 manual-control loss failsafe is tied to the selected manual control source and a timeout after the last setpoint, because the vehicle otherwise continues using the last stick position until the timeout: <https://docs.px4.io/main/en/config/safety>.
- ArduPilot radio failsafe treats loss/corruption of RC input beyond a configured timeout as a failsafe condition rather than accepting stale pilot input indefinitely: <https://ardupilot.org/copter/docs/radio-failsafe.html>.

Key outcomes:
- `ManualControl::apply()` now treats manual mode as a source-owned deadman lease.
- Same-source `MANUAL_SET` refreshes the TTL.
- Competing sources are rejected while the current lease is live and can take over only after TTL expiry or explicit `MANUAL_OFF`/`STOP`.
- Added native coverage for same-source refresh, competing-source rejection, and takeover after lease expiry.
- Updated `docs/BLE_PROTOCOL.md` and `docs/MANUAL_CONTROL.md`; the latter was stale and still claimed manual control was not exposed in the main app flow.
- Promoted the source-owned manual lease rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because the shipped phone path remains a single `BLE_PHONE` source. Manual Android smoke is scheduled with the module-five hardware batch.

Validation:
- `cd boatlock && platformio test -e native -f test_manual_control -f test_ble_command_handler -f test_runtime_motion -f test_runtime_control` -> passed (`55/55`).
- `cd boatlock && platformio test -e native` -> passed (`271/271`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697309` bytes.
- `git diff --check` -> clean.

Self-review:
- This intentionally changes future multi-controller behavior: no silent source fight while a live manual lease exists.
- It does not remove manual control or alter the current phone command shape.
- Remaining work for actual external BLE remote support is source identity at the BLE/session layer; current phone commands still enter as `BLE_PHONE`.

Promote to skill:
- Manual control must remain a source-owned deadman lease; do not let a second source overwrite a live lease without expiry or explicit stop.

### 2026-04-25 Stage 94: Supervisor config and command-limit fail-closed gate

Scope:
- Close the five-module actuator/control batch with `AnchorSupervisor` and `RuntimeSupervisorPolicy`.
- Keep failsafe timeout and command-limit config bounded before it can affect supervisor decisions.
- This is module `5/5`; hardware acceptance and Android BLE smokes ran after the module.

External baseline:
- ArduPilot pre-arm checks treat invalid configuration and unhealthy subsystems as blockers, and the docs recommend fixing the cause instead of bypassing safety checks: <https://ardupilot.org/copter/docs/common-prearm-safety-checks.html>.
- ArduPilot radio failsafe triggers after configured RC loss timeout and explicitly documents invalid failsafe parameter values falling back to a safe action: <https://ardupilot.org/copter/docs/radio-failsafe.html>.
- PX4 safety configuration models failsafes as explicit actions such as Hold, Disarm, and Terminate, with timeouts for manual-control and data-link loss: <https://docs.px4.io/main/en/config/safety>.

Key outcomes:
- `AnchorSupervisor` now clamps `maxCommandThrustPct` locally to `0..100` before command out-of-range checks.
- Negative command-limit config fails closed by allowing only zero command thrust.
- `RuntimeSupervisorPolicy` now clamps non-finite timeout settings to the safe minimum instead of relying on `constrain()`/casts.
- Added native coverage for non-finite timeout settings, over-100 command limit, and negative command limit.
- Promoted the supervisor fail-closed config rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no new smoke mode needed because BLE command shape and app behavior did not change. Full Android smoke package ran because this closed module `5/5`.

Validation:
- `cd boatlock && platformio test -e native -f test_anchor_supervisor -f test_runtime_supervisor_policy -f test_runtime_motion -f test_motor_control` -> passed (`42/42`).
- `cd boatlock && platformio test -e native` -> passed (`274/274`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697453` bytes.
- `./tools/hw/nh02/status.sh` -> target confirmed, RFC2217 service active, ESP32-S3 USB serial `98:88:e0:03:ba:5c`.
- `./tools/hw/nh02/flash.sh` -> build success, flash success, app image write `697824` bytes, hard reset via RTS.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-control-batch-60s.log --json-out /tmp/boatlock-control-batch-60s.json` -> `[ACCEPT] PASS lines=75`.
- Acceptance matched BNO08x-RVC ready, display ready, EEPROM loaded, security state, BLE init/advertising, stepper config, STOP button, fresh compass heading events, and GPS UART data.
- Error scan over the 60-second log for panic/assert/Guru/config save or CRC errors/GPS stale/no data/compass loss/I2C errors/fail/error tokens -> no matches.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --status --wait-secs 130` -> first install hit `INSTALL_FAILED_USER_RESTRICTED`, canonical retry `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"status_stop_alert_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[EVENT] FAILSAFE_TRIGGERED reason=STOP_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --anchor --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"anchor_denied_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[EVENT] ANCHOR_OFF reason=BLE_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --sim --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"sim_run_abort_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[SIM] ABORTED"}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`; final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `git diff --check` -> clean.

Self-review:
- This keeps normal supervisor timing unchanged for valid settings.
- The change is defensive at the boundary where bad config could weaken failsafe decisions.
- The hardware batch proves boot, sensors, BLE control smokes, reconnect, and ESP reset recovery; it still does not prove powered thrust or on-water hold quality.

Promote to skill:
- Supervisor timeout and command-limit values must be bounded/fail-closed at the policy boundary before the failsafe state machine uses them.

### 2026-04-25 Stage 95: Anchor heading normalization bounded math

Scope:
- Start the next five-module navigation/anchor batch with `AnchorControl`.
- Keep anchor-point persistence behavior unchanged, but remove input-magnitude-dependent heading normalization from the safety path.
- This is module `1/5`; hardware and Android acceptance are deferred to module five because BLE protocol, phone app behavior, pins, and hardware drivers did not change.

External baseline:
- OpenCPN Auto Anchor keeps anchor monitoring as explicit anchor/watch state and depends on stable anchor position/state math rather than interactive correction loops: <https://opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:manual_advanced:features:auto_anchor>.
- Applied rule for BoatLock: safety-path angle normalization must be deterministic and bounded for any finite persisted/input value.

Key outcomes:
- `AnchorControl::normalizeHeading()` now rejects non-finite headings to `0` and uses `fmodf()` for bounded normalization into `[0, 360)`.
- Removed repeated `while` normalization loops that could hang on very large finite headings.
- Added native coverage for positive wrap, negative wrap, huge finite headings, and `NaN`.
- Promoted the bounded heading-normalization rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because command shape, telemetry shape, and app behavior did not change.

Validation:
- `cd boatlock && platformio test -e native -f test_anchor_control -f test_ble_command_handler -f test_runtime_ble_params -f test_runtime_anchor_gate` -> passed (`45/45`).
- `cd boatlock && platformio test -e native` -> passed (`275/275`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697465` bytes.
- `git diff --check` -> clean.

Self-review:
- The change is intentionally narrow: invalid persisted heading still becomes safe heading `0`, and valid anchor coordinates are not modified.
- No runtime mode, GNSS source, BLE, or motor behavior changed.
- Remaining validation risk is real GNSS/on-water hold behavior, which this module does not exercise.

Promote to skill:
- Heading and angle normalization in safety-path code must use bounded modulo-style math and must treat non-finite input as a safe value.

### 2026-04-25 Stage 96: GNSS quality gate fail-closed boundaries

Scope:
- Continue the navigation/anchor batch with `GnssQualityGate`.
- Keep accepted BLE/app behavior unchanged while hardening the anchor pre-enable quality gate against bad direct config/sample values.
- This is module `2/5`; hardware and Android acceptance are deferred to module five because command shape, telemetry shape, app reconnect/install behavior, and hardware drivers did not change.

External baseline:
- ArduPilot pre-arm checks block arming/auto GPS-dependent modes on GPS glitch, missing 3D fix, and high HDOP rather than treating fix alone as sufficient: <https://ardupilot.org/copter/docs/common-prearm-safety-checks.html>.
- ArduPilot GPS glitch protection rejects position jumps against predicted motion and escalates sustained glitches in GPS-dependent modes: <https://ardupilot.org/copter/docs/gps-failsafe-glitch-protection.html>.
- PX4 position-loss failsafe treats stale aiding data and excessive horizontal position inaccuracy as invalid position evidence for modes that require position: <https://docs.px4.io/main/en/config/safety>.

Key outcomes:
- `evaluateGnssQuality()` now fails closed on invalid gate config: zero GPS age limit, invalid satellite threshold, non-finite/invalid HDOP limit, invalid position-jump limit, invalid speed/accel sanity limits, and negative required sentence count.
- Optional speed/accel sanity now rejects present-but-invalid motion samples (`NaN` or negative speed) instead of silently passing them.
- Non-finite jump distance now maps to `GPS_POSITION_JUMP`.
- Added direct native tests for invalid config, invalid optional motion samples, and non-finite jump evidence.
- Promoted the fail-closed GNSS quality gate rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because no phone-visible command/status schema changed.

Validation:
- `cd boatlock && platformio test -e native -f test_gnss_quality_gate -f test_runtime_gnss -f test_runtime_anchor_gate -f test_ble_command_handler` -> passed (`53/53`).
- `cd boatlock && platformio test -e native` -> passed (`278/278`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697645` bytes.
- `git diff --check` -> clean.

Self-review:
- This changes only bad-boundary behavior; normal valid GNSS samples and current settings-clamped production path continue to pass as before.
- Existing reason enums were reused to avoid widening the BLE/status surface.
- Remaining validation risk is live GNSS behavior under multipath/on-water motion; this module only hardens the deterministic gate.

Promote to skill:
- GNSS quality gates must reject invalid/non-finite config and sample evidence at the gate boundary; never rely only on upstream settings clamps for auto-mode admission.

### 2026-04-25 Stage 97: Runtime GNSS coordinate boundary validation

Scope:
- Continue the navigation/anchor batch with `RuntimeGnss`.
- Harden the live GNSS state boundary so raw hardware/parser or phone/app validity flags cannot place invalid coordinates into runtime fix state.
- This is module `3/5`; hardware and Android acceptance are deferred to module five because this does not touch pins, drivers, BLE command shape, phone install/reconnect, or UI behavior.

External baseline:
- ArduPilot GPS-dependent modes are blocked by pre-arm/pre-enable checks when GPS quality evidence is bad, including GPS glitch and high HDOP, instead of trusting a single validity flag: <https://ardupilot.org/copter/docs/common-prearm-safety-checks.html>.
- ArduPilot glitch protection compares new GPS positions against predicted motion before accepting them as good measurements: <https://ardupilot.org/copter/docs/gps-failsafe-glitch-protection.html>.
- PX4 position-loss failsafe invalidates position when aiding data is stale or position accuracy is outside acceptable limits for position-dependent modes: <https://docs.px4.io/main/en/config/safety>.

Key outcomes:
- Added `RuntimeGnss::validPosition()` for finite, range-checked coordinates and explicit `0/0` rejection.
- `applyHardwareFix()` now returns `INVALID_FIX`, clears live control fix, and resets GNSS fail reason to `NO_FIX` when coordinate input is invalid.
- `setPhoneFix()` now rejects invalid phone/app positions and clears active phone fallback if it was the live source.
- Added native coverage for invalid hardware coordinate rejection, `0/0` rejection, and invalid phone fallback clearing.
- Promoted the runtime GNSS coordinate boundary rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because command shape and phone-visible status schema did not change.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_gnss -f test_gnss_quality_gate -f test_runtime_buttons -f test_runtime_status -f test_ble_command_handler` -> passed (`62/62`).
- `cd boatlock && platformio test -e native` -> passed (`280/280`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697853` bytes.
- `git diff --check` -> clean.

Self-review:
- The change tightens only invalid-coordinate handling; normal valid hardware fixes, phone fallback telemetry, jump rejection, bearing cache, and heading correction remain unchanged.
- `INVALID_FIX` is internal to `RuntimeGnss`; no BLE/status enum expansion was introduced.
- Remaining risk is real GNSS multipath and receiver-specific behavior, which still requires bench/on-water observation outside native tests.

Promote to skill:
- Runtime GNSS source handlers must validate coordinate values before updating live fix state; raw parser/app valid flags are not sufficient at the control boundary.

### 2026-04-25 Stage 98: Anchor nudge fail-atomic projection

Scope:
- Continue the navigation/anchor batch with `RuntimeAnchorNudge`.
- Keep the existing fixed small jog behavior while making nudge projection fail-atomic and explicit around non-finite math.
- This is module `4/5`; hardware and Android acceptance are deferred to module five because command shape, phone UI behavior, pins, drivers, and BLE reconnect/install behavior did not change.

External baseline:
- Minn Kota Spot-Lock Jog moves the active spot in fixed 5 ft forward/back/left/right steps: <https://minnkota.johnsonoutdoors.com/us/learn/technology/spot-lock>.
- Minn Kota Advanced GPS docs require Spot-Lock to be engaged and a heading sensor for Spot-Lock Jog, then move the active location by fixed 5 ft increments: <https://minnkota-help.johnsonoutdoors.com/hc/en-us/articles/23607178243991-Using-Advanced-GPS-Navigation-Features-and-Manual-2023-present>.
- Garmin Force Current anchor-lock gesture controls jog the held position by 1.5 m / 5 ft in the selected direction: <https://www8.garmin.com/manuals/webhelp/GUID-91136105-7EB2-442E-8C25-7B4CF00FC466/EN-US/GUID-EBB86151-3235-4DF7-B466-988B71411E16.html>.

Key outcomes:
- Kept BoatLock's fixed `1.5 m` jog step and existing active-anchor/heading gates.
- `normalizeRuntimeAnchorNudgeBearing()` now returns safe `0` for non-finite input, matching the repo's bounded-angle convention.
- `normalizeRuntimeAnchorNudgeLon()` now explicitly returns `NaN` for non-finite longitude so validation rejects it.
- `projectRuntimeAnchorNudge()` now computes and validates the full next point before writing to the caller's target.
- Added native coverage for non-finite normalization and no target mutation on rejected projection.
- Promoted the nudge fail-atomic projection rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because the BLE command surface and app flow did not change.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_anchor_nudge -f test_ble_command_handler -f test_anchor_control -f test_runtime_ble_params` -> passed (`49/49`).
- `cd boatlock && platformio test -e native` -> passed (`281/281`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697897` bytes.
- `git diff --check` -> clean.

Self-review:
- This does not change jog distance, direction mapping, active-anchor requirements, or persistence semantics.
- The change only prevents rejected projection paths from exposing partially written target data.
- Remaining risk is live operator UX and real drift/current response after repeated jogs, which requires hardware/on-water validation.

Promote to skill:
- Anchor nudge/jog projection must be fail-atomic: compute, validate, then publish/persist; never partially update output on rejection.

### 2026-04-25 Stage 99: Runtime control input invalid distance gate

Scope:
- Close the five-module navigation/anchor batch with `RuntimeControlInputBuilder`.
- Make invalid distance evidence unavailable for auto-control instead of turning it into a fake zero-error control fix.
- This is module `5/5`; hardware acceptance and Android BLE smokes are scheduled immediately after commit/push.

External baseline:
- ArduPilot pre-arm checks block GPS-dependent modes on bad sensor data and GPS quality failures rather than treating partial evidence as usable: <https://ardupilot.org/copter/docs/common-prearm-safety-checks.html>.
- ArduPilot GPS glitch protection treats bad position updates as rejected measurements, not harmless zero-error updates: <https://ardupilot.org/copter/docs/gps-failsafe-glitch-protection.html>.
- PX4 position-loss failsafe triggers when the position estimate becomes invalid or position inaccuracy exceeds the allowed threshold for modes that need position: <https://docs.px4.io/main/en/config/safety>.

Key outcomes:
- `buildRuntimeControlState()` now computes distance validity once.
- In auto-control modes, invalid distance clears `input.controlGpsAvailable` and clamps `input.distanceM` to `0` so motion code goes quiet through the existing no-control-GPS path.
- Outside auto-control modes, the original `controlGpsAvailable` flag is preserved while invalid distance is still clamped away from downstream math.
- Added native coverage for invalid auto distance clearing the control-GPS flag and for manual mode preserving the flag.
- Promoted the invalid-distance-as-unavailable rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no new Android smoke mode needed because command/status schema did not change. Full Android smoke package runs with this module-five hardware batch.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_control_input_builder -f test_runtime_motion -f test_anchor_supervisor -f test_anchor_control_loop -f test_runtime_status` -> passed (`43/43`).
- `cd boatlock && platformio test -e native` -> passed (`282/282`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697929` bytes.
- `git diff --check` -> clean.

Self-review:
- This changes only invalid-distance auto behavior; valid anchor control input is unchanged.
- The safe effect is conservative: bad distance evidence now disables auto thrust/stepper via the same path as missing control GPS.
- Remaining validation risk is live sensor/dropout behavior, covered next by the scheduled hardware and Android batch.

Promote to skill:
- In auto/position-control modes, invalid distance/position evidence must disable the control input instead of masquerading as a zero-error measurement.

### 2026-04-25 Stage 100: Navigation batch hardware and Android acceptance

Scope:
- Run the scheduled hardware acceptance after the five-module navigation/anchor batch.
- Validate the exact firmware flashed to the `nh02` ESP32-S3 bench and the Android USB/BLE smoke suite through canonical wrappers.

Batch commits:
- `8d529db` `Bound anchor heading normalization`
- `bd10c3c` `Fail closed GNSS quality gate`
- `2767319` `Validate runtime GNSS coordinates`
- `a112946` `Make anchor nudge projection fail atomic`
- `26487e8` `Gate auto control on valid distance`

Validation:
- `./tools/hw/nh02/status.sh` -> RFC2217 service active, ESP32-S3 USB serial `98:88:E0:03:BA:5C`, listener on port `4000`.
- `./tools/hw/nh02/flash.sh` -> build success, flash success, app image write `698288` bytes, hard reset via RTS.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-nav-batch-60s.log --json-out /tmp/boatlock-nav-batch-60s.json` -> `[ACCEPT] PASS lines=70`.
- Acceptance matched BNO08x-RVC ready on `rx=12 baud=115200`, display ready, EEPROM loaded `ver=23`, security state `paired=0`, BLE init and advertising, stepper config, STOP button, fresh compass heading events, and GPS UART data.
- Error scan over `/tmp/boatlock-nav-batch-60s.log` for panic/assert/Guru/config save/CRC/GPS stale/no UART/compass loss/I2C/Arduino `[E]`/fail/error tokens -> no matches.
- `./tools/hw/nh02/android-status.sh` -> Xiaomi `220333QNY`, adb state `device`, USB serial `68b657f0`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> first install attempt hit `INSTALL_FAILED_USER_RESTRICTED`, canonical retry `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --status --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"status_stop_alert_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[EVENT] FAILSAFE_TRIGGERED reason=STOP_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --anchor --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"anchor_denied_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[EVENT] ANCHOR_OFF reason=BLE_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --sim --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"sim_run_abort_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[SIM] ABORTED"}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.

Self-review:
- The batch is locally covered, flashed, boot-accepted, and covered by the current Android BLE smoke suite.
- The one basic-smoke install policy hiccup was not a blocker because the canonical retry completed with `Success` and the wrapper returned a passing terminal result.
- This still does not prove powered thrust, on-water hold quality, or real GNSS fix behavior; the bench is reporting `NO_GPS` in Android smokes.

Promote to skill:
- No new workflow rule needed; the existing canonical hardware and Android acceptance path worked as documented.

### 2026-04-25 Stage 101: GPS UART watchdog timing floors

Scope:
- Start the watchdog/telemetry batch with `RuntimeGpsUart`.
- Keep the existing non-blocking no-data/stale/restart policy, but prevent zero/too-short timing config from causing warning or serial-restart thrash.
- This is module `1/5`; hardware and Android acceptance are deferred to module five because the live constants stay unchanged and the latest bench batch just passed.

External baseline:
- ESP-IDF UART guidance models UART reception through driver events and RX timeout handling, so UART activity/loss should be explicit state instead of blocking reads: <https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/uart.html>.
- ArduPilot pre-arm checks treat missing GPS data and unhealthy GPS as blockers that require physical/configuration diagnosis, not silent acceptance: <https://ardupilot.org/copter/docs/common-prearm-safety-checks.html>.
- Arduino's non-blocking timing pattern uses elapsed time checks rather than blocking delays for periodic work: <https://docs.arduino.cc/built-in-examples/digital/BlinkWithoutDelay/>.

Key outcomes:
- Added local minimum floors for `noDataWarnMs`, `staleRestartMs`, and `restartCooldownMs`.
- Zero config values no longer create immediate no-data warning or repeated serial restart loops.
- Preserved timestamp-zero and unsigned `millis()` rollover behavior.
- Updated native tests to cover floored zero config and rollover above the new floors.
- Promoted the GPS UART watchdog timing-floor rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because BLE command/status schema and phone behavior did not change.

Validation:
- First targeted run exposed a stale test expectation: the old rollover test used `100 ms`, now below the new safety floor. The test was corrected to verify rollover with intervals above the floor.
- `cd boatlock && platformio test -e native -f test_runtime_gps_uart -f test_runtime_gnss -f test_runtime_status -f test_anchor_control_loop` -> passed (`30/30`).
- `cd boatlock && platformio test -e native` -> passed (`283/283`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697929` bytes.

Self-review:
- Valid runtime constants remain unchanged, so normal GPS UART behavior should stay the same.
- The change protects direct module callers and future config wiring from zero/too-short intervals.
- Remaining validation risk is live GPS UART intermittent wiring/noise; scheduled batch hardware acceptance will cover boot/runtime logs after module five.

Promote to skill:
- UART/watchdog timing configs must have local floors so zero or corrupted intervals cannot create busy warning/restart loops.

### 2026-04-25 Stage 102: Runtime telemetry cadence zero-interval floor

Scope:
- Continue the watchdog/telemetry batch with `RuntimeTelemetryCadence`.
- Prevent UI/BLE/compass cadence callers from creating same-timestamp bursts through interval `0`.
- This is module `2/5`; hardware and Android acceptance are deferred to module five because live production intervals stay non-zero and no phone-visible schema changed.

External baseline:
- Arduino's non-blocking timing pattern is based on elapsed-time checks instead of blocking delays or unbounded loops: <https://docs.arduino.cc/built-in-examples/digital/BlinkWithoutDelay/>.
- ESP-IDF UART/event guidance reinforces event/cadence-driven processing rather than blocking or busy polling loops: <https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/uart.html>.
- PX4/ArduPilot safety patterns used elsewhere in this repo treat invalid timing/config values as values to bound locally before runtime logic uses them.

Key outcomes:
- Added `RuntimeTelemetryCadence::kMinIntervalMs = 1`.
- `shouldRun()` now floors interval `0` to `1 ms` instead of returning `true` repeatedly for the same timestamp.
- Preserved independent UI/BLE/compass timers and unsigned rollover behavior.
- Updated cadence tests for zero interval floor semantics.
- Promoted the cadence floor rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because BLE payload/schema and app behavior did not change.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_telemetry_cadence -f test_runtime_ble_live_frame -f test_runtime_status -f test_runtime_compass_health` -> passed (`21/21`).
- `cd boatlock && platformio test -e native` -> passed (`283/283`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697937` bytes.

Self-review:
- Valid production intervals are unchanged; the change only hardens bad direct input.
- The minimum remains intentionally tiny (`1 ms`) to avoid changing normal scheduling policy while removing same-timestamp burst behavior.
- Remaining validation risk is live BLE/display cadence under noisy runtime load; scheduled batch hardware acceptance will cover boot and smoke behavior after module five.

Promote to skill:
- Cadence helpers must floor zero intervals instead of allowing same-timestamp bursts.

### 2026-04-25 Stage 103: Runtime status reason token hygiene

Scope:
- Continue the watchdog/telemetry batch with `RuntimeStatus`.
- Keep the existing `status|mode|statusReasons` schema, but harden `statusReasons` against malformed future reason strings.
- This is module `3/5`; hardware and Android acceptance are deferred to module five because normal reason constants and BLE schema stay unchanged.

External baseline:
- Signal K notifications model alarms/alerts as structured state and keys under a notifications tree, not as free-form delimited text: <https://signalk.org/specification/1.7.0/doc/notifications.html>.
- Signal K's anchor alarm guide keeps monitoring authoritative on the boat/server and shows status/current-data indicators in the client: <https://demo.signalk.org/documentation/Guides/Anchor_Alarm.html>.

Key outcomes:
- Added a bounded token sanitizer for runtime status reasons.
- Comma, whitespace, newline, tab, and other unsupported characters are neutralized before a reason enters the comma-separated BLE/app field.
- Reason tokens are capped at `32` characters to keep the status payload deterministic.
- Existing normal reason constants and severity behavior are unchanged.
- Promoted the status-token hygiene rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because phone-visible schema and normal emitted constants did not change; malformed/future reason handling is covered by native tests.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_status -f test_runtime_ble_live_frame -f test_runtime_ble_params -f test_ble_command_handler` -> passed (`45/45`).
- `cd boatlock && platformio test -e native` -> passed (`284/284`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `698069` bytes.
- `git diff --check` -> clean.

Self-review:
- This does not change normal emitted status constants, so app parsing should stay behaviorally unchanged.
- The sanitizer is intentionally narrow: it protects the existing CSV contract instead of adding a new protocol field mid-batch.
- Remaining risk is any future reason that intentionally needs punctuation; that should be a protocol/schema decision, not an accidental runtime string.

### 2026-04-25 Stage 104: BLE live-frame reason mapping table

Scope:
- Continue the watchdog/telemetry batch with `RuntimeBleLiveFrame`.
- Keep the fixed 70-byte binary frame and field layout unchanged while reducing duplicated reason-to-bit branching.
- This is module `4/5`; hardware and Android acceptance are deferred to module five because the binary schema and normal emitted bytes stay unchanged.

External baseline:
- Bluetooth GATT notifications carry characteristic values without ATT-layer acknowledgement, so the application frame should stay compact, deterministic, and self-validated before notify: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.
- Android exposes notification/read data as byte arrays for the characteristic value, reinforcing the current fixed binary frame instead of JSON/string parsing on the live path: <https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic>.

Key outcomes:
- Added `kRuntimeBleLiveFrameSize` and used it for encoder reserve and frame-size tests.
- Replaced duplicated `if token then flag` branches with one explicit token-to-bit table and a loop.
- Added a native test that rejects substring/prefix lookalikes while preserving exact token matches with whitespace trimming.
- Promoted the live-frame mapping rule into `skills/boatlock/references/ble-ui.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no new Android smoke path is needed because the frame version, byte layout, size, decoder contract, and normal bytes are unchanged; the full Android smoke suite remains scheduled after module five.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_live_frame -f test_runtime_status -f test_runtime_ble_params -f test_ble_command_handler` -> passed (`46/46`).
- `cd boatlock_ui && flutter test test/ble_live_frame_test.dart` -> passed.
- `cd boatlock && platformio test -e native` -> passed (`285/285`).
- `cd boatlock_ui && flutter test` -> passed (`32/32`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697797` bytes.
- `git diff --check` -> clean.

Self-review:
- The frame layout stayed fixed at `70` bytes; no decoder or protocol version change was needed.
- The table removes duplicated branch logic and makes future reason additions reviewable in one place.
- Remaining risk is cross-language drift between firmware and Flutter reason tables; current Flutter tests cover decode, and full Android smoke is scheduled after module five.

### 2026-04-25 Stage 105: BLE telemetry snapshot integer guards

Scope:
- Finish the watchdog/telemetry batch with `RuntimeBleParams`.
- Keep the live-frame schema unchanged while preventing bad float settings from being cast directly into `uint16_t` telemetry fields.
- This is module `5/5`; after local validation this batch requires `nh02` flash/acceptance and Android BLE smokes.

External baseline:
- Bluetooth GATT characteristic notifications carry application-provided byte values without a protocol-level semantic guard, so the firmware must validate fixed-frame values before notify: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.
- Android exposes characteristic values as byte arrays and updates its cached value from reads/notifications, so corrupt packed values become app state unless bounded at the source: <https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic>.
- C++ floating-point to integer conversion is undefined when the truncated value cannot fit the destination type, so firmware should saturate before integer casts: <https://www.cppreference.com/w/cpp/language/implicit_cast.html>.

Key outcomes:
- Added `runtimeBleTelemetryU16()` for direct integer fields in the telemetry snapshot.
- `StepSpr`, `StepMaxSpd`, and `StepAccel` now map non-finite, negative, and zero values to `0`, preserve normal in-range integer settings, and saturate above `UINT16_MAX`.
- Added native tests for `NaN`, infinity, negative, zero, fractional in-range, and over-range values.
- Promoted the telemetry integer-guard rule into `skills/boatlock/references/ble-ui.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no new smoke path is needed because the frame schema and normal values are unchanged; the scheduled post-module-five Android suite will cover live BLE delivery on hardware.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_params -f test_runtime_ble_live_frame -f test_runtime_status -f test_settings` -> passed (`30/30`).
- `cd boatlock_ui && flutter test test/ble_live_frame_test.dart test/ble_boatlock_test.dart` -> passed (`14/14`).
- `cd boatlock && platformio test -e native` -> passed (`286/286`).
- `cd boatlock_ui && flutter test` -> passed (`32/32`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697861` bytes.
- `./tools/hw/nh02/status.sh` -> RFC2217 active, ESP32-S3 USB serial `98:88:E0:03:BA:5C`, port `4000`.
- `./tools/hw/nh02/flash.sh` -> build success, flash success, app image write `698224` bytes, hard reset via RTS.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-watchdog-telemetry-batch-60s.log --json-out /tmp/boatlock-watchdog-telemetry-batch-60s.json` -> `[ACCEPT] PASS lines=78`.
- Acceptance matched BNO08x-RVC ready on `rx=12 baud=115200`, fresh compass heading events, display ready, EEPROM loaded `ver=23`, security state `paired=0`, BLE init and advertising, stepper config, STOP button, and GPS UART data.
- Error scan over `/tmp/boatlock-watchdog-telemetry-batch-60s.log` for panic/assert/Guru/config save/CRC/GPS stale/no UART/compass loss/I2C/Arduino `[E]`/fail/error tokens -> no matches.
- `./tools/hw/nh02/android-status.sh` -> Xiaomi `220333QNY`, adb state `device`, USB serial `68b657f0`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> first install attempt hit `INSTALL_FAILED_USER_RESTRICTED`, canonical retry `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --status --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"status_stop_alert_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[EVENT] FAILSAFE_TRIGGERED reason=STOP_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --anchor --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"anchor_denied_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[EVENT] ANCHOR_OFF reason=BLE_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --sim --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"sim_run_abort_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[SIM] ABORTED"}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.

Self-review:
- The batch passed local unit/UI tests, was flashed to the bench, boot-accepted, and passed the current Android BLE smoke suite.
- The first basic-smoke install still shows the known MIUI first-attempt `USER_RESTRICTED` prompt path, but the canonical retry installed the exact APK and the wrapper returned a passing terminal result.
- The captured hardware log had one cosmetic truncation-looking disconnect line (`Reas9`), but no error/fail pattern and no acceptance miss; treat this as log formatting debt, not a runtime blocker.
- Remaining validation gaps are real GNSS fix behavior, powered actuator load, and on-water hold quality; this bench still reports `NO_GPS` in Android smokes.

### 2026-04-25 Stage 106: BLE heartbeat command log suppression

Scope:
- Start the diagnostics/readability batch with `BLEBoatLock` command logging.
- Reduce serial/BLE diagnostic noise by suppressing high-frequency `HEARTBEAT` command log lines while preserving state-changing command and event logs.
- This is module `1/5`; hardware and Android acceptance are deferred to module five because behavior and protocol are unchanged.

External baseline:
- ESP-IDF logging guidance separates levels from error through verbose and calls out performance/log-processing cost, so frequent non-action traffic should not live in normal runtime logs: <https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32/api-reference/system/log.html>.
- ESP-IDF also notes that logs are first formatted into a buffer and then sent to UART with thread-safe behavior; BoatLock should keep diagnostic lines compact enough that the bench capture remains useful.

Key outcomes:
- Added `RuntimeBleCommandLog.h` with a single rule: suppress `HEARTBEAT`, keep all other commands logged.
- `BLEBoatLock::CmdCallbacks::onWrite()` now still handles every command, but only logs operator/service commands that carry diagnostic value.
- Added native tests for skipped heartbeat and retained `STREAM_START`, `MANUAL_SET`, and `STOP` logs.
- Promoted the heartbeat-log suppression rule into `skills/boatlock/references/ble-ui.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because command behavior, BLE schema, reconnect, install, and UI behavior are unchanged; scheduled post-module-five hardware acceptance will verify log readability on the bench.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_command_log -f test_ble_command_handler -f test_runtime_ble_log_text` -> passed (`38/38`).
- `cd boatlock && platformio test -e native` -> passed (`288/288`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697885` bytes.
- `git diff --check` -> clean.

Self-review:
- This removes normal heartbeat log noise without changing heartbeat handling or link activity semantics.
- The helper keeps the rule explicit and directly testable instead of burying a string comparison inside a callback.
- Remaining risk is that the previous `Reas9` line may also involve serial transport byte loss; scheduled module-five hardware acceptance will verify whether lower log volume improves capture readability.

### 2026-04-25 Stage 107: Logger bounded write helper

Scope:
- Continue the diagnostics/readability batch with `Logger`.
- Keep log content and BLE forwarding policy the same, but make serial emission length-bounded from the `vsnprintf` result instead of relying on C-string scanning.
- This is module `2/5`; hardware and Android acceptance are deferred to module five because runtime behavior and protocols are unchanged.

External baseline:
- ESP-IDF logging guidance says log strings are first written into a memory buffer and then sent to UART, and that callbacks can be invoked from multiple thread contexts: <https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32/api-reference/system/log.html>.
- The same guidance treats high-volume logging as a performance concern, so the logger should keep deterministic bounded work in the normal path.

Key outcomes:
- Added `RuntimeLogText.h` with testable helpers for bounded formatted length and the existing BLE-forwarding filter.
- `logMessage()` now checks `vsnprintf` result, writes the exact bounded byte count with `Serial.write()`, and returns on formatting failure or empty output.
- The existing rule that `[BLE]` lines are not forwarded back over BLE logs is preserved through the helper.
- Promoted the bounded logger-write rule into `skills/boatlock/references/firmware.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because BLE schema, command behavior, reconnect, install, and UI behavior are unchanged.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_log_text -f test_runtime_ble_log_text -f test_runtime_ble_command_log` -> passed (`8/8`).
- `cd boatlock && platformio test -e native` -> passed (`291/291`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `697893` bytes.

Self-review:
- This keeps emitted log text and BLE forwarding semantics stable while bounding the serial write path by `vsnprintf`'s contract.
- The helper gives native coverage for formatting failures, truncation, and the `[BLE]` forwarding filter.
- Remaining risk is live serial transport byte loss or line interleaving under bench load; module-five hardware acceptance will check the canonical log path.

### 2026-04-25 Stage 108: BLE connection log formatter

Scope:
- Continue the diagnostics/readability batch with `BLEBoatLock` connection/disconnection logs.
- Keep BLE behavior unchanged while replacing long free-text connect/disconnect lines with shorter key=value formatter output.
- This is module `3/5`; hardware and Android acceptance are deferred to module five because transport behavior, protocol, and app parsing are unchanged.

External baseline:
- NimBLE-Arduino server callbacks expose `onConnect(...)` and `onDisconnect(..., int reason)` with `NimBLEConnInfo`, so connection diagnostics should preserve callback metadata: <https://h2zero.github.io/NimBLE-Arduino/class_nim_b_l_e_server_callbacks.html>.
- Apache NimBLE return codes are partitioned into layer ranges such as `0x100`, `0x200`, `0x300`, `0x400`, and `0x500`, so logging only decimal disconnect reasons makes field triage harder: <https://mynewt.incubator.apache.org/latest/network/ble_hs/ble_hs_return_codes.html>.
- Espressif BLE troubleshooting likewise describes NimBLE and HCI error code ranges, reinforcing raw hex reason logging for bench diagnostics: <https://docs.espressif.com/projects/esp-techpedia/en/latest/esp-friends/advanced-development/ble-application-note/troubleshooting.html>.

Key outcomes:
- Added `RuntimeBleConnectionLog.h` with bounded formatters for connect and disconnect lines.
- Connect logs now use `[BLE] connect addr=... mtu=... int=... timeout=...`.
- Disconnect logs now use `[BLE] disconnect addr=... reason=... hex=0x... clients=...`.
- Added native tests for normal formatting, unknown address fallback, reason decimal+hex retention, truncation, and missing buffers.
- Promoted the connection-log formatting rule into `skills/boatlock/references/ble-ui.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because BLE schema, commands, reconnect logic, install path, and UI behavior are unchanged; module-five Android smokes will exercise real connect/disconnect on hardware.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_connection_log -f test_runtime_log_text -f test_ble_advertising_watchdog` -> passed (`13/13`).
- `cd boatlock && platformio test -e native` -> passed (`295/295`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `698085` bytes.

Self-review:
- This keeps BLE connection behavior untouched and only changes the diagnostic text shape.
- The formatter is bounded, native-tested, and preserves raw reason code information needed to diagnose layered NimBLE/HCI disconnects.
- Remaining risk is whether real bench serial capture still drops bytes under connect/disconnect churn; module-five hardware acceptance will check the canonical log path.

### 2026-04-25 Stage 109: BLE log queue bounded payload copy

Scope:
- Continue the diagnostics/readability batch with the BLE log queue payload path.
- Keep log delivery semantics unchanged while removing `strlen()` from the queue enqueue path.
- This is module `4/5`; hardware and Android acceptance are deferred to module five because BLE schema, command behavior, and app parsing are unchanged.

External baseline:
- Bluetooth GATT notifications send characteristic values as attribute-value bytes, not C strings, so firmware should carry explicit lengths through the BLE-facing path: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.
- SEI CERT STR32-C warns against passing a non-null-terminated character sequence to string functions because termination errors can cause overflow or information disclosure: <https://wiki.sei.cmu.edu/confluence/pages/viewpage.action?pageId=554434583>.

Key outcomes:
- Added `runtimeBlePrepareLogPayload()` to zero-fill a queue slot and copy at most `slotSize - 1` bytes from a bounded source scan.
- `BLEBoatLock::enqueueLogLine()` now uses the helper instead of `strlen()` plus manual copy.
- Extended native BLE log-text tests for unterminated source input and null input destination clearing.
- Promoted the bounded log queue payload rule into `skills/boatlock/references/ble-ui.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because phone-visible schema and behavior are unchanged; module-five Android smokes will validate real log delivery and reconnect.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_log_text -f test_runtime_log_text -f test_runtime_ble_connection_log` -> passed (`12/12`).
- `cd boatlock && platformio test -e native` -> passed (`297/297`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `698097` bytes.

Self-review:
- This removes one unbounded source scan from the BLE log path without changing queued value format or notify cadence.
- The helper zero-fills every slot, so dropped/empty/null inputs cannot leak stale payload bytes into later log notifications.
- Remaining risk is live queue pressure under Android reconnect churn; module-five hardware acceptance and Android smokes will exercise the canonical device path.

### 2026-04-25 Stage 110: BLE live notify packet length guard

Scope:
- Finish the diagnostics/readability batch with the BLE live data notify packet path.
- Keep the v2 live-frame schema unchanged while preventing `processQueuedData()` from constructing a characteristic value from an invalid internal packet length.
- This is module `5/5`; after local validation this batch requires `nh02` flash/acceptance and Android BLE smokes.

External baseline:
- Bluetooth GATT notifications carry an attribute value byte sequence, so the server-side application must choose a valid characteristic value length before notify: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.
- Android exposes characteristic values as `byte[]` cached from read or notification updates, so malformed live-frame length at the source becomes app state unless dropped before notify: <https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic>.

Key outcomes:
- Added `RuntimeBleDataPacket.h` with a small guard for valid nonzero notify payload lengths.
- `BLEBoatLock::processQueuedData()` now drops zero-length or oversized internal packets before building the characteristic value.
- Added native tests for accepted normal/max lengths and rejected empty/oversized values.
- Promoted the live-notify packet-length rule into `skills/boatlock/references/ble-ui.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: this touches live data delivery, so the scheduled post-module-five Android smoke suite must be run in this stage.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_data_packet -f test_runtime_ble_live_frame -f test_runtime_ble_params` -> passed (`10/10`).
- `cd boatlock_ui && flutter test test/ble_live_frame_test.dart test/ble_boatlock_test.dart` -> passed (`14/14`).
- `cd boatlock && platformio test -e native` -> passed (`299/299`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `698105` bytes.
- `./tools/hw/nh02/status.sh` -> RFC2217 active, ESP32-S3 USB serial `98:88:E0:03:BA:5C`, port `4000`.
- `./tools/hw/nh02/flash.sh` -> build success, flash success, app image write `698464` bytes, hard reset via RTS.
- `./tools/hw/nh02/acceptance.sh --seconds 60 --log-out /tmp/boatlock-diagnostics-batch-60s.log --json-out /tmp/boatlock-diagnostics-batch-60s.json` -> `[ACCEPT] PASS lines=29`.
- Acceptance matched BNO08x-RVC ready on `rx=12 baud=115200`, fresh compass heading events, display ready, EEPROM loaded `ver=23`, security state `paired=0`, BLE init and advertising, stepper config, STOP button, and GPS UART data.
- Error scan over `/tmp/boatlock-diagnostics-batch-60s.log` for panic/assert/Guru/config save/CRC/GPS stale/no UART/compass loss/I2C/Arduino `[E]`/fail/error tokens -> no matches.
- Acceptance log confirmed new concise BLE diagnostics, including `[BLE] connect addr=...` and later stream/subscribe events.
- `./tools/hw/nh02/android-status.sh` -> Xiaomi `220333QNY`, adb state `device`, USB serial `68b657f0`.
- `./tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","rssi":-36}`.
- `./tools/hw/nh02/android-run-smoke.sh --status --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"status_stop_alert_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[EVENT] FAILSAFE_TRIGGERED reason=STOP_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"manual_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --anchor --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"anchor_denied_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[EVENT] ANCHOR_OFF reason=BLE_CMD"}`.
- `./tools/hw/nh02/android-run-smoke.sh --sim --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"sim_run_abort_roundtrip","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","lastDeviceLog":"[SIM] ABORTED"}`.
- `./tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.
- `./tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130` -> exact install `Success`, final `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_after_reconnect","mode":"IDLE","status":"WARN","statusReasons":"NO_GPS"}`.

Self-review:
- Local tests cover the guard boundary and the normal frame encoder/decoder contract.
- The guard drops only invalid internal packets; the normal live frame remains the same binary v2 payload.
- The diagnostics/readability batch passed local unit/UI tests, was flashed to `nh02`, boot-accepted, and passed the current Android BLE smoke suite with exact APK installs.
- The prior cosmetic `Reas9` disconnect artifact did not reappear in the 60s acceptance log; connection logs now preserve address and raw reason fields in shorter key=value form.
- Remaining validation gaps are real GNSS fix behavior, powered actuator load, multi-controller arbitration, and on-water hold quality; this bench still reports `NO_GPS` in Android smokes.

### 2026-04-25 Stage 111: Module cadence changed to fifteen

Scope:
- Capture the user's corrected default cadence before continuing module refactors.

Key outcomes:
- Default refactor batch size is now fifteen modules.
- Per-module requirements remain unchanged: targeted external baseline, smallest useful refactor, local tests, phone-smoke decision, `WORKLOG.md` self-review, commit, and push.
- Scheduled `nh02` flash, hardware acceptance, serial log scan, and Android BLE smoke suite now run after every fifteenth module.
- High-risk changes still trigger immediate hardware or Android acceptance instead of waiting for the fifteenth module.

Validation:
- Documentation-only workflow change; `git diff --check` is the relevant local validation.

Self-review:
- The longer cadence reduces bench churn for low-risk local-only refactors.
- The main risk is a larger local-only window, so the immediate hardware gate for drivers, pinout, deploy/debug, actuator safety, BLE reconnect/install, and other unbounded paths must stay strict.

### 2026-04-25 Stage 112: RuntimeControl deterministic mode priority

Scope:
- Start the next refactor batch with module `1/15`: runtime mode arbitration.
- Keep behavior unchanged while making the priority contract explicit and fully tested.

External baseline:
- W3C SCXML defines conflicting transition selection through explicit priority and an optimal conflict-free transition set, which maps to BoatLock's need for deterministic mode arbitration instead of incidental state resolution: <https://www.w3.org/TR/scxml/>.

Key outcomes:
- `RuntimeControl.h` no longer includes `Arduino.h`; it only needs standard `math.h` and `stdint.h`.
- Replaced partial priority tests with an exhaustive 16-case table for `SIM > MANUAL > ANCHOR > SAFE_HOLD > IDLE`.
- Added a fail-closed label test for unknown `CoreMode` values returning `IDLE`.
- Promoted the deterministic mode arbitration rule into `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because no BLE payload, command, reconnect, install, UI, or phone-visible behavior changed.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_control -f test_runtime_control_input_builder` -> passed (`11/11`).
- `cd boatlock && platformio test -e native` -> passed (`297/297`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `698105` bytes.

Self-review:
- This is intentionally a small refactor: changing arbitration code would add risk without simplifying the already-clear priority chain.
- The useful safety gain is test coverage around every conflicting mode combination, especially manual/anchor/hold and sim/manual overlap.
- Hardware acceptance is not required for this module under the fifteen-module cadence because no hardware driver, pinout, actuator, BLE reconnect/install, or phone-visible protocol behavior changed.

### 2026-04-25 Stage 113: RuntimeControlInputBuilder compass evidence sanitation

Scope:
- Continue the refactor batch with module `2/15`: runtime control input boundary.
- Keep the builder as the single place that converts raw sensor/mode evidence into motion input.

External baseline:
- SEI CERT FLP04-C recommends checking floating-point inputs for exceptional values before using them because NaN and infinity can propagate into later calculations and corrupt behavior: <https://wiki.sei.cmu.edu/confluence/display/c/FLP04-C.+Check+floating-point+inputs+for+exceptional+values>.

Key outcomes:
- `RuntimeControlInputBuilder` now sanitizes BNO rotation-vector accuracy before motion control: non-finite or negative values become unknown (`NAN`).
- Out-of-range BNO quality values now map to `0`, the worst quality bucket, instead of accidentally behaving like high quality.
- Added native tests for valid quality preservation and invalid high/low quality fail-closed behavior.
- Promoted the sensor-evidence sanitation rule into `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because no phone-visible BLE, command, reconnect, install, UI, or telemetry schema behavior changed.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_control_input_builder -f test_runtime_motion` -> passed (`16/16`).
- `cd boatlock && platformio test -e native` -> passed (`298/298`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `698105` bytes.

Self-review:
- This is a small semantic hardening at a module boundary, not a runtime behavior rewrite.
- The change only affects corrupted/impossible compass metadata; normal valid BNO inputs are preserved.
- Hardware acceptance is not required for this module under the fifteen-module cadence because this does not change drivers, pinout, deploy/debug wrappers, BLE reconnect/install, or phone-visible behavior.

### 2026-04-25 Stage 114: RuntimeMotion stop-failsafe single owner

Scope:
- Continue the refactor batch with module `3/15`: runtime motion failsafe application.
- Keep stop-style failsafes in the shared stop path instead of duplicating anchor disable steps.

External baseline:
- ArduPilot Rover failsafes map communication/RC failures to deterministic safe actions such as Hold, and operator action is required to retake Manual after recovery: <https://ardupilot.org/rover/docs/rover-failsafes.html>.
- Existing BoatLock storage baseline still applies: settings saves are dirty-state guarded and flash writes should not be amplified without a semantic change.

Key outcomes:
- Removed the redundant `AnchorEnabled=0`/`save()` pre-clear before `stopAllMotionNow()`.
- Stop-style supervisor failsafes now let the shared stop path see the previous anchor state and emit `[EVENT] ANCHOR_OFF reason=STOP` before the final failsafe event.
- Extended runtime motion tests with a bounded log capture to assert both anchor-off and failsafe log events.
- Promoted the single-owner stop-failsafe rule into `skills/boatlock/references/firmware.md`.
- Phone-smoke decision: no Android smoke added or run for this module because no phone-visible BLE command, reconnect, install, UI, or telemetry schema changed.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_motion -f test_anchor_supervisor` -> passed (`27/27`).
- `cd boatlock && platformio test -e native` -> passed (`298/298`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `698089` bytes.

Self-review:
- This reduces duplicated state mutation and makes diagnostics more accurate without changing the final safe state.
- The behavioral surface is still safety-relevant, but it is log/state sequencing inside an already-tested STOP path; local tests should bound it unless later batches touch live actuator behavior.
- Hardware acceptance is not required yet under the fifteen-module cadence because this change does not alter drivers, pinout, deploy/debug wrappers, BLE reconnect/install, or live actuator output math.

### 2026-04-25 Stage 115: RuntimeSupervisorPolicy floor constants

Scope:
- Continue the refactor batch with module `4/15`: supervisor config/input policy.
- Remove duplicated safety-floor literals between `RuntimeSupervisorPolicy` and `AnchorSupervisor`.

External baseline:
- ArduPilot pre-arm checks treat configuration and bad sensor data as movement-blocking safety concerns, and Rover failsafes use explicit timeout-triggered safe actions such as Hold: <https://ardupilot.org/rover/docs/common-prearm-safety-checks.html> and <https://ardupilot.org/rover/docs/rover-failsafes.html>.

Key outcomes:
- `RuntimeSupervisorPolicy` now derives timeout floors from `AnchorSupervisor::kMin*` constants.
- Replaced the Arduino `constrain()` dependency in `runtimeSupervisorFiniteClamp()` with explicit finite/low/high checks.
- Promoted the single-source supervisor floor rule into `skills/boatlock/references/firmware.md`.
- Phone-smoke decision: no Android smoke added or run for this module because no BLE command, phone-visible status field, reconnect/install, UI, or telemetry schema changed.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_supervisor_policy -f test_anchor_supervisor` -> passed (`22/22`).
- `cd boatlock && platformio test -e native` -> passed (`298/298`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `698069` bytes.

Self-review:
- This is KISS cleanup with safety value: the builder and core supervisor can no longer drift on minimum timeout values.
- Behavior should be unchanged for all current valid and invalid settings; existing clamp/non-finite tests should prove that.
- Hardware acceptance is not required yet under the fifteen-module cadence because this does not alter drivers, pinout, deploy/debug wrappers, actuator output behavior, BLE reconnect/install, or phone-visible behavior.

### 2026-04-25 Stage 116: AnchorSupervisor core dependency cleanup

Scope:
- Continue the refactor batch with module `5/15`: core anchor supervisor state machine.
- Keep supervisor behavior unchanged while reducing dependencies and covering unknown diagnostic reasons.

External baseline:
- ArduPilot Rover pre-arm/failsafe docs emphasize explicit failure causes and safe actions instead of silent operation through bad configuration or bad sensor data: <https://ardupilot.org/rover/docs/common-prearm-safety-checks.html> and <https://ardupilot.org/rover/docs/rover-failsafes.html>.

Key outcomes:
- `AnchorSupervisor.h` no longer includes `Arduino.h`; it only needs `stdint.h` for enum storage.
- Added a direct native test that unknown `AnchorSupervisor::Reason` values fail closed to `"NONE"`.
- Phone-smoke decision: no Android smoke added or run for this module because no BLE command, phone-visible status field, reconnect/install, UI, or telemetry schema changed.

Validation:
- `cd boatlock && platformio test -e native -f test_anchor_supervisor -f test_runtime_supervisor_policy` -> passed (`23/23`).
- `cd boatlock && platformio test -e native` -> passed (`299/299`).
- `cd boatlock && platformio run -e esp32s3` -> success, flash size `698069` bytes.

Self-review:
- This keeps the supervisor portable and easier to unit-test without changing decision priority.
- The added reason-string test protects diagnostic stability for invalid enum values.
- Hardware acceptance is not required at module five under the updated fifteen-module cadence because this is a core-header/test-only cleanup with no live hardware, BLE reconnect/install, or phone-visible behavior change.

### 2026-04-25 Stage 117: RuntimeStatus severity predicates

Scope:
- Continue the refactor batch with module `6/15`: runtime status summary and reason severity.
- Keep the wire values unchanged while making ALERT and WARN predicates separate.

External baseline:
- Signal K models alarm states as ordered severities (`normal`, `alert`, `warn`, `alarm`, `emergency`) and expects consumers to react consistently to the state metadata: <https://signalk.org/specification/1.7.0/doc/data_model_metadata.html> and <https://signalk.org/specification/1.5.0/doc/notifications.html>.

Key outcomes:
- Added `runtimeStatusInputHasAlert()` for explicit `HOLD`, `DRIFT_FAIL`, and active failsafe reason handling.
- Removed duplicate failsafe handling from the WARN helper; summary now checks ALERT first, then WARN.
- Added native tests proving `DRIFT_FAIL` and failsafe reasons produce `ALERT`.
- Promoted the separated severity-predicate rule into `skills/boatlock/references/firmware.md`.
- Phone-smoke decision: no Android smoke added or run for this module because `status`, `statusReasons`, BLE flags, commands, reconnect/install, and UI behavior are unchanged for existing inputs.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_status -f test_runtime_ble_live_frame` -> PASS (`11/11`).
- `cd boatlock && platformio test -e native` -> PASS (`300/300`).
- `cd boatlock && pio run -e esp32s3` -> PASS (`698097` bytes flash).

Self-review:
- This is a readability/safety refactor: output compatibility is preserved, but the code now makes severity ownership explicit.
- Remaining risk is app presentation of WARN/ALERT, which is outside this firmware-only module and already covered by existing Flutter status tests.

### 2026-04-25 Stage 118: BLE live-frame exact length

Scope:
- Continue the refactor batch with module `7/15`: Flutter live-frame decoder contract for characteristic `34cd`.
- Remove padded-frame tolerance from the unreleased app; the live characteristic is a fixed 70-byte binary value, not a stream.

External baseline:
- Bluetooth Core GATT defines characteristic values as the value carried by read/notify procedures and notifications as characteristic-value updates without an ATT acknowledgment; the Client Characteristic Configuration descriptor enables notifications: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.
- Android BLE docs deliver characteristic changes to the app through `onCharacteristicChanged()` after notifications are enabled: <https://developer.android.com/develop/connectivity/bluetooth/ble/transfer-ble-data> and <https://developer.android.com/reference/android/bluetooth/BluetoothGatt#setCharacteristicNotification(android.bluetooth.BluetoothGattCharacteristic,%20boolean)>.

Key outcomes:
- `decodeBoatLockLiveFrame()` now requires exactly 70 bytes instead of accepting longer padded payloads.
- Added a Flutter unit test for padded-frame rejection.
- Promoted the exact-length live-frame rule into `docs/BLE_PROTOCOL.md`, `skills/boatlock/references/ble-ui.md`, and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: Android BLE smoke is required for this module because the phone-visible decoder gate changed.

Validation:
- `cd boatlock_ui && flutter test test/ble_live_frame_test.dart test/ble_smoke_logic_test.dart` -> PASS (`8/8`).
- `cd boatlock_ui && flutter test` -> PASS (`33/33`).
- `tools/hw/nh02/android-status.sh` -> phone visible as ADB device `68b657f0`, model `220333QNY`.
- `tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> exact install `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","dataEvents":1,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","rssi":-34,...}`.

Self-review:
- This intentionally removes compatibility tolerance before alpha release; it should expose producer/padding bugs immediately instead of hiding protocol drift.
- The remaining risk is real-phone BLE stack behavior; local decoder tests alone are not enough, so the module must include Android smoke.

### 2026-04-25 Stage 119: RuntimeBleParams quality ordinals

Scope:
- Continue the refactor batch with module `8/15`: runtime-to-BLE telemetry snapshot quality fields.
- Keep the binary frame layout unchanged while preventing invalid quality ordinals from looking like high-confidence evidence.

External baseline:
- The BNO08X datasheet defines sensor report Status bits `1:0` as four accuracy states: `0` unreliable, `1` low, `2` medium, `3` high: <https://docs.sparkfun.com/SparkFun_VR_IMU_Breakout_BNO086_QWIIC/assets/component_documentation/BNO080_085-Datasheet_v1.16.pdf>.
- Signal K's model guidance treats structured/object and enum-like data as semantically bounded values for consumers; producers should not rely on clients guessing meaning outside the schema: <https://signalk.org/specification/1.7.0/doc/data_model.html>.

Key outcomes:
- Added `runtimeBleTelemetryQuality()` for bounded quality ordinals.
- Compass, magnetometer, and gyroscope quality now publish only `0..3`; invalid values publish as `0`.
- GNSS quality now publishes only `0..2`; invalid values publish as `0`.
- Added native tests for negative and too-high quality ordinals.
- Promoted the bounded-quality rule into `skills/boatlock/references/ble-ui.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because valid wire values, frame layout, commands, reconnect/install, and UI behavior are unchanged; invalid values are hardened to lower confidence.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_params -f test_runtime_ble_live_frame` -> PASS (`9/9`).
- `cd boatlock && platformio test -e native` -> PASS (`301/301`).
- `cd boatlock && pio run -e esp32s3` -> PASS (`698101` bytes flash).

Self-review:
- This is a small fail-safe hardening cut: bad producer values can no longer appear as high confidence in the phone UI or future tooling.
- Remaining risk is that GNSS quality is not currently displayed by Flutter; the value is still part of the binary frame and should stay bounded for future use.

### 2026-04-25 Stage 120: BLE data packet exact producer length

Scope:
- Continue the refactor batch with module `9/15`: firmware live-data queue boundary before GATT `setValue()`.
- Align the producer-side notify path with the exact 70-byte live-frame contract introduced on the Flutter decoder side.

External baseline:
- Bluetooth Core GATT treats notifications as characteristic-value updates configured through CCCD, not as an arbitrary byte stream; characteristic-value shape is an application/profile contract: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.
- Android receives notification values through `onCharacteristicChanged()` after notification enablement, so rejecting malformed characteristic values at the producer boundary keeps app-side parsing deterministic: <https://developer.android.com/develop/connectivity/bluetooth/ble/transfer-ble-data>.

Key outcomes:
- Replaced max-length notify acceptance with `runtimeBleFixedNotifyPayloadLength()`.
- `BLEBoatLock` data queue now uses the live-frame size as its packet capacity.
- Firmware drops non-70-byte live data packets before queueing and again before `setValue()`.
- Updated native tests to reject short, padded, and old max-size packets.
- Promoted the producer/client exact-length rule into `skills/boatlock/references/ble-ui.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because valid live-frame bytes, BLE schema, reconnect/install, and UI behavior are unchanged; malformed producer packets are now dropped earlier.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_data_packet -f test_runtime_ble_live_frame` -> PASS (`6/6`).
- `cd boatlock && platformio test -e native` -> PASS (`301/301`).
- `cd boatlock && pio run -e esp32s3` -> PASS (`698085` bytes flash).

Self-review:
- This removes a stale generic-data escape hatch from the live characteristic and makes future frame-size drift fail visibly.
- Remaining risk is future non-live frame types: they should be added as explicit frame types with tests rather than reopening a generic length range.

### 2026-04-25 Stage 121: BLE log text single-line payloads

Scope:
- Continue the refactor batch with module `10/15`: BLE log text characteristic payload preparation.
- Keep serial logging unchanged while making BLE log notifications length-bounded and single-line.

External baseline:
- SEI CERT STR32-C warns that passing non-null-terminated byte sequences into string functions can read outside object bounds or disclose unintended memory; bounded scans and explicit terminators are required: <https://wiki.sei.cmu.edu/confluence/pages/viewpage.action?pageId=554434583>.
- Bluetooth Core GATT treats the log characteristic notification as a characteristic-value update, so BoatLock should publish one bounded diagnostic line per update instead of leaking embedded line separators into the client stream: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.

Key outcomes:
- `RuntimeBleLogText` now trims trailing CR/LF from BLE log payloads.
- Embedded ASCII control bytes are neutralized to spaces before enqueueing/publishing over BLE.
- Existing bounded C-string scans remain in place; serial output path is unchanged.
- Added native tests for control-byte sanitation and trailing line-break trimming.
- Promoted the BLE single-line log rule into `skills/boatlock/references/ble-ui.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because app parsing, commands, telemetry, reconnect/install, and valid BLE control behavior are unchanged; BLE log text is only normalized to one line.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_log_text -f test_runtime_log_text` -> PASS (`10/10`).
- `cd boatlock && platformio test -e native` -> PASS (`303/303`).
- `cd boatlock && pio run -e esp32s3` -> PASS (`698289` bytes flash).

Self-review:
- This reduces diagnostic ambiguity without touching control behavior or serial evidence.
- Remaining risk is that historical consumers expecting BLE log trailing newlines may differ, but the app is unreleased and its UI already treats logs as discrete lines.

### 2026-04-25 Stage 122: BLE connection log address tokens

Scope:
- Continue the refactor batch with module `11/15`: BLE connect/disconnect diagnostic formatting.
- Keep connection behavior unchanged while bounding and sanitizing address text in logs.

External baseline:
- Bluetooth Core GAP describes Bluetooth Device Addresses as 48-bit addresses and UI-level representation as 12 hexadecimal characters, optionally separated with `:`: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host/generic-access-profile.html>.
- Short key=value logs with raw reason codes stay useful for GATT/HCI triage, but diagnostic fields still need bounded token rules so malformed text cannot create fake fields or lines.

Key outcomes:
- Added address-token validation for BLE connection logs.
- Unsafe, empty, overlong, or control-containing address text now logs as `unknown`.
- Valid hex/colon address text is preserved.
- Added native tests for missing, malformed, overlong, and valid address tokens.
- Promoted the BLE connection-log address rule into `skills/boatlock/references/ble-ui.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because BLE transport, commands, telemetry, reconnect/install, and UI behavior are unchanged; only diagnostic text formatting is hardened.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_connection_log -f test_runtime_ble_log_text` -> PASS (`12/12`).
- `cd boatlock && platformio test -e native` -> PASS (`304/304`).
- `cd boatlock && pio run -e esp32s3` -> PASS (`698365` bytes flash).

Self-review:
- This is diagnostic hardening only; it does not affect NimBLE connection state or reconnect behavior.
- Remaining risk is that a future stack may render addresses in a non-hex token format, in which case the log will fall back to `unknown` until explicitly allowed.

### 2026-04-25 Stage 123: BLE command log redaction and sanitation

Scope:
- Continue the refactor batch with module `12/15`: BLE control-point and auth-required diagnostic fields.
- Keep command execution behavior unchanged while preventing command-derived diagnostics from leaking secrets or forging log records.

External baseline:
- OWASP Logging Cheat Sheet treats event data from other trust zones as untrusted, requires sanitization against CR/LF/delimiter log injection, warns against logging secrets/tokens, and requires verification tests for logging behavior: <https://cheatsheetseries.owasp.org/cheatsheets/Logging_Cheat_Sheet.html>.
- Bluetooth Core GATT keeps command writes as characteristic values; BoatLock's command text remains an application-layer payload and must be validated/sanitized before diagnostics, not treated as trusted log text: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.

Key outcomes:
- Added `runtimeBleLogCommandText()` as the single BLE command diagnostic formatter.
- Redacted `PAIR_SET`, `AUTH_PROVE`, and `SEC_CMD` payloads from command logs.
- Neutralized ASCII control bytes and bounded command log fields.
- Applied the formatter to control-point logs, command-queue-full logs, and auth-required logs.
- Added native tests for sensitive command redaction, control-byte sanitation, and length bounding.
- Promoted the rule into `skills/boatlock/references/ble-ui.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because BLE command execution, telemetry, reconnect/install, and phone-visible UI behavior are unchanged; only diagnostic text is redacted/sanitized.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_command_log -f test_runtime_ble_log_text` -> PASS (`12/12`).
- `cd boatlock && platformio test -e native` -> PASS (`307/307`).
- `cd boatlock && pio run -e esp32s3` -> PASS (`699501` bytes flash).

Self-review:
- This centralizes the command-log policy instead of adding local ad-hoc redaction at each log site.
- Remaining risk is future new secret-bearing command prefixes; they must be added to `RuntimeBleCommandLog` in the same change as the command handler.

### 2026-04-25 Stage 124: BLE command queue overlong write rejection

Scope:
- Continue the refactor batch with module `13/15`: BLE command queue boundary.
- Replace silent command truncation with deterministic rejection before enqueueing.

External baseline:
- Bluetooth Core GATT says invalid or too-long write-without-response characteristic values must not succeed, and acknowledged writes should return errors for too-long or invalid values: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.
- OWASP Input Validation Cheat Sheet recommends enforcing minimum/maximum length checks server-side before processing input: <https://cheatsheetseries.owasp.org/cheatsheets/Input_Validation_Cheat_Sheet.html>.

Key outcomes:
- Added `RuntimeBleCommandQueue.h` as the single bounded copy helper for command queue payloads.
- Overlong command writes are now rejected before queueing instead of being truncated to `kCmdMaxLen - 1`.
- Queue rejection logs use the sanitized/redacted command log helper.
- Added native tests for max-size acceptance, overlong rejection without truncation, missing destination, and zero-length slots.
- Promoted the no-truncation command boundary rule into `skills/boatlock/references/ble-ui.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because valid app commands remain below the queue slot size and phone-visible protocol shape is unchanged; malformed overlong writes now fail closed.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_command_queue -f test_runtime_ble_command_log` -> PASS (`9/9`).
- `cd boatlock && platformio test -e native` -> PASS (`311/311`).
- `cd boatlock && pio run -e esp32s3` -> PASS (`699617` bytes flash).

Self-review:
- This reduces ambiguity at the BLE boundary with less code in `BLEBoatLock.cpp`; truncation is not an acceptable compatibility layer before alpha.
- Remaining risk is that NimBLE may already truncate above ATT MTU before the callback; the app protocol still must treat any received callback value as complete and validate its own queue slot boundary.

### 2026-04-25 Stage 125: BLE command queue printable-byte gate

Scope:
- Continue the refactor batch with module `14/15`: BLE command byte contract before queueing.
- Prevent embedded NUL/control/non-ASCII bytes from being copied into a queue slot that is later consumed as a C string.

External baseline:
- SEI CERT STR32-C warns that passing non-null-terminated or unexpectedly terminated byte sequences to string functions can cause out-of-bounds reads or unintended disclosure; if copying without truncation is intended, an overlarge or invalid string is an error condition: <https://wiki.sei.cmu.edu/confluence/pages/viewpage.action?pageId=554434583>.
- OWASP Input Validation Cheat Sheet recommends defining the accepted character set and minimum/maximum length server-side before data is processed: <https://cheatsheetseries.owasp.org/cheatsheets/Input_Validation_Cheat_Sheet.html>.

Key outcomes:
- Extended `RuntimeBleCommandQueue` with a printable ASCII byte gate and explicit reject reasons.
- Embedded NUL/control/non-ASCII bytes now fail before queueing, so `STOP\0...` cannot become `STOP` downstream.
- Queue rejection logs now preserve the concrete reason: `too_long`, `bad_bytes`, `bad_slot`, or `copy_failed`.
- Added native tests for embedded NUL and non-ASCII rejection.
- Promoted the printable-byte command rule into `skills/boatlock/references/ble-ui.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no Android smoke added or run for this module because valid app commands are printable ASCII and phone-visible protocol shape is unchanged; malformed byte payloads now fail closed.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_command_queue -f test_runtime_ble_command_log` -> PASS (`11/11`).
- `cd boatlock && platformio test -e native` -> PASS (`313/313`).
- `cd boatlock && pio run -e esp32s3` -> PASS (`699765` bytes flash).

Self-review:
- This completes the command queue boundary from the previous module: length and byte contract now live in one helper.
- Remaining risk is that special non-queued commands (`STREAM_START`, `STREAM_STOP`, `SNAPSHOT`) are still checked directly in `handleControlPoint`; malformed variants are not accepted because they require exact string equality.

### 2026-04-25 Stage 126: BLE immediate command exact-match classifier

Scope:
- Complete the refactor batch with module `15/15`: BLE immediate transport command classification.
- Move `STREAM_START`, `STREAM_STOP`, and `SNAPSHOT` exact matching out of `BLEBoatLock` branching into a pure, directly tested helper.

External baseline:
- Bluetooth Core GATT treats writes as characteristic values; BoatLock owns the application-level command grammar and must not let transport convenience commands accept alternate encodings: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.
- OWASP Input Validation Cheat Sheet recommends allowlist validation for small sets of string parameters and exact server-side validation before processing: <https://cheatsheetseries.owasp.org/cheatsheets/Input_Validation_Cheat_Sheet.html>.

Key outcomes:
- Added `RuntimeBleImmediateCommand.h` with a tiny exact-match classifier.
- `BLEBoatLock::handleControlPoint()` now switches on the classifier instead of carrying local string branches.
- Added native tests proving exact accepted commands and prefix/suffix/control-byte variants do not trigger immediate side effects.
- Promoted the exact-match immediate command rule into `skills/boatlock/references/ble-ui.md` and `skills/boatlock/references/external-patterns.md`.
- Phone-smoke decision: no standalone Android smoke added inside this module because valid phone-visible commands and reconnect behavior are unchanged; full Android BLE smoke is required now as part of the 15-module batch acceptance.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_ble_immediate_command -f test_runtime_ble_command_queue` -> PASS (`9/9`).
- `cd boatlock && platformio test -e native` -> PASS (`316/316`).
- `cd boatlock && pio run -e esp32s3` -> PASS (`699761` bytes flash).
- Batch hardware and Android acceptance completed in Stage 127.

Self-review:
- This is mostly structure and proof: immediate command behavior is unchanged, but it is now hard to accidentally widen with a prefix match.
- Remaining risk is end-to-end BLE transport behavior, so batch acceptance must flash and smoke the real nh02 + Android path after this commit.

### 2026-04-25 Stage 127: 15-module batch hardware and Android acceptance

Scope:
- Complete mandatory real-hardware validation for modules `1/15..15/15`.
- Flash current `main` to nh02, run the canonical hardware acceptance wrapper, and run the Android USB + BLE smoke wrapper with exact APK install proof.

Key outcomes:
- Proved nh02 target before mutation: RFC2217 service active on port `4000`, ESP32-S3 serial by-id `usb-Espressif_USB_JTAG_serial_debug_unit_98:88:E0:03:BA:5C-if00`.
- Flashed current firmware through `tools/hw/nh02/flash.sh`; target MAC `98:88:e0:03:ba:5c`; flash and hard reset completed.
- Fixed `tools/hw/nh02/acceptance.sh` for macOS bash 3.2 with `set -u`: empty `"${ARGS[@]}"` expansion can fail as unbound, so the wrapper now branches before expanding optional args.
- Hardware acceptance passed through the canonical wrapper.
- Android target was visible through nh02 as ADB device `68b657f0`, model `220333QNY`.
- Android BLE smoke passed through the canonical wrapper after one expected Xiaomi/MIUI `INSTALL_FAILED_USER_RESTRICTED` retry; terminal install result was `Success`.

Validation:
- `bash -n tools/hw/nh02/acceptance.sh` -> PASS.
- `tools/hw/nh02/status.sh` -> PASS; service enabled/active, serial bridge present.
- `tools/hw/nh02/flash.sh` -> PASS; firmware `699761` bytes flash, esptool verified hashes.
- `tools/hw/nh02/acceptance.sh` -> PASS (`[ACCEPT] PASS lines=29`), including BNO08x-RVC ready, heading events ready, display ready, EEPROM loaded, BLE init/advertising, stepper config, STOP button, and GPS UART data.
- `tools/hw/nh02/android-status.sh` -> PASS; phone `68b657f0` visible.
- `tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> PASS; `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","dataEvents":1,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","rssi":-35,...}`.

Self-review:
- The acceptance blocker was in the canonical wrapper, not the bench; it was fixed at the source before rerunning validation.
- The Android first-attempt install restriction is not a blocker when the canonical retry returns `Success` and the smoke verdict passes; keep recording it because it is important phone-side evidence.

### 2026-04-25 Stage 128: Flutter command write byte contract

Scope:
- Start the next refactor batch with module `1/15`: Flutter BLE command write boundary.
- Align the app-side `56ef` writer with the firmware command queue byte contract.

External baseline:
- Android `BluetoothGatt.writeCharacteristic()` writes a caller-provided `byte[]` characteristic value, so the app owns the exact byte payload it sends: <https://developer.android.com/reference/android/bluetooth/BluetoothGatt#writeCharacteristic(android.bluetooth.BluetoothGattCharacteristic,byte[],int)>.
- OWASP Input Validation Cheat Sheet recommends allowlist validation plus min/max length checks before processing input: <https://cheatsheetseries.owasp.org/cheatsheets/Input_Validation_Cheat_Sheet.html>.

Key outcomes:
- Added `ble_command_text.dart` with the shared Flutter-side command contract: non-empty, max `191` printable ASCII bytes.
- Routed control-point writes, pairing, auth, secure envelopes, and direct `PAIR_CLEAR` through the same validator/encoder before writing `56ef`.
- Fixed `_writeCommand()` to return `false` when a plain or unpaired command fails the app-side write gate.
- Updated `docs/BLE_PROTOCOL.md` and `skills/boatlock/references/ble-ui.md` with the client/server command byte contract.
- Added Flutter unit tests for valid max-length commands, empty/overlong rejection, and control/non-ASCII byte rejection.
- Phone-smoke decision: Android BLE smoke is required for this module because the Flutter writer used by `STREAM_START`, `SNAPSHOT`, and `HEARTBEAT` changed.

Validation:
- `cd boatlock_ui && flutter test test/ble_command_text_test.dart test/ble_boatlock_test.dart` -> PASS (`15/15`).
- `cd boatlock_ui && flutter test` -> PASS (`36/36`).
- `tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> PASS; install `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","dataEvents":1,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","rssi":-35,...}`.

Self-review:
- This intentionally duplicates the firmware boundary in the app, not as trust in the client but as earlier failure and clearer diagnostics.
- Android smoke closed the valid-write risk for the telemetry/control heartbeat path; remaining risk is command-specific semantics, which stay covered by the dedicated manual/status/sim/anchor smokes when those modules change.

### 2026-04-25 Stage 129: Flutter live-frame decoder contract

Scope:
- Continue the refactor batch with module `2/15`: Flutter BLE live-frame decoding.
- Make the fixed binary frame schema explicit on the client and fail closed on unknown enum ordinals.

External baseline:
- Bluetooth Core GATT defines characteristic notifications as a single Attribute Value for the characteristic handle, so BoatLock's app protocol owns the exact binary payload contract: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-60/out/en/host/generic-attribute-profile--gatt-.html>.
- Dart `ByteData.sublistView` is the official API for interpreting incoming byte buffers, and `Endian.little` must be specified explicitly for little-endian wire fields instead of relying on host defaults: <https://api.dart.dev/dart-typed_data/ByteData-class.html> and <https://api.dart.dev/dart-typed_data/Endian-class.html>.

Key outcomes:
- Replaced magic numeric offsets in `ble_live_frame.dart` with named frame-size, header, and field-offset constants.
- Avoided an extra allocation when the incoming notification value is already a `Uint8List`.
- Changed unknown `mode`, `status`, and `secReject` codes from `UNKNOWN`/downgraded `WARN` display values to rejected frames while protocol version remains `2`.
- Updated BLE protocol docs and repo BLE reference to make unknown enum rejection part of the current app/firmware contract.
- Phone-smoke decision: Android BLE smoke is required because the live-frame decoder now rejects schema drift and still must accept current bench telemetry.

Validation:
- `cd boatlock_ui && flutter test test/ble_live_frame_test.dart` -> PASS (`4/4`).
- `cd boatlock_ui && flutter test` -> PASS (`37/37`).
- `tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> PASS; install `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","dataEvents":1,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","rssi":-37,...}`.

Self-review:
- This keeps the live parser simple and stricter instead of adding compatibility branches before alpha.
- Android smoke proved the stricter decoder still accepts current nh02 telemetry; remaining risk is future enum additions without a version bump, now intentionally rejected instead of silently downgraded.

### 2026-04-25 Stage 130: Remove legacy Flutter JSON telemetry parser

Scope:
- Continue the refactor batch with module `3/15`: Flutter telemetry model cleanup.
- Remove an unused legacy JSON parser from `BoatData` now that the app consumes the fixed binary live frame.

External baseline:
- Bluetooth Core GATT notifications deliver a characteristic Attribute Value, while BoatLock's current `34cd` profile defines that value as one fixed binary frame, not a JSON stream: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-60/out/en/host/generic-attribute-profile--gatt-.html>.
- OWASP Input Validation guidance favors explicit allowlisted formats over permissive multi-format parsing; keeping an unused parser is a hidden alternate input surface: <https://cheatsheetseries.owasp.org/cheatsheets/Input_Validation_Cheat_Sheet.html>.

Key outcomes:
- Deleted `BoatData.fromJson()` and its single legacy unit test.
- Kept `BoatData` as a plain immutable telemetry container populated by `decodeBoatLockLiveFrame()`.
- Promoted the no-parallel-JSON-telemetry rule into the BLE/UI repo reference and external patterns.
- Phone-smoke decision: no Android smoke required for this module because no runtime app path changed; the previous module already proved current binary telemetry on the phone.

Validation:
- `rg -n "BoatData\\.fromJson|fromJson\\(" boatlock_ui/lib boatlock_ui/test` -> PASS; references removed.
- `cd boatlock_ui && flutter test` -> PASS (`36/36`).

Self-review:
- This removes code instead of moving it. If JSON telemetry is ever restored, it must be a deliberate protocol path with docs and tests, not a leftover helper.
- Reference search and full Flutter tests covered the removal; remaining risk is external ad-hoc tooling expecting JSON, which is intentionally unsupported in `main` before alpha.

### 2026-04-25 Stage 131: Extract Flutter BLE command builders

Scope:
- Continue the refactor batch with module `4/15`: Flutter app-originated BLE command builders.
- Move pure command formatting and value allowlists out of the stateful BLE transport class.

External baseline:
- Dart Effective Dart recommends narrow public APIs and notes that Dart does not require all code to live inside classes; top-level functions are appropriate when state is not needed: <https://dart.dev/effective-dart/design>.
- OWASP Input Validation recommends syntactic and semantic validation with exact allowed values and min/max range checks before processing structured input: <https://cheatsheetseries.owasp.org/cheatsheets/Input_Validation_Cheat_Sheet.html>.

Key outcomes:
- Added `ble_commands.dart` for pure builders: anchor, phone GPS, nudge, anchor profile, and manual deadman commands.
- Removed those static helper methods from `BleBoatLock`, keeping the transport class focused on scan/connect/auth/write state.
- Moved command-builder tests from `ble_boatlock_test.dart` into `ble_commands_test.dart`.
- Updated the BLE/UI reference to mark `ble_commands.dart` as the canonical app command-builder module.
- Phone-smoke decision: no Android smoke required because command bytes and runtime app behavior are unchanged; this is a pure extraction with existing command-builder tests.

Validation:
- `cd boatlock_ui && flutter test test/ble_commands_test.dart test/ble_boatlock_test.dart` -> PASS (`12/12`).
- `cd boatlock_ui && flutter test` -> PASS (`36/36`).

Self-review:
- This reduces the central BLE class without introducing a compatibility wrapper for the removed static methods.
- Targeted and full Flutter tests covered import/reference drift; remaining risk is command-builder growth, so future app commands should go into this pure module with local tests first.

### 2026-04-25 Stage 132: Extract Flutter BLE security codec

Scope:
- Continue the refactor batch with module `5/15`: Flutter owner secret/auth envelope codec.
- Move deterministic secret normalization, auth proof, secure command formatting, and owner-secret generation out of the stateful BLE transport class.

External baseline:
- Dart Effective Dart supports top-level functions when class state is not needed and recommends narrow public APIs instead of exposing unrelated helpers from a class: <https://dart.dev/effective-dart/design>.
- Dart `Random.secure()` is the official cryptographically secure RNG constructor and throws if a secure source is unavailable: <https://api.dart.dev/dart-math/Random/Random.secure.html>.

Key outcomes:
- Added `ble_security_codec.dart` for owner-secret generation/normalization, auth proof, secure envelope formatting, and private SipHash helpers.
- Removed the static security helper methods from `BleBoatLock`; the class now calls the pure codec while keeping session sequencing/state local.
- Updated settings UI/tests to import the codec directly instead of reaching through `BleBoatLock`.
- Updated the BLE/UI reference to mark `ble_security_codec.dart` as the canonical Flutter security codec module.
- Phone-smoke decision: no Android smoke required because the generated command bytes are unchanged and this module does not alter connect/write sequencing.

Validation:
- `cd boatlock_ui && flutter test test/ble_security_codec_test.dart test/ble_boatlock_test.dart test/settings_page_test.dart` -> PASS (`7/7`).
- `cd boatlock_ui && flutter test` -> PASS (`38/38`).

Self-review:
- This is a structure-only extraction with no compatibility wrapper for removed static methods.
- Targeted codec tests preserve proof/envelope shape and full Flutter tests cover imports; remaining risk is deeper auth acceptance on hardware, which should be covered by a dedicated pairing/auth smoke when that flow changes.

### 2026-04-25 Stage 133: Extract Flutter BLE log decoder

Scope:
- Continue the refactor batch with module `6/15`: Flutter BLE log characteristic decoding.
- Move length-delimited log byte parsing out of the stateful BLE transport class.

External baseline:
- Bluetooth Core GATT notification payloads are characteristic Attribute Values; BoatLock's log characteristic therefore needs an app-level byte-string decoder instead of implicit C-string handling: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-60/out/en/host/generic-attribute-profile--gatt-.html>.
- Dart `Utf8Codec` documents `allowMalformed=true` as replacement-character decoding for invalid/unterminated UTF-8, which is appropriate for display-only diagnostics but not for control payloads: <https://api.flutter.dev/flutter/dart-convert/Utf8Codec/Utf8Codec.html>.

Key outcomes:
- Added `ble_log_line.dart` with `decodeBoatLockLogLine()`.
- Removed `BleBoatLock.decodeLogLine()` with no compatibility wrapper; `BleBoatLock` now only calls the log decoder.
- Added focused tests for NUL padding and malformed UTF-8 replacement behavior.
- Updated the BLE/UI reference to make `ble_log_line.dart` canonical for Flutter log characteristic parsing.
- Phone-smoke decision: no Android smoke required because runtime log notification behavior is unchanged and this is a pure parser extraction.

Validation:
- `cd boatlock_ui && flutter test test/ble_log_line_test.dart test/ble_boatlock_test.dart` -> PASS (`3/3`).
- `cd boatlock_ui && flutter test` -> PASS (`39/39`).

Self-review:
- This keeps diagnostic byte handling separate from BLE state and command/control parsing.
- Targeted and full Flutter tests covered import/reference drift; remaining risk is only future misuse of this tolerant decoder for control payloads, which remains blocked by `ble_command_text.dart`.

### 2026-04-25 Stage 134: Extract Flutter BLE device matching

Scope:
- Continue the refactor batch with module `7/15`: Flutter BLE adapter readiness and scan-result matching.
- Move BoatLock advertisement matching out of `BleBoatLock` so it can be tested without starting a scan.

External baseline:
- Android `ScanRecord` exposes local device name and service UUIDs from BLE advertising data, so matching should be explicit over those advertised fields rather than hidden in scan-loop side effects: <https://developer.android.com/reference/android/bluetooth/le/ScanRecord>.
- Dart Effective Dart favors narrow APIs and top-level functions when class state is not needed: <https://dart.dev/effective-dart/design>.

Key outcomes:
- Added `ble_device_match.dart` for adapter readiness, pure advertisement matching, and a thin `ScanResult` adapter.
- Removed adapter/device matching helpers from `BleBoatLock`; the scan loop now calls the dedicated matcher.
- Deleted the now-empty `ble_boatlock_test.dart`; transport helpers are covered in focused modules instead.
- Added focused tests for name match, service UUID match, unrelated advert rejection, and adapter readiness.
- Updated the BLE/UI reference to make `ble_device_match.dart` canonical for matching rules.
- Phone-smoke decision: no Android smoke required because matching semantics are unchanged; this is a pure extraction covered by local tests.

Validation:
- `cd boatlock_ui && flutter test test/ble_device_match_test.dart` -> PASS (`4/4`).
- First `cd boatlock_ui && flutter test` -> FAIL; caught one leftover call to removed `isAdapterReady()` in `_readAdapterReady()`.
- Fixed `_readAdapterReady()` to use `isBluetoothAdapterReady()`.
- `cd boatlock_ui && flutter test` -> PASS (`42/42`).

Self-review:
- This removes another pure concern from the transport class and keeps scan matching deterministic.
- Full Flutter tests caught the only stale reference, which confirms why extraction modules still need full-suite validation.
- Remaining risk is plugin-specific `ScanResult` wrapping; the wrapper is tiny and still exercised by full Flutter compilation.

### 2026-04-25 Stage 135: Centralize Flutter BLE identity constants

Scope:
- Continue the refactor batch with module `8/15`: Flutter BLE identity constants.
- Replace duplicated hardcoded device name, service UUID, and characteristic UUID fragments in Flutter BLE modules.

External baseline:
- Bluetooth Core GATT identifies services and characteristics by UUIDs; client code should treat those identifiers as protocol constants, not incidental string literals: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-60/out/en/host/generic-attribute-profile--gatt-.html>.
- Android `BluetoothGattCharacteristic` construction and lookup are UUID-based, reinforcing that characteristic IDs are part of the API contract: <https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic.html>.

Key outcomes:
- Added `ble_ids.dart` with Flutter BLE identity constants for device name, service UUID, and characteristic UUID fragments.
- Updated scan matching and service discovery to consume those constants.
- Updated device-match tests to use the same constants instead of duplicating UUID/name literals.
- Updated the BLE/UI reference to make `ble_ids.dart` canonical for Flutter BLE identity.
- Phone-smoke decision: no Android smoke required because UUID/name values and runtime behavior are unchanged.

Validation:
- `cd boatlock_ui && flutter test test/ble_device_match_test.dart` -> PASS (`4/4`).
- `cd boatlock_ui && flutter test` -> PASS (`42/42`).

Self-review:
- This is a small duplication removal with no compatibility shim or protocol change.
- Tests and full Flutter compile covered current consumers; remaining risk is future firmware UUID changes, which must update `ble_ids.dart`, firmware, and protocol docs together.

### 2026-04-25 Stage 136: Tighten Android BLE smoke telemetry criteria

Scope:
- Continue the refactor batch with module `9/15`: Android smoke acceptance logic.
- Make basic smoke reject telemetry with unknown `mode` or `status` instead of accepting any non-empty strings.

External baseline:
- Android testing guidance favors many small tests for fast feedback plus larger application/device tests for high-fidelity critical journeys; smoke logic should therefore be unit-tested locally and then proven through the device smoke wrapper: <https://developer.android.com/training/testing/fundamentals/strategies?hl=en>.
- OWASP Input Validation recommends exact allowed-value checks for small fixed sets, which fits protocol mode/status enums better than non-empty string checks: <https://cheatsheetseries.owasp.org/cheatsheets/Input_Validation_Cheat_Sheet.html>.

Key outcomes:
- Added explicit allowed mode/status sets to `ble_smoke_logic.dart`.
- Updated basic telemetry health checks to require known current protocol values.
- Added tests for unknown mode/status rejection.
- Updated hardware acceptance skill wording so basic Android smoke is documented as known-mode/status telemetry proof.
- Phone-smoke decision: Android BLE smoke is required because this changes the app-side acceptance criterion used by the smoke APK.

Validation:
- `cd boatlock_ui && flutter test test/ble_smoke_logic_test.dart` -> PASS (`5/5`).
- `cd boatlock_ui && flutter test` -> PASS (`42/42`).
- `tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> PASS; first Xiaomi install attempt hit `INSTALL_FAILED_USER_RESTRICTED`, canonical retry returned `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","dataEvents":1,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","rssi":-35,...}`.

Self-review:
- This makes smoke stricter without changing production BLE behavior.
- Android smoke proved current nh02 telemetry satisfies the stricter criterion; keep recording Xiaomi first-install restriction because it is phone-side evidence, not a blocker when canonical retry passes.

### 2026-04-25 Stage 137: Extract Android smoke mode parser

Scope:
- Continue the refactor batch with module `10/15`: Android BLE smoke APK mode selection.
- Move the smoke mode enum and `BOATLOCK_SMOKE_MODE` parser out of UI/entrypoint glue into a pure module.

External baseline:
- Dart `String.fromEnvironment` is a compile-time environment read and must be consistent across a program, so the define name/default belong in one tested place instead of duplicated literals: <https://api.dart.dev/dart-core/String/String.fromEnvironment.html>.
- Android testing strategy guidance separates fast local/unit coverage from higher-fidelity device tests; this module needs parser tests plus a real APK compile, not a BLE hardware run because BLE behavior is unchanged: <https://developer.android.com/training/testing/fundamentals/strategies>.

Key outcomes:
- Added `ble_smoke_mode.dart` with `BleSmokeMode`, `BOATLOCK_SMOKE_MODE` constants, and `boatLockSmokeModeFromString()`.
- Reduced `main_smoke.dart` to entrypoint glue that reads the define and delegates parsing.
- `BleSmokePage` now consumes the shared mode enum instead of owning smoke mode definitions.
- Added focused parser tests for supported modes, unknown-value fallback, and stable define names.
- Updated BLE/UI and validation references so future smoke modes are added through the pure module and wrapper build path.
- Phone-smoke decision: no Android BLE smoke required because this does not change scan/connect/write/telemetry behavior; `build-smoke-apk.sh --mode basic` covers the changed smoke entrypoint.

Validation:
- `cd boatlock_ui && flutter test test/ble_smoke_mode_test.dart` -> PASS (`3/3`).
- `cd boatlock_ui && flutter test` -> PASS (`45/45`).
- `tools/android/build-smoke-apk.sh --mode basic` -> PASS, produced `boatlock_ui/build/app/outputs/flutter-apk/app-debug.apk`.

Self-review:
- This removes test-invisible mode parsing from `main_smoke.dart` without changing accepted mode names.
- Remaining risk is wrapper/code list drift when a new mode is added; the validation reference now requires parser tests plus smoke APK build for that contract.

### 2026-04-25 Stage 138: Centralize Android smoke wrapper modes

Scope:
- Continue the refactor batch with module `11/15`: Android smoke shell wrapper mode validation.
- Remove duplicated smoke mode allowlists from individual wrappers and add a CI drift check against the Flutter smoke mode enum.

External baseline:
- Google Shell Style Guide favors reusable sourced helpers for shared shell logic and keeping scripts consistent rather than repeating policy branches: <https://google.github.io/styleguide/shellguide.html>.
- Android testing strategy guidance keeps cheap local checks in front of device tests; wrapper mode drift is a local contract bug and should be caught before any nh02/phone run: <https://developer.android.com/training/testing/fundamentals/strategies>.

Key outcomes:
- Added `BOATLOCK_SMOKE_MODES`, `boatlock_is_smoke_mode()`, and `boatlock_validate_smoke_mode()` to `tools/android/common.sh`.
- Updated `tools/android/build-smoke-apk.sh`, `tools/android/run-smoke.sh`, and `tools/hw/nh02/android-run-smoke.sh` to use the shared validator.
- Added `tools/ci/test_android_smoke_modes.py` to compare the shell allowlist with the Flutter `BleSmokeMode` enum and prevent hardcoded wrapper case-list regressions.
- Updated validation and external-pattern references with the new shared wrapper rule.
- Phone-smoke decision: no Android BLE smoke required because wrapper mode validation and APK build path changed, not BLE scan/connect/write/telemetry behavior.

Validation:
- `bash -n tools/android/common.sh tools/android/build-smoke-apk.sh tools/android/run-smoke.sh tools/hw/nh02/android-run-smoke.sh` -> PASS.
- `pytest -q tools/ci/test_android_smoke_modes.py` -> PASS (`2/2`).
- `tools/android/build-smoke-apk.sh --mode unsupported` -> expected reject with supported-mode list.
- `tools/android/build-smoke-apk.sh --mode basic` -> PASS, produced `boatlock_ui/build/app/outputs/flutter-apk/app-debug.apk`.
- `pytest -q tools/ci/test_*.py` -> PASS (`11/11`).

Self-review:
- This removes one duplicated acceptance-contract list without changing accepted flags or default mode.
- Remaining risk is a future mode added to shell but not implemented in smoke page behavior; the enum/list drift test catches names, while mode behavior still needs a targeted smoke-page or Android run when the new mode is real.

### 2026-04-25 Stage 139: Filter Flutter BLE scans by service UUID

Scope:
- Continue the refactor batch with module `12/15`: Flutter BLE scan configuration.
- Reduce scan overhead and accidental candidate processing by filtering Android/iOS BLE scans for BoatLock's advertised service UUID before app-side validation.

External baseline:
- Android `BluetoothLeScanner.startScan(filters, settings, callback)` supports filtered scans, and Android docs note unfiltered scans can be stopped on screen-off/power-saving paths while filtered scans avoid that class of behavior: <https://developer.android.com/reference/android/bluetooth/le/BluetoothLeScanner>.
- Android `ScanFilter` exposes service UUID filtering, which matches BoatLock because firmware advertises service `12ab`: <https://developer.android.com/reference/android/bluetooth/le/ScanFilter>.

Key outcomes:
- Added `ble_scan_config.dart` with BoatLock scan service filter and scan timing constants.
- Updated `BleBoatLock._scanAndConnect()` to call `FlutterBluePlus.startScan()` with the `12ab` service filter.
- Kept `isBoatLockScanResult()` as the second validation layer before connect.
- Added focused tests for service-filter UUID and scan timing relationship.
- Updated BLE/UI and external-pattern references.
- Phone-smoke decision: Android BLE smoke required because scan discovery behavior changed.

Validation:
- `cd boatlock_ui && flutter test test/ble_scan_config_test.dart` -> PASS (`2/2`).
- `cd boatlock_ui && flutter test` -> PASS (`47/47`).
- `tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> PASS; install `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","dataEvents":1,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","rssi":-35,...}`.

Self-review:
- The change is intentionally narrow: scan filtering changed, connection/auth/write logic did not.
- Hardware smoke proves current ESP32 advertising includes the service UUID and the phone still discovers/connects through the canonical path.

### 2026-04-25 Stage 140: Tighten Flutter GATT UUID discovery

Scope:
- Continue the refactor batch with module `13/15`: Flutter BLE service/characteristic UUID matching.
- Replace substring UUID matching with exact 16-bit/Bluetooth-base UUID matching and scope characteristic discovery to BoatLock service `12ab`.

External baseline:
- Android GATT scanning/discovery APIs operate on service and characteristic UUIDs; `ScanFilter.getServiceUuid()` and `BluetoothLeScanner.startScan(filters, ...)` model UUIDs as exact filter/discovery keys, not substring text: <https://developer.android.com/reference/android/bluetooth/le/ScanFilter> and <https://developer.android.com/reference/android/bluetooth/le/BluetoothLeScanner>.
- Bluetooth GATT treats services and characteristics as UUID-addressed attributes, so accepting a characteristic UUID outside the expected service widens the client binding surface: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>.

Key outcomes:
- Added `boatLockFullUuid()` and `isBoatLockUuid()` to `ble_ids.dart`.
- Updated advertisement matching to reject substring service UUID matches such as `112ab0`.
- Updated service discovery to ignore non-BoatLock services and match `34cd`, `56ef`, and `78ab` through the exact UUID helper.
- Added focused UUID helper tests and extended advertisement matching tests.
- Updated BLE/UI and external-pattern references.
- Phone-smoke decision: Android BLE smoke required because service/characteristic discovery behavior changed.

Validation:
- `cd boatlock_ui && flutter test test/ble_ids_test.dart test/ble_device_match_test.dart` -> PASS (`6/6`).
- `cd boatlock_ui && flutter test` -> PASS (`49/49`).
- `tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> PASS; first Xiaomi install attempt hit `INSTALL_FAILED_USER_RESTRICTED`, canonical retry returned `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","dataEvents":1,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","rssi":-36,...}`.

Self-review:
- This removes a real false-positive binding class without changing BoatLock UUID values.
- Android smoke proves the current firmware advertises/discovers with canonical UUID forms accepted by the stricter helper.

### 2026-04-25 Stage 141: Fail closed on partial Flutter GATT discovery

Scope:
- Continue the refactor batch with module `14/15`: Flutter BLE GATT completeness after service discovery.
- Prevent half-connected app state when the BoatLock service is present but one of the required data/command/log characteristics is missing.

External baseline:
- Android `BluetoothGatt.discoverServices()` discovers services plus characteristics/descriptors offered by the remote device, and client operations require discovery to have completed for the associated characteristic: <https://developer.android.com/reference/android/bluetooth/BluetoothGatt>.
- Android's GATT server/client guide frames service discovery and characteristic notification setup as prerequisites before using device data/control paths: <https://developer.android.com/develop/connectivity/bluetooth/ble/connect-gatt-server>.

Key outcomes:
- Added `ble_discovery_check.dart` with pure completeness and missing-endpoint description helpers.
- Updated `BleBoatLock._connectToDevice()` to require all current BoatLock characteristics: data `34cd`, command `56ef`, and log `78ab`.
- On partial discovery, the app logs the missing endpoints and applies the reconnect policy's `connectFailed()` decision instead of proceeding silently.
- Added focused tests for complete and missing-characteristic cases.
- Updated BLE/UI and external-pattern references.
- Phone-smoke decision: Android BLE smoke required because connected service discovery behavior changed.

Validation:
- `cd boatlock_ui && flutter test test/ble_discovery_check_test.dart` -> PASS (`2/2`).
- `cd boatlock_ui && flutter test` -> PASS (`51/51`).
- `tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> PASS; install `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","dataEvents":1,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","rssi":-34,...}`.

Self-review:
- This is a fail-closed behavior change on malformed/incomplete GATT service discovery only; normal BoatLock path remains unchanged and was proven on hardware.
- Remaining risk is if we later make the log characteristic optional; that must be an explicit protocol/smoke change, not an accidental missing endpoint.

### 2026-04-25 Stage 142: Throttle Flutter BLE RSSI reads

Scope:
- Complete the current refactor batch with module `15/15`: Flutter BLE RSSI read cadence.
- Reduce GATT operation overhead by avoiding `readRssi()` on every telemetry notification.

External baseline:
- Android `BluetoothGatt.readRemoteRssi()` is a GATT operation whose result is delivered through `BluetoothGattCallback.onReadRemoteRssi()`, not a free local field read: <https://developer.android.com/reference/android/bluetooth/BluetoothGatt> and <https://developer.android.com/reference/android/bluetooth/BluetoothGattCallback>.
- Android BLE client guidance keeps GATT operations explicit and callback-driven; hot-path telemetry should not add unnecessary extra GATT reads: <https://developer.android.com/develop/connectivity/bluetooth/ble/connect-gatt-server>.

Key outcomes:
- Added `ble_rssi_throttle.dart` with a simple 5-second RSSI read throttle and reset hook.
- Updated `BleBoatLock._onNotify()` to read RSSI immediately on a fresh link and then reuse the last value inside the throttle interval.
- Reset the throttle when characteristics/link state are cleared so reconnect gets a fresh RSSI sample.
- Added focused tests for first-read, suppression, interval boundary, and reset behavior.
- Updated BLE/UI and external-pattern references.
- Phone-smoke decision: Android BLE smoke required because telemetry hot path and RSSI sourcing changed.

Validation:
- `cd boatlock_ui && flutter test test/ble_rssi_throttle_test.dart` -> PASS (`3/3`).
- `cd boatlock_ui && flutter test` -> PASS (`54/54`).
- `tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> PASS; install `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","dataEvents":1,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","rssi":-34,...}`.

Self-review:
- This is a hot-path overhead reduction with no protocol or UI surface change.
- First telemetry still has RSSI because the throttle permits the first read immediately; subsequent freshness is intentionally lower-rate link metadata.

### 2026-04-25 Stage 143: Batch hardware acceptance after module 15/15

Scope:
- Run the required real-hardware acceptance after completing the 15-module Flutter BLE/smoke refactor batch.
- Validate current `main` on `nh02` using the canonical flash, hardware acceptance, and Android USB/BLE smoke wrappers.

Validation:
- `tools/hw/nh02/status.sh` -> PASS target proof; `boatlock-esp32s3-rfc2217.service` enabled/active, ESP32-S3 USB target `98:88:E0:03:BA:5C`, RFC2217 listener on `:4000`.
- `tools/hw/nh02/flash.sh` -> PASS; PlatformIO `esp32s3` build success, RAM `11.5%`, flash `20.9%`, upload/write/verify success, hard reset via RTS.
- `tools/hw/nh02/acceptance.sh` -> PASS (`[ACCEPT] PASS lines=29`): BNO08x-RVC ready on `rx=12 baud=115200`, heading events ready, display ready, EEPROM `ver=23`, security unpaired, BLE init/advertising, stepper cfg, STOP button, GPS UART data.
- `tools/hw/nh02/android-run-smoke.sh --wait-secs 130` -> PASS; first Xiaomi install attempt hit `INSTALL_FAILED_USER_RESTRICTED`, canonical retry returned `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"telemetry_received","dataEvents":1,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","rssi":-34,...}`.

Self-review:
- The batch changed Flutter BLE scanning, GATT discovery, smoke tooling, command validation, and app-side parsers; final hardware acceptance confirms firmware boot/sensors are still healthy and current Android app still scans/connects/receives telemetry after a fresh ESP32 flash.
- No new durable workflow rule emerged beyond the already-recorded MIUI retry rule and wrapper-first acceptance discipline.

### 2026-04-25 Stage 144: HIL time-window helper

Scope:
- Start the next refactor batch with module `1/15`: on-device HIL simulation timing.
- Replace the local `inWindow(now, at, duration)` helper in `HilSimRunner.h` with one rollover-safe helper and direct tests.

External baseline:
- PX4 SIH documents lockstep simulation and deterministic/reproducible results; BoatLock HIL timing helpers must therefore be explicit and deterministic across boundary cases: <https://docs.px4.io/main/en/sim_sih/index>.
- ArduPilot SITL runs autopilot code as ordinary C++ and feeds simulated sensor data from a flight dynamics model, matching BoatLock's strategy of testing real control code through simulated sensor injections: <https://ardupilot.org/dev/docs/sitl-simulator-software-in-the-loop.html>.
- Arduino's non-blocking timing pattern uses elapsed-time checks around `millis()` instead of delay/absolute expiry; the same unsigned elapsed-time style is needed for long HIL runs and wrap boundaries: <https://docs.arduino.cc/built-in-examples/digital/BlinkWithoutDelay/>.

Key outcomes:
- Added `HilSimTime.h` with `simTimeWindowContains(nowMs, atMs, durationMs)`.
- Replaced all timed HIL injection windows in `HilSimRunner.h` with the shared helper.
- Added `test_hil_sim_time` for zero-duration, start/end boundary, pre-start rejection, and unsigned rollover.
- Promoted the durable rule into `external-patterns.md`.
- Phone-smoke decision: no Android smoke required because this module changes only local/on-device HIL timing helpers and does not change BLE command/telemetry schema or app behavior.

Validation:
- `cd boatlock && platformio test -e native -f test_hil_sim_time` -> PASS (`4/4`).
- `cd boatlock && platformio test -e native -f test_hil_sim` -> PASS (`11/11`).
- `python3 tools/sim/test_sim_core.py` -> PASS (`4/4`).
- `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json` -> PASS for all offline scenarios.

Self-review:
- This is a small safety refactor with one behavior hardening at wrap boundaries; normal scenario timings remain unchanged.
- Remaining risk is that `HilSimRunner.h` is still too large; next modules should keep extracting pure HIL pieces without changing scenario expectations.

### 2026-04-25 Stage 145: HIL JSON report escaping

Scope:
- Continue the refactor batch with module `2/15`: HIL `SIM_REPORT` JSON string construction.
- Stop interpolating scenario IDs, reasons, and event text directly into JSON string fields.

External baseline:
- RFC 8259 requires JSON strings to escape quotation marks, reverse solidus, and control characters `U+0000..U+001F`: <https://www.rfc-editor.org/rfc/rfc8259>.
- PX4/ArduPilot simulation guidance treats simulation output as a developer/test interface; BoatLock's `SIM_REPORT` must therefore remain machine-parseable even when future event text contains punctuation or control bytes.

Key outcomes:
- Added `HilSimJson.h` with `appendSimJsonString()` and `simJsonString()`.
- Updated `HilScenarioRunner::reportJson()` to encode `id`, `reason`, `code`, and `details` via the shared JSON string helper.
- Added `test_hil_sim_json` covering quote, backslash, newline, tab, arbitrary control-byte escaping, and report-level escaping.
- Promoted the durable JSON-report rule into `external-patterns.md`.
- Added validation guidance not to run multiple PlatformIO native test suites in parallel against one `.pio/build/native` directory.
- Phone-smoke decision: no Android smoke required because current valid `SIM_REPORT` schema and BLE command behavior are unchanged; malformed/future report text hardening is covered by native tests.

Validation:
- `cd boatlock && platformio test -e native -f test_hil_sim_json` -> PASS (`2/2`).
- `cd boatlock && platformio test -e native -f test_hil_sim` -> PASS (`11/11`).
- `cd boatlock && platformio test -e native -f test_runtime_sim_log` -> PASS (`5/5`).
- I initially started `test_runtime_sim_command`, `test_runtime_sim_log`, and `test_runtime_sim_execution` in parallel; two runs were killed with `SIGKILL`, consistent with shared PlatformIO build-dir contention rather than code failure.
- Sequential rerun `cd boatlock && platformio test -e native -f test_runtime_sim_command` -> PASS (`9/9`).
- Sequential rerun `cd boatlock && platformio test -e native -f test_runtime_sim_execution` -> PASS (`7/7`).

Self-review:
- This hardens a phone-visible test/report interface without changing current scenario outcomes.
- Remaining risk is bounded-buffer truncation if future event strings become long after escaping; current event fields are short and the next HIL report/log modules should keep report chunks explicitly bounded.

### 2026-04-25 Stage 146: HIL event duplicate suppression rollover

Scope:
- Continue the refactor batch with module `3/15`: HIL control-event de-duplication timing.
- Replace the `atMs >= lastEventMs_` duplicate-suppression guard with the shared rollover-safe time-window helper.

External baseline:
- PX4 SIH emphasizes lockstep deterministic simulation output; event streams are part of that output and should stay deterministic at timing boundaries: <https://docs.px4.io/main/en/sim_sih/index>.
- Arduino `millis()` style timing should be reasoned about as elapsed unsigned time rather than absolute ordering when wrap is possible: <https://docs.arduino.cc/built-in-examples/digital/BlinkWithoutDelay/>.

Key outcomes:
- Updated `HilScenarioRunner::onControlEvent()` to suppress duplicate events through `simTimeWindowContains()`.
- Added `test_hil_sim_events` covering duplicate suppression across unsigned rollover and preserving events whose details differ.
- Updated external-pattern guidance so event de-duplication follows the same helper as HIL injection windows.
- Phone-smoke decision: no Android smoke required because this only affects duplicate event filtering under synthetic time wrap and does not change BLE command/telemetry schema.

Validation:
- `cd boatlock && platformio test -e native -f test_hil_sim_events` -> PASS (`2/2`).
- `cd boatlock && platformio test -e native -f test_hil_sim` -> PASS (`11/11`).

Self-review:
- This is a focused rollover fix, not a report/event schema change.
- Remaining risk is broader event-buffer ownership still living inside `HilScenarioRunner`; a later module can extract bounded event recording if it removes complexity without changing output.

### 2026-04-25 Stage 147: HIL sensor reset clears heading drift baseline

Scope:
- Continue the refactor batch with module `4/15`: HIL simulated sensor reset correctness.
- Fix repeat-run state leakage in `SimSensorHub` heading drift timing.

External baseline:
- PX4 SIH describes deterministic/reproducible simulation with simulated sensors and actuator feedback in lockstep; repeatability requires each scenario to start from a clean sensor epoch: <https://docs.px4.io/main/en/sim_sih/index>.
- ArduPilot SITL feeds simulated sensor data from the flight dynamics model into the real autopilot code; sensor simulation state must therefore be reset as carefully as control state between runs: <https://ardupilot.org/dev/docs/sitl-simulator-software-in-the-loop.html>.

Key outcomes:
- Updated `SimSensorHub::reset()` to clear `lastHeadingMs_` along with GNSS timestamps, heading bias, and output samples.
- Added `test_hil_sensor_hub` proving heading drift baseline does not leak across reset.
- Promoted the durable HIL sensor-reset rule into `external-patterns.md`.
- Phone-smoke decision: no Android smoke required because this module only affects HIL sensor simulation repeatability and does not change BLE/app behavior.

Validation:
- `cd boatlock && platformio test -e native -f test_hil_sensor_hub` -> PASS (`1/1`).
- `cd boatlock && platformio test -e native -f test_hil_sim` -> PASS (`11/11`).

Self-review:
- This fixes a real latent bug for future heading-drift scenarios; current default scenarios mostly hid it because heading drift defaults to zero.
- Remaining risk is that other HIL components may still have reset-state coupling inside the large runner; continue extracting/reset-testing them in later modules.

### 2026-04-25 Stage 148: HIL report JSON without fixed string buffers

Scope:
- Continue the refactor batch with module `5/15`: HIL report JSON construction.
- Remove fixed-buffer formatting around string-bearing report fields so long future scenario IDs, reasons, or event details cannot truncate valid JSON.

External baseline:
- RFC 8259 defines JSON strings as escaped string values; BoatLock should encode string fields through one helper and then append them as complete fields: <https://www.rfc-editor.org/rfc/rfc8259>.
- ArduPilot/PX4 simulation guidance treats simulation output as test/diagnostic evidence; a report path must stay machine-readable instead of depending on current short event text.

Key outcomes:
- Updated `HilScenarioRunner::reportJson()` to append `id`, `reason`, `code`, and `details` through `appendSimJsonString()` instead of embedding them in fixed `snprintf()` buffers.
- Kept fixed formatting only for numeric fields with bounded output.
- Added a regression test proving long ID/details text remains present and escaped in the final report.
- Promoted the durable rule into `external-patterns.md`.
- Corrected the hardware acceptance skill cadence after user correction: normal batch hardware runs after module `15/15`, not module `5/15`.
- Phone-smoke decision: no Android smoke required because this module only changes local/on-device HIL report construction; BLE command schema, telemetry schema, reconnect, install, and UI behavior are unchanged.

Validation:
- `cd boatlock && platformio test -e native -f test_hil_sim_json` -> PASS (`3/3`).
- `cd boatlock && platformio test -e native -f test_hil_sim` -> PASS (`11/11`).
- `cd boatlock && platformio test -e native -f test_runtime_sim_execution` -> PASS (`7/7`).

Self-review:
- The report builder is now simpler for variable-length string fields and keeps numeric formatting bounded.
- Remaining risk is that `HilScenarioRunner` still owns report assembly, event storage, and scenario execution in one large class; later HIL modules should keep extracting pure helpers where they reduce branching.

### 2026-04-25 Stage 149: HIL bounded event log extraction

Scope:
- Continue the refactor batch with module `6/15`: HIL event recording and duplicate suppression.
- Move event tail retention, duplicate suppression, and required-event token tracking out of `HilScenarioRunner`.

External baseline:
- PX4 SIH describes deterministic/reproducible lockstep simulation output; event capture should therefore be its own deterministic component instead of incidental runner state: <https://docs.px4.io/main/en/sim_sih/index>.
- ArduPilot AutoTest emphasizes repeatable regression tests and extracting logs/results after each run; BoatLock event logs are test evidence and need direct unit coverage: <https://ardupilot.org/dev/docs/the-ardupilot-autotest-framework.html>.

Key outcomes:
- Added `HilSimEvents.h` with `SimEvent` and `SimEventLog`.
- Replaced runner-owned `events_`, seen-token storage, duplicate event fields, and marking helpers with one `SimEventLog` member.
- Kept behavior unchanged: duplicate suppression still uses the rollover-safe time-window helper, final/abort results still receive the bounded event tail, and required-event checks still use seen tokens.
- Added direct `test_hil_sim_event_log` coverage for rollover duplicate suppression, bounded tail retention, seen-token independence, and clear/reset behavior.
- Promoted the durable event-log rule into `external-patterns.md`.
- Phone-smoke decision: no Android smoke required because valid `SIM_*` BLE commands and payload schemas are unchanged; this is internal HIL state ownership with native coverage.

Validation:
- `cd boatlock && platformio test -e native -f test_hil_sim_event_log` -> PASS (`3/3`).
- `cd boatlock && platformio test -e native -f test_hil_sim_events` -> PASS (`2/2`).
- `cd boatlock && platformio test -e native -f test_hil_sim` -> PASS (`11/11`).
- `cd boatlock && platformio test -e native -f test_hil_sim_json` -> PASS (`3/3`).

Self-review:
- This reduces `HilScenarioRunner` responsibility without changing scenario semantics or BLE-visible output for valid scenarios.
- Remaining risk is report/status serialization still living in the runner; a later HIL module can extract status/report formatting if it stays behavior-preserving.

### 2026-04-25 Stage 150: HIL expectation metric evaluator

Scope:
- Continue the refactor batch with module `7/15`: HIL pass/fail expectation evaluation.
- Move metric threshold and failure-priority policy out of `HilScenarioRunner::finalize()`.

External baseline:
- ArduPilot AutoTest frames simulation tests as repeatable behavior locks that prevent regressions; BoatLock expectation policy therefore needs direct tests for reason priority and allowed failures: <https://ardupilot.org/dev/docs/the-ardupilot-autotest-framework.html>.
- PX4 SIH emphasizes deterministic and reproducible simulation; deterministic output includes deterministic pass/fail reason selection, not only physics timing: <https://docs.px4.io/main/en/sim_sih/index>.

Key outcomes:
- Added `HilSimExpect.h` with `ScenarioExpect`, `SimMetrics`, `SimExpectationEval`, and `evaluateSimMetrics()`.
- Removed metric policy branching from `HilScenarioRunner`; the runner now computes facts, calls the pure evaluator, then checks required event tokens.
- Added `test_hil_sim_expect` covering expected NaN failsafe allowance, failure-priority stability, and threshold-specific reasons.
- Promoted the durable expectation-evaluator rule into `external-patterns.md`.
- Phone-smoke decision: no Android smoke required because scenario execution behavior, valid `SIM_*` BLE payloads, and telemetry schemas are unchanged; this is local HIL pass/fail policy extraction with native tests.

Validation:
- `cd boatlock && platformio test -e native -f test_hil_sim_expect` -> PASS (`3/3`).
- `cd boatlock && platformio test -e native -f test_hil_sim` -> PASS (`11/11`).
- `cd boatlock && platformio test -e native -f test_hil_sim_events -f test_hil_sim_json` -> PASS (`5/5`).

Self-review:
- This makes failure-reason priority explicit and easier to review without stepping through the runner loop.
- Remaining risk is that `hardFailure` remains collected but not evaluated by existing behavior; changing that would be semantic and should be a separate safety module with scenario coverage.

### 2026-04-25 Stage 151: HIL error histogram extraction

Scope:
- Continue the refactor batch with module `8/15`: HIL error metric accumulation.
- Move p95/error histogram ownership out of `HilScenarioRunner`.

External baseline:
- ArduPilot SITL documents accessing logs and graphing vehicle state/internal variables after simulation; BoatLock's aggregate metrics should be explicit evidence, not hidden loop state: <https://ardupilot.org/dev/docs/using-sitl-for-ardupilot-testing.html>.
- PX4 simulation docs note that simulated sensor data can be logged and compared for analysis/debugging; invalid numeric inputs must be contained before they distort metrics: <https://docs.px4.io/main/en/simulation/>.

Key outcomes:
- Added `HilSimMetrics.h` with `SimErrorHistogram`.
- Replaced runner-owned histogram bins and p95 calculation helpers with one metrics helper.
- Added direct tests for empty histogram behavior, p95 bin centers, invalid value fail-closed behavior, and reset.
- Promoted the durable HIL metrics rule into `external-patterns.md`.
- Phone-smoke decision: no Android smoke required because this only changes internal HIL metric accumulation; valid `SIM_*` command/output behavior is unchanged and covered by native scenarios.

Validation:
- `cd boatlock && platformio test -e native -f test_hil_sim_metrics` -> PASS (`4/4`).
- `cd boatlock && platformio test -e native -f test_hil_sim` -> PASS (`11/11`).
- `cd boatlock && platformio test -e native -f test_hil_sim_expect` -> PASS (`3/3`).

Self-review:
- The runner now delegates timing, events, expectation policy, and p95 histogram to focused helpers.
- Remaining risk is that `SimMetrics` itself still mixes collected facts with currently-unused `hardFailure`; handle that separately only when adding a tested semantic policy for hard failures.

### 2026-04-25 Stage 152: HIL deterministic RNG extraction

Scope:
- Continue the refactor batch with module `9/15`: HIL deterministic random/noise source.
- Move `XorShift32` out of `HilSimRunner`.

External baseline:
- PX4 SIH emphasizes lockstep deterministic/reproducible simulation; simulated noise must therefore be seed-owned and directly testable: <https://docs.px4.io/main/en/sim_sih/index>.
- ArduPilot AutoTest uses repeatable simulation tests to prevent regressions; BoatLock seeded HIL scenarios need stable RNG behavior across refactors: <https://ardupilot.org/dev/docs/the-ardupilot-autotest-framework.html>.

Key outcomes:
- Added `HilSimRandom.h` with `XorShift32`.
- Removed the RNG implementation from `HilSimRunner.h`.
- Added direct tests for same-seed repeatability, zero-seed normalization, `uniform01()` range, and zero-sigma noise.
- Promoted the durable RNG determinism rule into `external-patterns.md`.
- Phone-smoke decision: no Android smoke required because valid scenario results and `SIM_*` BLE behavior are unchanged; this is internal deterministic noise ownership with native coverage.

Validation:
- `cd boatlock && platformio test -e native -f test_hil_sim_random` -> PASS (`4/4`).
- `cd boatlock && platformio test -e native -f test_hil_sim -f test_hil_sensor_hub` -> PASS (`12/12`).

Self-review:
- The RNG is now independently testable and does not depend on runner state.
- Remaining risk is that all sensor simulation still shares one RNG stream; split streams only if a future scenario needs isolation and can prove it with deterministic tests.

### 2026-04-25 Stage 153: HIL virtual clock extraction

Scope:
- Continue the refactor batch with module `10/15`: HIL virtual time source.
- Move `VirtualClock` out of `HilSimRunner`.

External baseline:
- PX4 SIH uses lockstep simulation time synchronized with the flight stack; BoatLock HIL should also own simulated time explicitly: <https://docs.px4.io/main/en/sim_sih/index>.
- Arduino's non-blocking timing pattern uses explicit elapsed-time checks around `millis()`; virtual time needs direct wrap tests instead of relying on wall-clock behavior: <https://docs.arduino.cc/built-in-examples/digital/BlinkWithoutDelay/>.

Key outcomes:
- Added `HilSimClock.h` with `VirtualClock`.
- Removed the virtual clock implementation from `HilSimRunner.h`.
- Added direct tests for initial time, set, advance, and unsigned wrap.
- Promoted the durable virtual-clock rule into `external-patterns.md`.
- Phone-smoke decision: no Android smoke required because this only changes internal HIL time ownership; valid BLE command and telemetry behavior are unchanged.

Validation:
- `cd boatlock && platformio test -e native -f test_hil_sim_clock` -> PASS (`3/3`).
- `cd boatlock && platformio test -e native -f test_hil_sim_time -f test_hil_sim` -> PASS (`15/15`).

Self-review:
- Virtual clock behavior is now independently covered and no longer hidden inside the runner class.
- Remaining risk is that realtime vs speedup scheduling still lives in `HilSimManager`; that should be handled separately because it touches phone-visible `SIM_RUN` timing behavior.

### 2026-04-25 Stage 154: HIL actuator capture extraction

Scope:
- Continue the refactor batch with module `11/15`: HIL actuator output capture.
- Move `ActuatorCapture` out of `HilSimRunner`.

External baseline:
- PX4 SIH reads actuator outputs to update the simulated vehicle state at each timestep; BoatLock's actuator capture is therefore a first-class simulation boundary: <https://docs.px4.io/main/en/sim_sih/index>.
- ArduPilot Simulation-on-Hardware guidance stresses monitoring actuator outputs and disabling dangerous movement; BoatLock's internal test double should default to safe stop: <https://ardupilot.org/dev/docs/sim-on-hardware.html>.

Key outcomes:
- Added `HilSimActuator.h` with `ActuatorCapture`.
- Removed the actuator test-double implementation from `HilSimRunner.h`.
- Added direct tests for safe default command and storing the last command.
- Promoted the durable actuator-capture rule into `external-patterns.md`.
- Phone-smoke decision: no Android smoke required because this only changes internal HIL test-double ownership; valid BLE command, telemetry, and actuator safety behavior are unchanged.

Validation:
- `cd boatlock && platformio test -e native -f test_hil_sim_actuator` -> PASS (`2/2`).
- `cd boatlock && platformio test -e native -f test_hil_sim` -> PASS (`11/11`).

Self-review:
- The actuator boundary is now isolated and covered with the safe default that matters for accidental-motion review.
- Remaining risk is that the world model still applies thrust/steer in `HilSimRunner.h`; that should be the next pure extraction.

### 2026-04-25 Stage 155: HIL world model extraction

Scope:
- Continue the refactor batch with module `12/15`: HIL 2D boat world model.
- Move `Vec2`, boat world config/state, clamp helper, and `BoatSim2D` out of `HilSimRunner`.

External baseline:
- PX4 SIH runs a C++ physics model that reads actuator outputs and updates vehicle state every timestep; BoatLock's physics update should be an isolated module: <https://docs.px4.io/main/en/sim_sih/index>.
- ArduPilot simulation receives firmware servo/motor outputs and returns vehicle status/position/velocities; this validates keeping actuator-output-to-state conversion as a tested boundary: <https://ardupilot.ardupilot.org/dev/docs/simulation-2.html>.

Key outcomes:
- Added `HilSimWorld.h` with `Vec2`, `BoatWorldConfig`, `BoatWorldState`, `clampf()`, and `BoatSim2D`.
- Removed the world model implementation from `HilSimRunner.h`.
- Added direct tests for reset behavior, passive current drift, thrust clamping, and turn-rate application.
- Promoted the durable world-model boundary rule into `external-patterns.md`.
- Phone-smoke decision: no Android smoke required because this only changes internal HIL physics ownership; default scenario results and valid `SIM_*` BLE behavior are unchanged.

Validation:
- `cd boatlock && platformio test -e native -f test_hil_sim_world` -> PASS (`3/3`).
- `cd boatlock && platformio test -e native -f test_hil_sim -f test_hil_sim_metrics` -> PASS (`15/15`).

Self-review:
- The runner now orchestrates a world object instead of owning physics details, which is closer to the external SIH/SITL pattern.
- Remaining risk is `SimSensorHub` still lives in the runner header and should be the next high-value extraction because sensors are the other half of the physics/control boundary.

### 2026-04-25 Stage 156: HIL sensor hub extraction

Scope:
- Continue the refactor batch with module `13/15`: HIL simulated GNSS/heading sensors and fault injection.
- Move sensor configs, timed sensor faults, and `SimSensorHub` out of `HilSimRunner`.

External baseline:
- PX4 SIH provides simulated sensor data to the flight stack and reads actuator outputs to update the model; BoatLock's sensor simulator should be a first-class module: <https://docs.px4.io/main/en/sim_sih/index>.
- ArduPilot SITL is used to simulate environments and failure modes; BoatLock sensor faults need direct coverage, not only whole-scenario coverage: <https://ardupilot.org/dev/docs/using-sitl-for-ardupilot-testing.html>.

Key outcomes:
- Added `HilSimSensors.h` with `SensorConfig`, timed GNSS/heading fault structs, and `SimSensorHub`.
- Removed the sensor simulator implementation from `HilSimRunner.h`.
- Extended `test_hil_sensor_hub` for non-positive GNSS rate fallback, heading dropout fail-closed state, and GNSS dropout age/frozen-fix behavior.
- Fixed a HIL sensor bug found by the new test: heading dropout set `valid=false` and stale age, but the GNSS rate-limit return path overwrote heading age back to `0`.
- Promoted the durable sensor-simulator rule into `external-patterns.md`.
- Phone-smoke decision: no Android smoke required because this changes internal HIL sensor simulation; default scenarios and valid `SIM_*` BLE behavior remain covered by native HIL tests.

Validation:
- `cd boatlock && platformio test -e native -f test_hil_sensor_hub` -> PASS (`4/4`) after the age fail-closed fix.
- `cd boatlock && platformio test -e native -f test_hil_sim -f test_hil_sim_world` -> PASS (`14/14`).

Self-review:
- This is both a structural extraction and a small HIL correctness fix; it does not touch production GNSS/compass drivers.
- Remaining risk is scenario catalog construction still living in the runner header; next modules should target scenario catalog/list/status boundaries before module 15 hardware acceptance.

### 2026-04-25 Stage 157: HIL SIM_STATUS JSON builder

Scope:
- Continue the refactor batch with module `14/15`: HIL `SIM_STATUS` JSON construction.
- Move status JSON formatting out of `HilScenarioRunner` and remove fixed-buffer/raw-id string formatting.

External baseline:
- RFC 8259 requires JSON strings to escape quotes, backslashes, and control characters; `SIM_STATUS` must use the same string encoder as `SIM_REPORT`: <https://www.rfc-editor.org/rfc/rfc8259>.
- ArduPilot AutoTest keeps logs/results as regression evidence; BoatLock status payloads must remain machine-parseable under future long or escaped IDs: <https://ardupilot.org/dev/docs/autotest-verbose.html>.

Key outcomes:
- Added `HilSimStatus.h` with `buildSimStatusJson()`.
- Updated `HilScenarioRunner::statusJson()` to delegate to the builder.
- Added tests proving the current status shape is preserved and long/escaped IDs are not truncated.
- Promoted the durable JSON-builder rule into `external-patterns.md`.
- Phone-smoke decision: this touches the phone-visible `SIM_STATUS` test interface, but current valid payload shape is unchanged; Android SIM smoke is required after module `15/15` as the batch gate.

Validation:
- `cd boatlock && platformio test -e native -f test_hil_sim_status` -> PASS (`2/2`).
- `cd boatlock && platformio test -e native -f test_runtime_sim_execution -f test_hil_sim` -> PASS (`18/18`).

Self-review:
- This closes the same variable-string JSON risk for `SIM_STATUS` that earlier modules closed for `SIM_REPORT`.
- Remaining risk is `SIM_REPORT` assembly still lives in the runner; module `15/15` should extract that builder before the hardware/Android acceptance gate.

### 2026-04-25 Stage 158: HIL SIM_REPORT JSON builder

Scope:
- Complete the refactor batch with module `15/15`: HIL `SIM_REPORT` JSON construction.
- Move report JSON formatting out of `HilScenarioRunner` and keep variable string fields on the shared encoder path.

External baseline:
- RFC 8259 defines JSON string escaping and value structure; BoatLock report artifacts should be built through a dedicated encoder boundary: <https://www.rfc-editor.org/rfc/rfc8259>.
- ArduPilot AutoTest extracts logs/results after simulation runs; BoatLock `SIM_REPORT` is the equivalent regression artifact and must stay machine-parseable: <https://ardupilot.org/dev/docs/the-ardupilot-autotest-framework.html>.

Key outcomes:
- Added `HilSimReport.h` with `buildSimReportJson()`.
- Updated `HilScenarioRunner::reportJson()` to delegate report assembly to the builder.
- Removed the direct `HilSimJson.h` include from `HilSimRunner.h`.
- Added direct report-builder tests for metrics/event shape, bounded event tail, and escaped variable strings.
- Phone-smoke decision: this touches the phone-visible `SIM_REPORT` test interface. The module-local proof is native; mandatory Android SIM smoke runs immediately after commit/push as part of the `15/15` hardware gate.

Validation:
- `cd boatlock && platformio test -e native -f test_hil_sim_report` -> PASS (`3/3`).
- `cd boatlock && platformio test -e native -f test_hil_sim_json -f test_runtime_sim_execution -f test_hil_sim` -> PASS (`21/21`).

Self-review:
- HIL JSON status/report assembly is now outside the scenario runner and directly tested.
- Remaining risk is live Android SIM command/report transport after these phone-visible test-interface changes; this is covered by the required post-module-15 hardware/Android acceptance.

### 2026-04-25 Stage 159: 15-module HIL batch hardware and Android acceptance

Scope:
- Run the mandatory real-hardware gate after completing modules `1/15..15/15` of the HIL refactor batch.
- Validate current `main` on `nh02` with canonical target proof, flash, hardware acceptance, and Android USB/BLE SIM smoke.

Validation:
- `tools/hw/nh02/status.sh` -> PASS target proof; `boatlock-esp32s3-rfc2217.service` enabled/active, ESP32-S3 USB target `98:88:E0:03:BA:5C`, RFC2217 listener on `:4000`.
- `tools/hw/nh02/flash.sh` -> PASS; PlatformIO `esp32s3` build success, RAM `11.5%`, flash `21.0%`, upload/write/verify success, hard reset via RTS.
- `tools/hw/nh02/acceptance.sh` -> PASS (`[ACCEPT] PASS lines=29`): BNO08x-RVC ready on `rx=12 baud=115200`, heading events ready, display ready, EEPROM `ver=23`, security unpaired, BLE init/advertising, stepper cfg, STOP button, GPS UART data.
- `tools/hw/nh02/android-status.sh` -> PASS; ADB target `68b657f0`, Xiaomi `220333QNY`, device `rain`.
- `tools/hw/nh02/android-run-smoke.sh --sim --wait-secs 130` -> PASS; first install attempt hit `INSTALL_FAILED_USER_RESTRICTED`, canonical retry returned `Success`, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"sim_run_abort_roundtrip","dataEvents":6,"deviceLogEvents":2,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","secPaired":false,"secAuth":false,"rssi":-34,"lastDeviceLog":"[SIM] ABORTED"}`.

Self-review:
- The batch heavily refactored HIL internals and touched phone-visible `SIM_STATUS`/`SIM_REPORT`; native tests covered structure and the Android SIM smoke proved scan/connect/telemetry plus `SIM_RUN`/`SIM_ABORT` roundtrip after a fresh flash.
- No new durable workflow rule emerged; the existing module-15 hardware gate and MIUI canonical retry rule matched this run.

### 2026-04-25 Stage 160: Production Flutter app e2e bench path

Scope:
- Add a canonical Android e2e path for the real `lib/main.dart` Flutter app, not only the dedicated smoke entrypoint.
- Reuse the existing safe BLE smoke modes and logcat verdict parser so production-app acceptance does not create a second proof path.

External baseline:
- Android testing guidance separates small local tests from larger device tests for critical user journeys; the main app must therefore have a real-device e2e hook, not only unit coverage: <https://developer.android.com/training/testing/fundamentals/strategies>.
- Flutter documents integration/e2e tests as full-app tests run on a real device; BoatLock's bench equivalent is a debuggable main-app APK with a compile-time hook and canonical adb/logcat wrapper: <https://docs.flutter.dev/testing/integration-tests>.

Key outcomes:
- Added `BoatLockAppE2eProbe`, enabled only by `BOATLOCK_APP_E2E_MODE`, that observes the production `MapPage` BLE client and emits `BOATLOCK_SMOKE_STAGE`/`BOATLOCK_SMOKE_RESULT`.
- Added `tools/android/build-app-apk.sh`, `tools/android/run-app-e2e.sh`, and `tools/hw/nh02/android-run-app-e2e.sh`.
- Updated validation and hardware skills to distinguish production-app e2e from the dedicated smoke APK.
- Added tests proving the e2e define is disabled by default and that app-e2e wrappers build `lib/main.dart`.

Validation:
- `bash -n tools/android/build-app-apk.sh tools/android/run-app-e2e.sh tools/hw/nh02/android-run-app-e2e.sh tools/android/build-smoke-apk.sh tools/android/run-smoke.sh tools/hw/nh02/android-run-smoke.sh` -> PASS.
- `pytest -q tools/ci/test_android_smoke_modes.py` -> PASS (`4/4`).
- `cd boatlock_ui && flutter test` -> PASS (`55/55`).
- `tools/android/build-app-apk.sh --e2e-mode basic` -> PASS, built `boatlock_ui/build/app/outputs/flutter-apk/app-debug.apk` from `lib/main.dart`.
- `tools/hw/nh02/android-status.sh` -> PASS, ADB target `68b657f0`, Xiaomi `220333QNY`, device `rain`.
- `tools/hw/nh02/android-run-app-e2e.sh --wait-secs 130` -> PASS, `BOATLOCK_SMOKE_RESULT {"pass":true,"reason":"app_telemetry_received","dataEvents":1,"deviceLogEvents":0,"mode":"IDLE","status":"WARN","statusReasons":"NO_GPS","secPaired":false,"secAuth":false,"rssi":-32,"lastDeviceLog":""}`.
- `tools/hw/nh02/android-run-app-e2e.sh --manual --wait-secs 130` -> PASS after one expected Xiaomi `USER_RESTRICTED` retry, `reason=app_manual_roundtrip`.
- `tools/hw/nh02/android-run-app-e2e.sh --status --wait-secs 130` -> PASS after one expected Xiaomi `USER_RESTRICTED` retry, `reason=app_status_stop_alert_roundtrip`.
- `tools/hw/nh02/android-run-app-e2e.sh --anchor --wait-secs 130` -> PASS after one expected Xiaomi `USER_RESTRICTED` retry, `reason=app_anchor_denied_roundtrip`.

Self-review:
- The production app now has direct BLE e2e proof for connect/telemetry and safe command roundtrips without replacing the existing smoke APK path.
- Remaining app-e2e gaps are `sim`, `reconnect`, `esp-reset`, and a future `gps-field` mode for BLE-only GNSS validation when ESP32 is powered from a powerbank away from `nh02`.

### 2026-04-25 Stage 161: BNO08x SH2-UART DCD migration target

Scope:
- Add the full BNO08x SH2 feature path needed for Dynamic Calibration Data, calibration control, and tare without breaking the accepted UART-RVC bench firmware.
- Keep current `main` safe on the existing RVC wiring and put SH2-UART behind an explicit PlatformIO target until the hardware is rewired.

External baseline:
- Adafruit's BNO08x library exposes SH2 UART and uses the same SH2 C API for `begin_UART`, report enabling, and sensor event decode: <https://github.com/adafruit/Adafruit_BNO08x>.
- The Adafruit SH2 dependency exposes `sh2_saveDcdNow`, `sh2_setCalConfig`, `sh2_setDcdAutoSave`, `sh2_setTareNow`, `sh2_persistTare`, and `sh2_clearTare`.
- SparkFun's BNO08x library documents the sensor as a fused IMU with UART/SPI/I2C capability and exposes calibration/DCD/tare concepts in its SH2 wrapper: <https://github.com/sparkfun/SparkFun_BNO08x_Arduino_Library>.

Key outcomes:
- Added `BNO08xMath.h` for bounded heading/quaternion math and made non-finite angle normalization fail to `0`.
- Reworked `BNO08xCompass.h` so the default firmware remains BNO08x UART-RVC, while `esp32s3_bno08x_sh2_uart` builds a bidirectional SH2-UART backend at `3000000` baud.
- SH2-UART backend enables rotation vector, calibrated magnetometer, calibrated gyro, accelerometer, dynamic calibration config, DCD save/autosave control, and Z-axis tare APIs.
- Added BLE command routing for `COMPASS_CAL_START`, `COMPASS_DCD_SAVE`, `COMPASS_DCD_AUTOSAVE_ON/OFF`, `COMPASS_TARE_Z`, `COMPASS_TARE_SAVE`, and `COMPASS_TARE_CLEAR`.
- Added safe Android/app compass smoke mode that sends only `COMPASS_CAL_START`, `COMPASS_DCD_AUTOSAVE_OFF`, and `COMPASS_DCD_SAVE`, then requires device-log acknowledgements. Tare is intentionally excluded from generic smoke because it changes heading basis.
- Updated compass/hardware/protocol docs and promoted the durable RVC-vs-SH2 rule into the repo skill references.

Validation:
- `cd boatlock && platformio test -e native -f test_bno08x_compass -f test_ble_command_handler -f test_bno_rvc_frame -f test_runtime_compass_health` -> PASS (`51/51`).
- Earlier full native suite after this firmware slice: `cd boatlock && platformio test -e native` -> PASS (`359/359`).
- `cd boatlock && pio run -e esp32s3` -> PASS, RAM `11.5%`, flash `21.0%`.
- `cd boatlock && pio run -e esp32s3_bno08x_sh2_uart` -> PASS, RAM `12.5%`, flash `21.3%`.
- `bash -n tools/android/build-app-apk.sh tools/android/run-app-e2e.sh tools/android/run-smoke.sh tools/android/build-smoke-apk.sh tools/hw/nh02/android-run-app-e2e.sh tools/hw/nh02/android-run-smoke.sh` -> PASS.
- `pytest -q tools/ci/test_android_smoke_modes.py tools/hw/nh02/test_acceptance.py` -> PASS (`9/9`).
- `cd boatlock_ui && flutter test` -> PASS (`56/56`).
- `tools/android/build-app-apk.sh --e2e-mode compass` -> PASS.
- `tools/android/build-smoke-apk.sh --mode compass` -> PASS.
- `tools/hw/nh02/status.sh` -> `boatlock-esp32s3-rfc2217.service` active, but ESP32 USB target missing at `/dev/serial/by-id/...`; hardware flash/serial acceptance is blocked until the board is back on `nh02` USB.

Self-review:
- Default RVC firmware remains the accepted bench path and builds without the Adafruit dependency; SH2-UART is opt-in only.
- The SH2 UART HAL is build-validated and based on the Adafruit framing, but not hardware-accepted yet because the module is still wired for RVC. Real DCD/tare acceptance requires rewiring BNO `RXI/TXO`, `PS0=GND`, `PS1=3V3`, then flashing `esp32s3_bno08x_sh2_uart`.
- RVC Android compass smoke will prove BLE command delivery only (`ok=0 source=BNO08x-RVC` expected). It must not be recorded as DCD success.

### 2026-04-25 Stage 162: Production app GPS e2e mode

Scope:
- Add the missing production-app e2e mode for BLE-visible hardware GNSS fix while the ESP32 can be powered away from `nh02` USB for sky view.
- Run the new mode against the Android phone on `nh02` after the user reported that GPS likely caught.

Key outcomes:
- Added `gnssQ` to the Flutter `BoatData` model and decoded byte `69` from the v2 live frame.
- Added `gps` mode to the shared smoke-mode parser, smoke APK, production app e2e hook, local wrappers, and `nh02` wrappers.
- GPS smoke passes only on valid non-zero coordinates plus `gnssQ > 0`, while rejecting `NO_GPS`, `GPS_DATA_STALE`, and `GPS_HDOP_MISSING`.
- Smoke result payload now includes `lat`, `lon`, and `gnssQ` so failed field captures show whether BLE is alive but GNSS is absent.
- Updated hardware/validation docs with `tools/hw/nh02/android-run-app-e2e.sh --gps --wait-secs 180`.

Validation:
- `bash -n tools/android/build-app-apk.sh tools/android/run-app-e2e.sh tools/android/run-smoke.sh tools/android/build-smoke-apk.sh tools/hw/nh02/android-run-app-e2e.sh tools/hw/nh02/android-run-smoke.sh` -> PASS.
- `pytest -q tools/ci/test_android_smoke_modes.py tools/hw/nh02/test_acceptance.py` -> PASS (`9/9`).
- `cd boatlock_ui && flutter test` -> PASS (`57/57`).
- `tools/android/build-app-apk.sh --e2e-mode gps` -> PASS.
- `tools/android/build-smoke-apk.sh --mode gps` -> PASS.
- `tools/hw/nh02/android-status.sh` -> PASS, ADB target `68b657f0`, Xiaomi `220333QNY`.
- `tools/hw/nh02/android-run-app-e2e.sh --gps --wait-secs 180` -> FAIL after exact APK install succeeded. Result: `{"pass":false,"reason":"app_gps_timeout","dataEvents":176,"deviceLogEvents":0,"mode":"IDLE","status":"WARN","statusReasons":"","secPaired":false,"secAuth":false,"rssi":-54,"lat":0.0,"lon":0.0,"gnssQ":0,"lastDeviceLog":""}`.

Self-review:
- The BLE/app path is proven alive by 176 live frames, but the ESP32 firmware did not publish a hardware GNSS fix during the run.
- Because ESP32 USB was not present on `nh02`, serial GPS diagnostics and reflashing the new diagnostic-capable firmware remain blocked until the board is back on USB.
- Do not count GPS module LED/fix indication as firmware GNSS proof; acceptance proof is live frame `lat/lon` plus `gnssQ`, or serial logs from the canonical hardware path.

### 2026-04-25 Stage 163: GitHub CI test and firmware artifact packaging

Scope:
- Tighten the GitHub Actions pipeline so CI runs the repo CI helper tests and publishes an explicit firmware binary artifact bundle.

External baseline:
- GitHub's `actions/upload-artifact` docs support explicit artifact names, path inputs, and `if-no-files-found: error`; v4+ artifacts are immutable, so firmware and simulation reports should be uploaded under distinct names.

Key outcomes:
- Added `python3 -m pytest tools/ci/test_*.py` to the firmware CI job so helper scripts such as release notes and Android smoke mode checks are covered on every run.
- Kept schema and firmware version checks as execution checks after their tests.
- Added a `dist/firmware-esp32s3` packaging step that copies `firmware.bin`, `bootloader.bin`, `partitions.bin`, and `firmware.elf`, then writes `BUILD_INFO.txt` and `SHA256SUMS`.
- Split simulation reports into a separate `simulation-reports` artifact while keeping them attached to tagged GitHub releases.

Validation:
- `git diff --check -- .github/workflows/ci.yml WORKLOG.md` -> PASS.
- Python YAML parse of `.github/workflows/ci.yml` -> PASS.
- `python3 -m pytest tools/ci/test_*.py` -> PASS (`13/13`).
- `python3 tools/ci/check_config_schema_version.py && python3 tools/ci/check_firmware_version.py` -> PASS (`0x17`, `0.2.0`).
- `cd boatlock && pio run -e esp32s3` -> PASS, flash `701457` bytes.
- Local run of the firmware packaging shell step produced `firmware.bin`, `bootloader.bin`, `partitions.bin`, `firmware.elf`, `BUILD_INFO.txt`, and `SHA256SUMS`; generated `dist/firmware-esp32s3` was removed after validation.
- Ruby YAML parse was not usable on this host because the local rbenv Ruby links against an incompatible-architecture `gmp`; Python YAML parse covered syntax instead.

Self-review:
- This is pipeline-only and does not change firmware, BLE, Flutter, or hardware behavior.
- Remaining risk is only GitHub runner behavior for artifact upload/download shape; the paths are explicit and `if-no-files-found: error` keeps missing binaries from passing silently.

### 2026-04-25 Stage 164: Display bearing frame and GNSS jump lockout

Scope:
- Fix the field-observed compass screen behavior where the red GPS-anchor arrow rotated with the physical display/compass instead of compensating heading.
- Fix the field-observed GNSS behavior where a good outdoor fix could remain stuck behind `GPS_JUMP_REJECTED` after cold-start drift or real relocation.

External baseline:
- OpenCPN documents heading/course-up display modes as a rotated world where the own ship/bow is up, so world targets on that display must be transformed into the heading frame: <https://mail.opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:manual_basic:chart_panel:chart_panel_options:navigation_mode> and <https://opencpn-manuals.github.io/main/rotationctrl/index.html>.
- ArduPilot GPS glitch protection compares new GPS positions against prior/predicted motion and allows later samples to recover from a glitch rather than permanently locking to stale GPS evidence: <https://ardupilot.org/copter/docs/gps-failsafe-glitch-protection.html>.

Key outcomes:
- Restored the historical BoatLock anchor-arrow formula from `anchorBearing + heading` back to `anchorBearing - heading`.
- Added `DisplayMath.h` so display frame transforms are directly unit-tested instead of hidden inside `display.cpp`.
- Kept the existing BNO compass-card sign (`worldDeg + heading`) isolated from the GPS-anchor arrow sign.
- Runtime GNSS now rejects the first position jump above `MaxPosJumpM`, stores it as a pending candidate, and accepts a repeated stable candidate as the new baseline. This prevents endless `GPS_JUMP_REJECTED` after moving the bench outside or after cold-start fix drift while still refusing a single spike.
- Promoted the corrected display convention and GNSS jump recovery rule into `AGENTS.md`, `firmware.md`, and `external-patterns.md`.

Validation:
- `cd boatlock && platformio test -e native -f test_display_math -f test_runtime_gnss` -> PASS (`16/16`).
- `cd boatlock && platformio test -e native` -> PASS (`363/363`).
- `cd boatlock && pio run -e esp32s3` -> PASS, RAM `11.5%`, flash `21.0%`.
- `tools/hw/nh02/status.sh` -> RFC2217 service active, but ESP32 serial device missing from `/dev/serial/by-id/...`; firmware flash and serial acceptance are blocked while the board is off `nh02` USB.
- `tools/hw/nh02/android-status.sh` -> PASS, Android ADB target `68b657f0` present.

Self-review:
- This does not weaken `ANCHOR_ON`: the first jump still marks GNSS quality bad and keeps the previous control position.
- A repeated stable jump can rebase GNSS before hardware e2e has been rerun; next required proof is flash plus outdoor app GPS e2e/serial log capture once ESP32 USB is back on `nh02`.

### 2026-04-25 Stage 165: nh02 target probe before flash

Scope:
- Re-check live `nh02` USB targets after the user reported both Android phone and ESP32 were connected.

Validation:
- `tools/hw/nh02/android-status.sh` -> PASS, ADB target `68b657f0`, Xiaomi `220333QNY`.
- `tools/hw/nh02/status.sh` -> RFC2217 service enabled/active, but expected ESP32 serial path missing.
- Remote `lsusb`, `/dev/serial/by-id`, `/dev/ttyACM*`, and `/dev/ttyUSB*` probe over 60 seconds -> phone present, no Espressif/serial USB device enumerated.

Self-review:
- Flash and serial acceptance are still blocked at target discovery. Do not run Android BLE/GPS e2e as acceptance for this firmware until the ESP32 is visible on `nh02` and flashed through `tools/hw/nh02/flash.sh`.

### 2026-04-25 Stage 166: ESP32 USB replug scan

Scope:
- Re-scan `nh02` after ESP32 was physically replugged.

Validation:
- `tools/hw/nh02/status.sh` -> RFC2217 active, expected serial path still missing.
- Remote `/dev/serial/by-id`, `/dev/ttyACM*`, and `/dev/ttyUSB*` -> absent.
- Remote `dmesg` after replug -> `usb 1-3.2.3: device descriptor read/64, error -110`, `device not accepting address ... error -62`, and `unable to enumerate USB device`.
- `tools/hw/nh02/android-status.sh` -> PASS, Android target still visible as `68b657f0`.

Self-review:
- This is a physical USB enumeration failure before Linux creates a serial device. Flash/acceptance remain blocked until the ESP32 appears in `lsusb`/`/dev/ttyACM*`/`/dev/serial/by-id`.

### 2026-04-25 Stage 167: Flash after hub replug and GPS UART blocker

Scope:
- Flash the current display/GNSS-jump firmware after the hub replug fixed ESP32 USB enumeration.
- Run canonical post-flash hardware acceptance.

Validation:
- `tools/hw/nh02/status.sh` -> PASS for target discovery: ESP32 `303a:1001`, `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:88:E0:03:BA:5C-if00 -> ../../ttyACM0`.
- `tools/hw/nh02/android-status.sh` -> PASS, Android target `68b657f0`.
- `tools/hw/nh02/flash.sh` -> PASS. Remote `esptool v5.2.0` connected to ESP32-S3 `98:88:e0:03:ba:5c`, wrote bootloader, partitions, and firmware, verified hashes, and hard-reset the board.
- `tools/hw/nh02/acceptance.sh` -> FAIL only on missing `gps_uart`. Matched compass ready, compass heading events, display ready, EEPROM loaded, security state, BLE init/advertising, stepper config, and STOP button.
- `tools/hw/nh02/monitor.sh --seconds 45` -> firmware log contains `[GPS] no UART bytes on RX=17 (check wiring/baud/power)` and no later GPS UART data marker.

Self-review:
- The new firmware is on the ESP32 and core boot/peripheral checks pass except GNSS UART.
- Android GPS e2e was not run because the canonical hardware acceptance path is blocked by real GPS UART silence. Running the phone GPS smoke now would only confirm the known missing hardware GNSS input.
- Next fix is physical/electrical: GPS module power, common ground, GPS TX to ESP32 `GPIO17`, ESP32 `GPIO18` to GPS RX if used, and GPS baud `9600`.

### 2026-04-25 Stage 168: GitHub-signaled pull OTA and Android Wi-Fi debug

Scope:
- Replace the wireless firmware debug path with a GitHub-signal style pull OTA: the bench host receives the CI artifact and sends a small authenticated signal, then ESP32 downloads the firmware itself and verifies SHA-256.
- Seed the ESP32-S3 once over USB, then prove ESP32 firmware update and Android app debug over the shared home network.

External baseline:
- Android ADB supports TCP/IP debugging after initial USB discovery with `adb tcpip 5555` and `adb connect <ip>:5555`.
- Espressif OTA is an app-partition update mechanism; keep USB flashing as the recovery path for bootloader/partition/credential failures.
- GitHub self-hosted runners keep an outbound HTTPS connection to GitHub, which fits the home-network constraint better than exposing ESP32 or `nh02` inbound to the internet.
- GitHub workflow artifacts can be downloaded by a workflow job using `GITHUB_TOKEN`; keep that token on `nh02` and give ESP32 only a local firmware URL plus expected SHA-256.

Key outcomes:
- Added `esp32s3_debug_wifi_ota`, `DebugWifiOta`, and `POST /pull-update`.
- Debug firmware opens authenticated OTA endpoints for the first 45 seconds after boot/reset. The canonical path is `/pull-update`: ESP32 receives `url` and `sha256`, downloads `firmware.bin`, verifies the digest, writes OTA, and reboots.
- Added `.github/workflows/nh02-deploy.yml`, which runs on `[self-hosted, nh02, boatlock]`, downloads the `firmware-esp32s3` CI artifact, and calls `tools/hw/nh02/github-signal-ota.sh`.
- Added `tools/hw/nh02/github-signal-ota.sh` and `tools/firmware/signal-pull-ota.sh`. The bench script temporarily serves `firmware.bin`, resets ESP32 to open the debug OTA window, and sends the pull-update signal.
- Added Android Wi-Fi ADB wrapper `tools/hw/nh02/android-wifi-debug.sh` plus remote helper installation.
- Fixed `tools/hw/nh02/flash.sh` to stage `boot_app0.bin` and flash it at `0xe000`; after prior OTA, USB seed flashing now boots the freshly flashed `ota_0` image instead of a stale OTA slot.
- Updated hardware docs and BoatLock skill references with the GitHub-signaled OTA path, Android Wi-Fi serial flow, and BLE/Wi-Fi time-slicing rule.

Validation:
- `BOATLOCK_WIFI_SSID=... BOATLOCK_WIFI_PASS=... BOATLOCK_OTA_PASS=... tools/hw/nh02/flash.sh --env esp32s3_debug_wifi_ota` -> PASS; remote `esptool v5.2.0` wrote bootloader, partitions, `boot_app0.bin` at `0xe000`, and firmware, verified hashes, then hard-reset.
- `tools/hw/nh02/acceptance.sh --seconds 75` -> PASS; log showed `[NET] wifi connected ... ip=192.168.88.75`, `[NET] http ota ready ... port=8080 auth=1 path=/update`, `[NET] wifi off ble_start=1`, `[BLE] init`, and `[BLE] advertising started`, with no panic/abort/BLE malloc errors.
- `BOATLOCK_OTA_PASS=... BOATLOCK_OTA_RESET_CMD='...' tools/hw/nh02/github-signal-ota.sh --artifact-dir boatlock/.pio/build/esp32s3_debug_wifi_ota --esp-host 192.168.88.75 --serve-host 192.168.88.40` -> PASS, signal response `OK`; ESP32 pulled `firmware.bin` from the temporary HTTP server and verified SHA-256 `e68ee5fe8f3e37fdde450081aea02b6e7b7d8d474899a2ddd1635c4282bfa730`.
- `tools/hw/nh02/acceptance.sh --seconds 75` after pull-update -> PASS; log showed `[NET] wifi off ble_start=1`, `[BLE] init`, and `[BLE] advertising started`.
- `tools/hw/nh02/android-wifi-debug.sh` -> PASS, Android USB serial `68b657f0`, Wi-Fi serial `192.168.88.33:5555`.
- `tools/hw/nh02/android-run-smoke.sh --serial 192.168.88.33:5555 --wait-secs 180` -> PASS, reason `telemetry_received`.
- `tools/hw/nh02/android-run-smoke.sh --manual --serial 192.168.88.33:5555 --wait-secs 180` -> PASS, reason `manual_roundtrip`.
- `tools/hw/nh02/android-run-smoke.sh --status --serial 192.168.88.33:5555 --wait-secs 180` -> PASS, reason `status_stop_alert_roundtrip`.
- `tools/hw/nh02/android-run-smoke.sh --reconnect --serial 192.168.88.33:5555 --wait-secs 220` -> PASS, reason `telemetry_after_reconnect`.
- `bash -n ...` for touched shell wrappers -> PASS.
- `python3 -m py_compile tools/pio/boatlock_debug_wifi_flags.py tools/ci/test_debug_wireless_workflow.py` -> PASS.
- `pytest -q tools/ci/test_debug_wireless_workflow.py tools/ci/test_android_smoke_modes.py` -> PASS (`8 passed`).
- `git diff --check` -> PASS.
- `cd boatlock && pio run -e esp32s3` -> PASS, RAM `11.5%`, flash `21.0%`.
- `cd boatlock && platformio test -e native` -> PASS (`363/363`).

Self-review:
- Production `esp32s3` remains Wi-Fi-free; OTA support is debug-env only.
- Direct `/update` remains as a low-level debug fallback, but the documented and tested deploy path is now `/pull-update` via GitHub-style signal.
- Debug OTA deliberately uses a boot/reset window instead of keeping Wi-Fi alive next to BLE, because live acceptance showed BLE allocation errors when Wi-Fi remained active.
- This still needs USB as the recovery path for bad credentials, failed OTA, bootloader/partition changes, or a firmware that cannot reach the home AP.

### 2026-04-26 Stage 169: Settings NVS backend and firmware artifact availability

Scope:
- Decide whether settings should stay on the custom EEPROM blob or move to ESP32 NVS before release.
- Implement the better option only if it is simpler and safer for future schema changes.
- Build the production firmware and verify GitHub artifact availability.

External baseline:
- Espressif's Arduino-ESP32 docs call `Preferences`/NVS the replacement for Arduino EEPROM and describe NVS as retained storage for small key/value data.
- ESP-IDF NVS docs define a 15-character key limit, small key/value storage, explicit `nvs_commit()`, and built-in wear levelling.
- Espressif's NVS FAQ states NVS has wear levelling and is designed to resist accidental power loss during writes.
- The local Arduino-ESP32 `Preferences.cpp` commits inside each `putX()` call, so direct ESP-IDF NVS API is a better fit for BoatLock's dirty-key batching.
- GitHub Actions artifacts are queryable via the official Actions Artifacts API and downloadable as archives after upload.

Decision:
- Move `Settings` from the index-coupled EEPROM float array to direct ESP-IDF NVS.
- Store settings in namespace `boatlock_cfg` as `schema` plus one raw float blob per setting key.
- Keep values by stable key across schema version changes; missing keys use current defaults, removed keys are ignored, and invalid values are normalized/defaulted.
- Use one `nvs_commit()` per `Settings::save()` so multi-key anchor updates do not become a per-key commit sequence.
- Keep the old `Settings` EEPROM span constants only to preserve the existing `BleSecurity` EEPROM offset until that storage is migrated separately.

Key outcomes:
- Bumped `Settings::VERSION` and `docs/CONFIG_SCHEMA.md` to `0x18`.
- Added a native NVS mock and tests for schema mismatch, missing keys, invalid blobs, NVS key length limits, dirty-key batching, failed commit retry, and no partial publish after failed commit.
- Updated anchor-control tests to assert save-failure rollback through the NVS path.
- Updated nh02 acceptance matching from `[EEPROM] settings loaded` to `[NVS] settings loaded`.
- Added firmware artifact metadata to CI: `firmware_sha256` and `artifact_name=firmware-esp32s3` in `BUILD_INFO.txt`.
- Added a CI helper test that protects the firmware artifact bundle shape.

Validation:
- `cd boatlock && platformio test -e native -f test_settings -f test_anchor_control` -> PASS (`25/25`).
- `python3 tools/ci/check_config_schema_version.py && python3 tools/ci/check_firmware_version.py && python3 -m pytest -q tools/ci/test_*.py tools/hw/nh02/test_acceptance.py` -> PASS (`22 passed`, schema `0x18`, firmware `0.2.0`).
- `cd boatlock && platformio test -e native` -> PASS (`371/371`).
- `cd boatlock && pio run -e esp32s3` -> PASS, RAM `11.7%`, flash `21.3%`, `firmware.bin` built.
- Local CI packaging contract produced `BUILD_INFO.txt`, `SHA256SUMS`, `bootloader.bin`, `firmware.bin`, `firmware.elf`, and `partitions.bin`; `sha256sum -c SHA256SUMS` -> PASS. Local `firmware_sha256=27c15e8e99236e8b17902cb033611427d08cf55e405d42e2a9098d72f906380f`.
- GitHub Actions Artifacts API for `dslimp/boatlock` -> latest non-expired `firmware-esp32s3` artifact available, id `6639319067`, created `2026-04-25T10:31:02Z`.
- Downloaded that GitHub artifact archive and verified required entries: `BUILD_INFO.txt`, `SHA256SUMS`, `bootloader.bin`, `firmware.bin`, `firmware.elf`, `partitions.bin`.
- `git diff --check` -> PASS.

Self-review:
- This intentionally does not import old EEPROM settings into NVS because the product is not released and the user explicitly confirmed legacy compatibility is not required yet.
- NVS key names are now part of the settings schema contract; future keys must stay within the 15-character NVS limit or add an explicit short storage key.
- BLE security still uses its existing EEPROM/CRC record; moving it to NVS should be a separate security-focused change.
- The exact local firmware build is not in GitHub until these changes are pushed and CI runs; current GitHub availability proof is for the latest already uploaded `firmware-esp32s3` artifact and the workflow contract for the next artifact.

### 2026-04-26 Stage 170: Flutter delivery blocker and full local CI mirror

Scope:
- Fix the Flutter CI failure that blocked the delivery pipeline before release artifacts were produced.
- Re-run the local equivalent of the GitHub firmware and Flutter jobs before pushing `main`.

Key outcomes:
- Fixed the Flutter formatting drift in `boatlock_ui/lib/main.dart` and `boatlock_ui/lib/widgets/status_panel.dart`.
- Reworked `boatlock_ui/lib/ble/ble_security_codec.dart` SipHash math from Dart `int` 64-bit literals to masked `BigInt` operations so `dart2js` can compile the web release build.
- Added exact Flutter security codec golden tests for `AUTH_PROVE` and `SEC_CMD` output to keep the BigInt implementation aligned with the firmware protocol.

Validation:
- `cd boatlock_ui && flutter pub get && dart format --output=none --set-exit-if-changed lib test && flutter analyze && flutter test && flutter build apk --release && flutter build web --release` -> PASS; Flutter tests `60/60`, APK built, web built.
- `python3 -m pytest tools/ci/test_*.py && python3 tools/ci/check_config_schema_version.py && python3 tools/ci/check_firmware_version.py && bash tools/ci/check_firmware_werror.sh && bash tools/ci/check_firmware_cppcheck.sh && (cd boatlock && platformio test -e native) && python3 tools/sim/test_sim_core.py && python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json && python3 tools/sim/test_soak.py && python3 tools/sim/run_soak.py --hours 6 --check --json-out tools/sim/soak_report.json && (cd boatlock && pio run -e esp32s3)` -> PASS; CI helper tests `17/17`, native tests `371/371`, simulations PASS, ESP32-S3 firmware build PASS.

Self-review:
- The web build failure was real and would have kept the Flutter artifact from being uploaded in Actions.
- The BigInt change only touches the low-frequency security proof/signature path and keeps exact protocol outputs covered by tests.
- Remaining delivery proof is GitHub Actions after pushing this commit to `main`, including uploaded firmware and Flutter artifacts.

### 2026-04-26 Stage 171: CI failure diagnosis and split jobs

Scope:
- Diagnose the failed GitHub Actions run after pushing delivery changes to `main`.
- Split the pipeline into clear failure domains so future failures are visible at the job level.

Key outcomes:
- GitHub run `24949496909` failed in `firmware` step `Build firmware (esp32s3)` after firmware checks, native tests, and simulation all passed.
- Root cause was a case-sensitive Linux include failure: `BleOtaUpdate.cpp` used `#include <ESP.h>`, while Arduino-ESP32 provides `Esp.h`; macOS local build did not catch it.
- Fixed the include to `#include <Esp.h>`.
- Split `.github/workflows/ci.yml` into `firmware-checks`, `firmware-test`, `firmware-sim`, `firmware-build`, `flutter-checks`, `flutter-build-android`, and `flutter-build-web`.
- Split Flutter artifacts into `flutter-android-apk` and `flutter-web`; release now depends on all separate jobs and publishes both artifacts.
- Added a CI helper test that locks the split job names and artifact names.

Validation:
- `python3 -m pytest tools/ci/test_*.py` -> PASS (`18/18`).
- `cd boatlock && pio run -e esp32s3` -> PASS, including recompilation of `BleOtaUpdate.cpp`.
- `python3` YAML parse for `.github/workflows/ci.yml` -> PASS.
- `git diff --check` -> PASS.

Self-review:
- This was a real local/CI parity gap caused by macOS case-insensitive filesystem behavior.
- CI is now more verbose by design; duplicated setup time is acceptable because failures are easier to identify and build/test jobs can run independently.
- Remaining proof is the next GitHub Actions run on the new commit.

### 2026-04-26 Stage 172: settings migrations and phone-bridged BLE OTA proof

Scope:
- Add forward settings migration mechanics on top of the NVS settings store.
- Prove ESP32 firmware update through the phone app over BLE, using the `nh02` Android/ESP32 bench path.
- Make OTA progress visible both on the phone Settings screen and in wrapper/logcat output.

External baseline:
- Rechecked Espressif NVS guidance: settings remain stable key/value entries, writes require explicit commit, and writeback should be limited to real migrations/recovery. Future-schema rollback must not downgrade the stored schema marker.

Key outcomes:
- `Settings::load()` now distinguishes missing/older/current/future schemas.
- Older schemas run explicit `Settings::applyMigrations()` rules and write the current schema marker.
- Future schemas load known keys in RAM but do not write missing defaults or downgrade `schema`.
- Added the first explicit legacy-key migration: pre-`0x18` `MaxThrust` -> `MaxThrustA` when the current key is absent.
- Added production-app `ota` e2e mode. The app downloads `firmware.bin`, verifies SHA-256, uploads via BLE, and waits for a telemetry gap/reconnect after ESP reboot.
- `nh02` app-e2e wrapper now serves `firmware.bin` over local HTTP, exposes it to the phone via `adb reverse`, picks a free OTA port, and prints OTA progress lines while waiting.
- Settings page now shows OTA percent, byte counts, and approximate speed; app logcat emits `BOATLOCK_OTA_PROGRESS`.
- BLE client treats firmware `[OTA] finish ok` as the authoritative finish acknowledgement even if the ESP32 reboot causes a disconnect race immediately after `OTA_FINISH`.

Validation:
- `bash -n tools/android/common.sh tools/android/build-app-apk.sh tools/android/run-app-e2e.sh tools/hw/nh02/android-run-app-e2e.sh tools/hw/nh02/android-run-smoke.sh tools/hw/nh02/remote/boatlock-run-android-smoke.sh` -> PASS.
- `python3 tools/ci/check_config_schema_version.py && pytest -q tools/ci/test_android_smoke_modes.py` -> PASS (`4/4`, schema `0x18`).
- `python3 -m pytest -q tools/ci/test_*.py` -> PASS (`18/18`).
- `cd boatlock && platformio test -e native -f test_settings` -> PASS (`24/24`).
- `cd boatlock_ui && flutter analyze` -> PASS.
- `cd boatlock_ui && flutter test test/settings_page_test.dart test/ble_smoke_mode_test.dart test/ble_ota_payload_test.dart` -> PASS (`8/8`).
- `cd boatlock && pio run -e esp32s3` -> PASS; `firmware.bin` SHA-256 `520d13acb5cd1e7a9f718b02cff8e408b981d9d66aac3d260ab31d565daa07be`, size `711664`.
- `tools/hw/nh02/android-status.sh` -> PASS; phone visible over USB `68b657f0` and Wi-Fi ADB `192.168.88.33:5555`.
- `tools/hw/nh02/android-run-app-e2e.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin --serial 192.168.88.33:5555` -> PASS, reason `app_ota_reconnect_after_update`, last device log `[OTA] finish ok size=711664 reboot_ms=900`.
- `tools/hw/nh02/acceptance.sh --no-reset --seconds 20` after OTA -> PASS; boot path shows `[NVS] settings loaded (ver=24)`, BLE advertising, OTA characteristic, BNO08x heading events, and GPS UART activity.
- `git diff --check` -> PASS.

Self-review:
- Current OTA speed is not the BLE maximum path. ESP32 has MTU 247/data length 251, but the app does not request MTU/connection priority and OTA uses 180-byte acknowledged writes over a 30..60 ms connection interval.
- Bench throughput is about 1 KB/s; this is acceptable for first reliable acceptance but should be optimized separately with OTA-only fast conn params, Android MTU/priority request, MTU-sized chunks, and write-without-response plus backpressure.
- BLE OTA updates the app partition only; bootloader/partition-table changes still require USB flashing.

### 2026-04-26 Stage 173: BLE OTA fast transfer path

Scope:
- Optimize phone-to-ESP32 BLE OTA speed without weakening the existing SHA-256, size, finish, abort, and disconnect safety behavior.

External baseline:
- Android `BluetoothGatt` exposes connection-priority and MTU requests; bulk transfers should request high priority only while needed and return to balanced behavior after the transfer.
- ATT write payload is bounded by the negotiated MTU minus the 3-byte ATT header; BoatLock caps the chunk at 244 bytes because the ESP32 server advertises MTU 247.

Key outcomes:
- Firmware keeps normal control connections at 30..60 ms but requests OTA-only fast params at 7.5..15 ms while `bleOta.active()`.
- Android OTA prep requests high connection priority and MTU 247 before `OTA_BEGIN`.
- Flutter chunks firmware at `MTU - 3`, uses `writeWithoutResponse` when the OTA characteristic supports it, and applies an explicit window/backpressure pause.
- OTA logs now include chunk size, write mode, window, pause, progress, failure byte count, and abort requests.
- Existing safety path remains intact: authenticated command wrapping when paired, `OTA_BEGIN:<size>,<sha256>`, chunk writes only after begin, `OTA_FINISH`, firmware size/SHA validation, and `OTA_ABORT` on failed client-side transfer.
- `--no-build` OTA e2e is only valid when the APK's compile-time OTA URL/SHA still match the wrapper port and firmware. A mismatched no-build run failed before BLE upload with `app_ota_sha_mismatch` or HTTP connection refusal, so canonical new-firmware proof still rebuilds the APK.

Validation:
- `cd boatlock && pio run -e esp32s3` -> PASS; `firmware.bin` SHA-256 `5c07a91331cafd084ae885c97bb74f119f7b09ffbdeddab14596176ee9eb755d`, OTA size `711776`.
- `cd boatlock_ui && dart format lib/ble/ble_boatlock.dart lib/ble/ble_ota_payload.dart test/ble_ota_payload_test.dart && flutter analyze && flutter test test/ble_ota_payload_test.dart test/settings_page_test.dart test/ble_smoke_mode_test.dart` -> PASS (`10/10`).
- `python3 -m pytest -q tools/ci/test_*.py && python3 tools/ci/check_config_schema_version.py && git diff --check` -> PASS (`18/18`, schema `0x18`).
- `cd boatlock && platformio test -e native` -> PASS (`375/375`).
- `tools/hw/nh02/android-run-app-e2e.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin --serial 192.168.88.33:5555` -> PASS, reason `app_ota_reconnect_after_update`; upload `10:53:43..10:54:54`, reconnect verdict `10:55:06`, last device log `[OTA] finish ok size=711776 reboot_ms=900`.
- `tools/hw/nh02/android-run-app-e2e.sh --ota --no-build --ota-port 18082 --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin --serial 192.168.88.33:5555` -> PASS, reason `app_ota_reconnect_after_update`; repeat upload `10:56:46..10:57:23`, reconnect verdict `10:57:34`, last device log `[OTA] finish ok size=711776 reboot_ms=900`.
- `tools/hw/nh02/acceptance.sh --no-reset --seconds 20` after OTA -> PASS; boot path shows NVS settings `ver=24`, BLE advertising, BNO08x heading events, and GPS UART activity.

Self-review:
- The fast path is now reliable on two completed phone OTA uploads after tuning backpressure. The repeat upload is under one minute; the full canonical path is longer because it includes APK build/install and ESP reboot/reconnect.
- Throughput still depends on Android link scheduling and HTTP bridge setup. Do not use `--no-build` for a different firmware SHA or a different compile-time OTA port.
- Signed firmware remains future production hardening; current debug OTA integrity is still SHA-256 plus firmware-side size/hash validation.

### 2026-04-26 Stage 174: macOS client, diagnostics, and BLE second-client proof

Scope:
- Add the same BoatLock Flutter app surface for macOS so the Mac can be used as a second BLE client during bench/debug work.
- Add debug visibility in the app and CI delivery for the macOS app artifact.
- Prove that the ESP32 is visible from the Mac and can serve the Android phone plus the macOS app as concurrent BLE clients.

External baseline:
- FlutterBluePlus macOS guidance requires the app sandbox Bluetooth entitlement and Bluetooth usage plist key before CoreBluetooth can scan/connect from a packaged app.
- FlutterBluePlus notes macOS/iOS negotiate MTU automatically, so the app-side OTA MTU request remains Android-only.
- NimBLE/ESP32 multi-central examples restart advertising after a central connects when the server still has free connection slots.
- NimBLE characteristic notifications can target all subscribed peers, so normal telemetry/log notifications do not need per-client duplicate command paths.

Key outcomes:
- Added a macOS-compatible diagnostics page with BLE state, scan/connect/discovery/error counters, RSSI, telemetry metadata, and bounded app/device logs.
- Added macOS Bluetooth/location plist keys and entitlements for DebugProfile and Release builds.
- Added a `flutter-build-macos` GitHub Actions job and release artifact upload for `boatlock-macos.zip`.
- Reworked Flutter BLE scanning so a found BoatLock scan result is selected first, scanning is stopped, and connect/discovery runs sequentially. This prevents overlapping reconnect attempts on app resume.
- Increased ESP32 NimBLE resources for two central clients and restarted advertising while one central is connected and another slot is free.
- Stopped forcing fast connection params/data length during normal control connections; the aggressive params now stay OTA-only, preserving the fast OTA path without destabilizing weak macOS links.
- OTA remains protected as a single-transfer path: connected advertising is suppressed while OTA is active and any disconnect during OTA aborts the update.

Validation:
- `plutil -lint boatlock_ui/macos/Runner/Info.plist boatlock_ui/macos/Runner/DebugProfile.entitlements boatlock_ui/macos/Runner/Release.entitlements` -> PASS.
- `cd boatlock_ui && flutter analyze && flutter test` -> PASS.
- `python3 -m pytest -q tools/ci/test_*.py && python3 tools/ci/check_config_schema_version.py && git diff --check` -> PASS (`19/19`, schema `0x18`).
- `cd boatlock && platformio test -e native` -> PASS (`375/375`).
- `cd boatlock && pio run -e esp32s3` -> PASS; firmware size `711501` bytes, flash use `21.3%`.
- `cd boatlock_ui && flutter build macos --release` -> PASS; built `boatlock_ui.app` (`42.2MB`). CocoaPods still emits the known `geolocator_apple` deployment-target warning.
- `tools/hw/nh02/acceptance.sh --seconds 45` -> PASS; BLE advertising, NVS settings `ver=24`, BNO08x heading events, GPS UART, display, stepper, and STOP button all matched.
- Android production-app e2e over Wi-Fi ADB with the final app build -> PASS, reason `app_telemetry_received`, RSSI `-48`.
- macOS production-app e2e as the only BLE client -> PASS, reason `app_telemetry_received`, RSSI `-64`.
- Android first plus macOS second client -> PASS; macOS connected as the second central and received telemetry with RSSI `-62`. ESP32 monitor showed the macOS client disconnecting after smoke while one client remained connected, confirming Android stayed attached.

Self-review:
- This was not a local BLE permission blocker: the Mac app could scan and discover `BoatLock`, and after the connection sequencing plus normal-param change it connected and received telemetry. `tccutil reset BluetoothAlways com.example.boatlockUi` was run so the next manual packaged-app launch can prompt cleanly if macOS decides a prompt is needed.
- The upstairs Mac link is weak (`-74..-62` RSSI during scans), so retries are expected. Diagnostics now expose RSSI and recent app/device logs instead of leaving the failure mode invisible.
- The macOS artifact is unsigned/not notarized beyond local Flutter build signing. Production distribution still needs a real bundle id, signing identity, notarization, and a user-facing permission/pairing polish pass.

### 2026-04-26 Stage 175: status warning reason visibility

Scope:
- Fix the app case where the operator saw only `WARN` while the expected `NO_GPS` detail was not visible.

Key outcomes:
- Firmware live telemetry now builds `status` and `statusReasons` from one `RuntimeStatusSnapshot` instead of mixing a stored status string with freshly rebuilt reasons.
- The status panel translates key reason tokens into operator-facing Russian labels, including `NO_GPS` -> `Нет GPS`.
- If a device ever still sends a warning without reason flags, the panel fails visible with `GPS не готов` when `gnssQ=0`, otherwise `Причина не передана`.
- Rebuilt and relaunched the local macOS release app from `boatlock_ui/build/macos/Build/Products/Release/boatlock_ui.app`.

Validation:
- `cd boatlock_ui && dart format lib/widgets/status_panel.dart test/status_panel_test.dart && flutter test test/status_panel_test.dart` -> PASS (`3/3`).
- `cd boatlock && platformio test -e native -f test_runtime_status` -> PASS (`8/8`).
- `cd boatlock && pio run -e esp32s3` -> PASS; firmware size `711585` bytes.
- `cd boatlock_ui && flutter build macos --release` -> PASS; built `boatlock_ui.app` (`42.2MB`), with the existing CocoaPods deployment-target warning.
- `cd boatlock_ui && flutter analyze` -> PASS.
- `python3 -m pytest -q tools/ci/test_*.py && python3 tools/ci/check_config_schema_version.py && git diff --check` -> PASS (`19/19`, schema `0x18`).

Self-review:
- The root mismatch was a telemetry coherence issue, not only a layout issue.
- The UI fallback is intentionally diagnostic: it prevents a bare warning from being invisible, but exact reason flags still come from firmware after this firmware is flashed or delivered by OTA.

### 2026-04-26 Stage 176: Android CI build cache optimization

Scope:
- Reduce the slow `flutter-build-android` GitHub Actions job without changing the Android artifact shape or release command.

External baseline:
- GitHub `actions/setup-java` documents built-in Gradle dependency caching, but points more advanced Gradle caching users to `gradle/actions/setup-gradle`.
- Gradle `setup-gradle` caches Gradle User Home content such as downloaded dependencies, wrapper distributions, generated Gradle jars, compiled Kotlin DSL scripts, artifact transforms, and the local build cache.
- Gradle build cache is off by default and is enabled through `org.gradle.caching=true`.
- GitHub's `ubuntu-24.04` runner image already includes Android platform 34/35/36/37 and NDK 27.3/28/29, but the current Flutter/Android build downloads exact project-needed packages `ndk;27.0.12077973`, `platforms;android-33`, and `cmake;3.22.1`.

Key outcomes:
- `flutter-build-android` now uses `actions/setup-java@v5` to avoid the Node 20 deprecation warning on that job and keep Java setup current.
- Added `gradle/actions/setup-gradle@v6` before the Flutter Android build so Gradle dependency/build-script/transform/build-cache state can be reused across CI runs.
- Added an explicit Android SDK package cache for the exact NDK, Android 33 platform, and CMake versions that the job was previously installing inside `flutter build apk --release`.
- Enabled Gradle build cache and parallel execution in `boatlock_ui/android/gradle.properties`.
- Set `GRADLE_OPTS=-Dorg.gradle.vfs.watch=false` for the CI Android release build to avoid file-watcher overhead/noise on short-lived runners.
- Added CI helper coverage so the Android cache layers stay present in the workflow.

Validation:
- Latest measured pre-change GitHub job durations: `flutter-build-android` `7:33` and `7:07`; the logged `assembleRelease` segment was `391.2s` and included SDK package installs.
- `python3 -m pytest -q tools/ci/test_*.py && python3 tools/ci/check_config_schema_version.py && git diff --check` -> PASS (`20/20`, schema `0x18`).
- `cd boatlock_ui && GRADLE_OPTS='-Dorg.gradle.vfs.watch=false' flutter build apk --release` -> PASS; local `assembleRelease` `68.5s`, APK `22.7MB`.

Self-review:
- The first GitHub run after this change may still populate caches; the meaningful comparison is the second run on `main` after both Gradle and Android SDK caches exist.
- I did not change `ndkVersion`, compile SDK, APK ABI set, or release artifact format. Those could reduce time further, but they would change build environment or delivered artifact compatibility and should be separate decisions.

### 2026-04-26 Stage 177: GHCR Flutter CI image

Scope:
- Build a pinned Flutter/Android CI image in GHCR so Flutter tests and release builds can run from a preinstalled toolchain instead of installing Flutter, Java, Android SDK, NDK, and CMake inside every CI job.

External baseline:
- GitHub's container registry documentation supports publishing packages associated with the workflow repository through `GITHUB_TOKEN`.
- GitHub Actions job containers can pull registry images with `jobs.<job_id>.container.credentials`.
- Docker's official Buildx action supports `type=gha` layer caching; using a dedicated scope prevents unrelated image builds from overwriting each other's cache.

Key outcomes:
- Added `.github/images/flutter-android-ci/Dockerfile` with pinned Flutter `3.32.4`, JDK 17, Android platforms `35` and `33`, build tools `35.0.0`, NDK `27.0.12077973`, and CMake `3.22.1`.
- Added `.github/workflows/flutter-ci-image.yml` to publish `ghcr.io/dslimp/boatlock/flutter-android-ci:3.32.4-android33-ndk27.0`, `latest`, and `sha-*` tags.
- Added CI helper coverage for the image workflow, pinned toolchain values, and GHCR publishing permissions.
- Added `.dockerignore` so local Flutter/Gradle/PlatformIO build artifacts and `local.properties` are not accidentally sent into the CI image context.
- After the toolchain-only image was published, switched Linux Flutter check, Android build, and web build jobs to run inside `ghcr.io/dslimp/boatlock/flutter-android-ci:3.32.4-android33-ndk27.0` with `GITHUB_TOKEN` GHCR credentials.
- Removed `setup-java`, `subosito/flutter-action`, and the Android SDK package cache from those Linux Flutter jobs. The Android job still keeps `gradle/actions/setup-gradle` for project dependencies and build transforms.

Validation:
- First publisher run failed because the Dockerfile copied local `boatlock_ui/android/gradlew`, which exists on this Mac but is not tracked in git. Fixed the image warmup to use the real Flutter build path instead of the untracked local wrapper.
- Second publisher run failed because `yes | sdkmanager --licenses` trips `pipefail` with exit `141` after `sdkmanager` closes the pipe. Scoped `pipefail` off only around license acceptance and kept the rest of the Dockerfile strict.
- Third publisher run exhausted the GitHub-hosted runner disk while building the prewarmed project image. Removed project source copy, debug APK warmup, Buildx GHA layer export, and provenance attestation so the GHCR image is toolchain-only rather than a full project build cache snapshot.
- Toolchain-only publisher run succeeded in `5:37`.
- First consumer workflow did not create jobs because `jobs.<job_id>.container.image` does not allow the `env` context. Replaced the shared env indirection with the literal GHCR image expression in each Flutter Linux job.
- First runnable consumer workflow pulled the GHCR image successfully, but Flutter failed with Git dubious ownership for `/opt/flutter` because Actions sets `HOME=/github/home`, hiding the image build-time root global git config. Added a `Trust Flutter SDK` step in the Flutter Linux jobs and system-level safe-directory config to the Dockerfile for the next image build.
- Clean GHCR consumer verification after the publisher finished was green, but performance was worse: `flutter-build-android` `10:35`, `flutter-checks` `2:32`, and `flutter-build-web` `2:12`. Previous cached hosted-runner path was Android `5:46`, checks `1:27`, and web `1:12`.
- Reverted the main CI Flutter Linux jobs to the previous hosted-runner `subosito/flutter-action` plus Gradle/Android SDK cache path. Kept the GHCR image publisher only as an experimental artifact until a self-hosted or larger runner can make image reuse worthwhile.

Self-review:
- This deliberately lands the image publisher before switching CI consumers, so `main` does not point at a non-existent container image.

### 2026-04-26 Stage 178: Stable toolchain and dependency refresh

Scope:
- Refresh Flutter, Android, firmware, and CI toolchain pins to current stable versions where compatible, and validate the updated stack end to end.

External baseline:
- Flutter stable is `3.41.7` with Dart `3.11.5`.
- Gradle stable is `9.4.1`, Android Gradle Plugin stable is `9.2.0`, and Kotlin stable is `2.3.21`.
- Flutter's AGP 9 migration guidance says Flutter apps with plugins should not upgrade to AGP 9 yet because AGP 9 uses built-in Kotlin instead of the `kotlin-android` plugin model.
- Flutter's current generated Android template uses AGP 8, so BoatLock uses the latest compatible AGP 8 line rather than forcing AGP 9.

Key outcomes:
- Upgraded local Flutter SDK to stable `3.41.7` and Dart `3.11.5`.
- Upgraded Android project pins to AGP `8.13.2`, Gradle `8.14.4`, Kotlin `2.3.21`, NDK `28.2.13676358`, Android platform `36`, and added the Flutter-required build tools `35.0.0`.
- Installed Android cmdline tools `20.0`, platform `android-36`, build tools `36.0.0`/`35.0.0`, NDK `28.2.13676358`, CMake `4.1.2`, and accepted Android SDK licenses.
- Installed Homebrew CocoaPods `1.16.2`; the old rbenv `pod` path is broken by an x86_64/arm64 GMP mismatch, so validation used `/opt/homebrew/bin` first in `PATH`.
- Upgraded Flutter dependencies to newest resolvable versions and adjusted API changes in BLE connect licensing and `flutter_map` position zoom.
- Upgraded PlatformIO ESP32 platform to `espressif32@6.13.0` and NimBLE to `2.5.0`; kept GFX at `1.5.9` because `1.6.x` requires newer Arduino-ESP32 APIs than official PlatformIO `espressif32@6.13.0` provides.
- Removed the deprecated NimBLE `pService->start()` call, which is a no-op in NimBLE 2.x.
- Fixed Android e2e build wrappers to reuse a stable Gradle cache instead of forcing `/tmp/boatlock-gradle`, and to fail if `app-debug.apk` is not actually produced.
- Updated GitHub Actions and the experimental Flutter Android CI image pins to the refreshed toolchain versions.

Validation:
- `cd boatlock && pio run -e esp32s3` -> PASS; firmware size `714497` bytes.
- `cd boatlock && platformio test -e native` -> PASS (`376/376`).
- `python3 tools/sim/test_sim_core.py`, `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json`, `python3 tools/sim/test_soak.py`, and `python3 tools/sim/run_soak.py --hours 6 --check --json-out tools/sim/soak_report.json` -> PASS.
- `cd boatlock_ui && flutter analyze && flutter test` -> PASS (`66/66`).
- `cd boatlock_ui && flutter build apk --release`, `flutter build web --release`, and `flutter build macos --release` -> PASS.
- `tools/hw/nh02/flash.sh` -> PASS on ESP32-S3 `98:88:e0:03:ba:5c`; `tools/hw/nh02/acceptance.sh` -> PASS.
- `tools/hw/nh02/android-run-app-e2e.sh --esp-reset --wait-secs 130 --serial 68b657f0` -> PASS after canonical retry from `INSTALL_FAILED_USER_RESTRICTED`; result `app_telemetry_after_reconnect`.
- `pytest -q tools/ci/test_*.py`, `python3 tools/ci/check_firmware_version.py`, `python3 tools/ci/check_config_schema_version.py`, `bash tools/ci/check_firmware_werror.sh`, `bash tools/ci/check_firmware_cppcheck.sh`, and `git diff --check` -> PASS.

Self-review:
- AGP 9 was intentionally not kept: the build fails on Flutter's existing Kotlin plugin path, and official Flutter guidance says plugin apps are not ready for AGP 9 migration yet.
- `flutter_blue_plus` 2.x requires an explicit license enum; the app currently passes `License.free`, which must be revisited before any commercial/for-profit distribution.
- `flutter pub outdated` reports all direct dependencies at newest resolvable versions except `latlong2 0.9.1`, where `0.10.0` is not resolvable with the current dependency graph.
- `flutter doctor` still reports Xcode cannot list installed Simulator runtimes, but Android, Chrome, CocoaPods, and network resources are healthy and macOS release build succeeds.

### 2026-04-26 Stage 179: product readiness audit and water-test plan

Scope:
- Review the post-refactor firmware/UI/BLE/simulation module relationships from a product-readiness angle.
- Compare the current feature surface with commercial GPS-anchor patterns and prepare a concrete bench/water TODO.

External baseline:
- Minn Kota, Garmin Force, Lowrance Ghost, MotorGuide Pinpoint, and Rhodan/Raymarine all treat anchor lock, heading hold, jog/reposition, calibration/setup, mode feedback, and power sizing as first-class user behavior.
- Commercial products consistently keep small-step jog, distance-to-anchor, setup/calibration quality, and multiple control surfaces visible rather than hidden as developer-only details.

Key outcomes:
- Added `docs/PRODUCT_READINESS_PLAN.md` with commercial baseline, current-fit review, P0/P1/P2 gaps, simulation plan, powered-bench plan, and first protected-water plan.
- Replaced stale `boatlock/TODO.md` items that referenced obsolete BNO055/route-log-era work with the current product-readiness backlog.
- Fixed the README HC 160A direction pin snippet to match current `boatlock/main.cpp` (`DIR1=5`, `DIR2=10`).
- Added follow-up items for `ANCHOR_OFF` UI, phone-GPS correction isolation, leftover board/route surfaces, and unused `ReacqStrat`.

Validation:
- `python3 tools/sim/test_sim_core.py` -> PASS (`4/4`).
- `python3 tools/sim/run_sim.py --check --json-out /tmp/boatlock-sim-report.json` -> PASS (`8/8`).
- `python3 tools/sim/test_soak.py` -> PASS (`2/2`).
- `python3 tools/sim/run_soak.py --hours 6 --check --json-out /tmp/boatlock-soak-report.json` -> PASS, no violations.
- `git diff --check` -> PASS.

Self-review:
- No runtime code was changed in this stage.
- The main unresolved architecture gap is validation coverage: on-device HIL exercises `AnchorControlLoop`, while real actuation for water uses `RuntimeMotion`, `StepperControl`, and `MotorControl`. Powered bench acceptance must prove that production path directly.

### 2026-04-26 Stage 180: App anchor and disconnect safety cut

Scope:
- Close the independently actionable P0 app gaps from the product-readiness plan.
- Keep the firmware protocol unchanged and make the Flutter UI match `SET_ANCHOR` versus `ANCHOR_ON` semantics.

External baseline:
- Reused the Stage 179 commercial GPS-anchor baseline: operator-facing products expose explicit anchor lock/unlock, visible mode feedback, and non-emergency disable paths.
- `docs/BLE_PROTOCOL.md` is the internal source of truth: `SET_ANCHOR` saves the point and does not enable Anchor mode; `ANCHOR_OFF` is normal anchor disable; `STOP` remains emergency stop.

Key outcomes:
- `BleBoatLock` now reports `onData(null)` when it clears a stale link/data path, so `MapPage` disables controls instead of keeping stale `boatData`.
- Flutter anchor save no longer sends `ANCHOR_ON` as a side effect. The map now has separate save-anchor, enable-anchor, and anchor-off controls.
- Map anchor actions now show command success or rejection feedback instead of silently ignoring failed writes/auth failures.
- The manual-control sheet no longer labels `MANUAL_OFF` as emergency `STOP`.
- Updated the active TODO checklist for the closed P0 app items.

Validation:
- `cd boatlock_ui && dart format lib/ble/ble_boatlock.dart lib/pages/map_page.dart lib/widgets/manual_control_sheet.dart test/manual_control_sheet_test.dart` -> PASS.
- `cd boatlock_ui && flutter test test/manual_control_sheet_test.dart` -> PASS (`3/3`).
- `cd boatlock_ui && flutter analyze && flutter test` -> PASS, no analyzer issues, tests `67/67`.

Self-review:
- This cut does not add the full anchor preflight panel yet; `ANCHOR_ON` is still available as an explicit user action and remains firmware-gated.
- `MapPage` still needs widget coverage with an injectable BLE facade before the stale-disconnect and command-failure UI can be tested directly.
- Hardware and Android BLE smokes were not run for this UI-only cut; they should run before any powered bench session.

### 2026-04-26 Stage 181: Release-scope cleanup leftovers

Scope:
- Close the independently safe product-plan leftovers around non-target board branches, route-era display labels, and unused persisted anchor profile fields.
- Leave Flutter untouched in this slice.

External baseline:
- Reused the Stage 179 commercial GPS-anchor baseline and the repo release policy: `main` targets one default ESP32-S3 bench board and should not carry obsolete route/profile selector surfaces.
- No new external source was needed; this was removal of repo-local stale release code.

Key outcomes:
- Removed the `jc4832w535` PlatformIO environment and `BOATLOCK_BOARD_JC4832W535` pin/display branches from the release firmware.
- Removed the display-only `ROUTE` mode color branch.
- Removed unused persisted `ReacqStrat` and `AnchorProf` settings, bumped config schema to `0x19`, and kept `SET_ANCHOR_PROFILE` scoped to the four runtime settings that are actually consumed: `HoldRadius`, `DeadbandM`, `MaxThrustA`, and `ThrRampA`.
- Updated firmware tests, BLE/config docs, and the product TODO item.

Validation:
- `python3 tools/ci/check_config_schema_version.py` -> PASS (`0x19`).
- `bash tools/ci/check_firmware_werror.sh` -> PASS.
- `bash tools/ci/check_firmware_cppcheck.sh` -> PASS.
- `git diff --check` -> PASS.
- `cd boatlock && platformio test -e native -f test_anchor_profiles -f test_ble_command_handler -f test_settings` -> PASS (`61/61`).
- `cd boatlock && pio run -e esp32s3` -> PASS; firmware size `714173` bytes.

Self-review:
- `SET_ANCHOR_PROFILE` still exists because the Flutter BLE helper still exposes it and Flutter files were explicitly out of scope. It is now a bounded preset command rather than a hidden persisted profile selector.
- Full BLE command surface classification remains open under the P0 TODO item.

### 2026-04-26 Stage 181: Phone GPS yaw-correction isolation

Scope:
- Verify and fix the P0 invariant that `SET_PHONE_GPS` fallback is telemetry/display only.
- Own only `RuntimeGnss` yaw-correction behavior plus focused tests/reference notes.

External baseline:
- No new external source was needed for this cut: the required baseline is the repo invariant that control GNSS and GPS-to-compass correction must be hardware-only.

Key outcomes:
- `RuntimeGnss::applyPhoneFallback()` no longer seeds or updates GPS-to-compass yaw correction, even when called with fresh compass data and moving phone GPS samples.
- Added focused native coverage proving hardware GPS still updates yaw correction while phone fallback does not.
- Updated the firmware reference to make the hardware-only yaw-correction source explicit.

Validation:
- `cd boatlock && platformio test -e native -f test_runtime_gnss` -> PASS (`15/15`).

Self-review:
- This change deliberately leaves phone GPS as a visible fix source for telemetry/UI while keeping `controlGpsAvailable()` and corrected control heading hardware-only.
- No Flutter, anchor UI, BLE command surface, or hardware pinout files were touched.

### 2026-04-26 Stage 183: Anchor enable preflight

Scope:
- Add a minimal app-side preflight gate before the user can send `ANCHOR_ON`.
- Keep firmware gating unchanged and test the visible readiness logic as a pure Flutter helper.

External baseline:
- Reused the Stage 179 commercial GPS-anchor baseline: anchor lock should be an explicit mode transition with visible readiness, not a blind command.
- `docs/BLE_PROTOCOL.md` remains the local contract: `ANCHOR_ON` is allowed only after a saved anchor, onboard heading, and GNSS gate pass.

Key outcomes:
- Added `buildAnchorPreflight()` to evaluate BLE link, auth, saved anchor, GNSS gate, heading, safety latch/failsafe, stepper config, and STOP reachability from current telemetry.
- `MapPage` now shows a checklist dialog before `ANCHOR_ON` and disables the enable button when any visible prerequisite fails.
- Updated the active TODO checklist for the preflight item.

Validation:
- `cd boatlock_ui && dart format lib/models/anchor_preflight.dart lib/pages/map_page.dart test/anchor_preflight_test.dart` -> PASS.
- `cd boatlock_ui && flutter test test/anchor_preflight_test.dart` -> PASS (`4/4`).
- `cd boatlock_ui && flutter analyze && flutter test` -> PASS, no analyzer issues, tests `71/71`.

Self-review:
- Motor readiness is still bounded by available telemetry; the app can only check stepper config fields today, while firmware remains the final authority for actuation gates.
- A full readiness panel with link age, hardware GPS source, current/power, and richer blocked reasons remains a separate P1/P0 follow-up before powered bench.
