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
