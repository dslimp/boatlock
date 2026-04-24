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
