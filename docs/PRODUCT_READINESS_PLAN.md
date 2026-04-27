# BoatLock Product Readiness Plan

Date: 2026-04-26

This plan prepares BoatLock for powered bench tests and first protected-water
tests after the repo-wide refactor. Code remains the source of truth; this file
records the product and validation backlog that should drive the next cuts.

## Product Goal

`main` should stay focused on the releasable BoatLock path:

- app connection/reconnection over BLE
- secure owner pairing/auth for write/control commands
- set anchor, enable anchor, disable anchor, emergency stop
- deadman-protected manual control from phone and future BLE remote
- anchor hold from hardware GNSS plus onboard BNO08x heading
- deterministic failsafe behavior with no unexpected actuation
- enough status to understand link, fix, heading, anchor, safety, and actuation

Do not restore route navigation, phone-heading fallback, legacy split manual
commands, or compatibility shims unless a later task explicitly restores them
end to end.

## Commercial Baseline

Current commercial GPS-anchor products set the user expectation:

- Minn Kota Spot-Lock exposes lock/unlock from multiple control surfaces,
  small-step 5 ft jog, and distance-to-spot telemetry:
  <https://minnkota.johnsonoutdoors.com/us/learn/technology/spot-lock>
- Minn Kota One-Boat Network app puts speed, steering, Spot-Lock, AutoPilot,
  Drift Mode, and update/support flows in one app:
  <https://minnkota.johnsonoutdoors.com/us/learn/technology/one-boat-network-app>
- Minn Kota Advanced GPS docs treat heading sensor calibration/offset and
  boat-scale tuning as setup requirements, not hidden debug details:
  <https://minnkota-help.johnsonoutdoors.com/hc/en-us/articles/23607178243991-Using-Advanced-GPS-Navigation-Features-and-Manual-2023-present>
  and
  <https://minnkota-help.johnsonoutdoors.com/hc/en-us/articles/31296915731735-Using-the-Advanced-GPS-Navigation-Micro-Remote>
- Garmin Force separates manual, heading hold, anchor lock, and jog behavior on
  the remote:
  <https://www8.garmin.com/manuals/webhelp/GUID-91136105-7EB2-442E-8C25-7B4CF00FC466/EN-US/GUID-86396A06-E8A7-4E7B-97F5-A638D9DBAB49.html>
- Lowrance Ghost gives physical feedback for Anchor/Heading modes and exposes
  battery, pairing, and mode state at a glance:
  <https://www.lowrance.com/lowrance/type/trolling-motor/ghost-47/>
- MotorGuide Pinpoint GPS includes anchor, jog, heading lock, cruise control,
  wireless foot/hand remote, and chartplotter integration:
  <https://shop.motorguide.com/products/940800280.html>
- Rhodan/Raymarine integration exposes Anchor Here, Hold Heading, speed/thrust
  control, 5 ft increments, and wind/current compensation:
  <https://www.raymarine.com/en-us/learning/online-guides/rhodan-trolling-motor-integration>
- Minn Kota sizing guidance says thrust must be sized by loaded boat weight and
  increased for wind/current:
  <https://minnkota.johnsonoutdoors.com/us/learn/buying-guide/trolling-motors>

BoatLock should not copy the full fishing-feature matrix. The useful baseline is
the interaction model: explicit modes, visible readiness, small jog, separate
setup/calibration, conservative power sizing, and clear errors.
The focused comparison is tracked in `docs/COMMERCIAL_ANCHOR_UX_REVIEW.md`.

## Current Fit

What matches the target:

- Firmware orchestration is now mostly glue in `boatlock/main.cpp`, with mode
  arbitration, GNSS, motion, telemetry cadence, anchor gate, sim execution, and
  status split into narrower modules.
- BLE protocol uses fixed UUIDs, fixed-size live telemetry, command validation,
  pairing/auth envelope, logs, and OTA partition safety.
- Manual control uses atomic `MANUAL_TARGET:<angleDeg>,<throttlePct>,<ttlMs>` and
  `MANUAL_OFF`, with a short deadman TTL and source-owned lease.
- Multi-client control ownership is now specified as a future implementation
  contract: read-only telemetry clients may coexist, but only one eligible
  controller, authenticated when paired, can hold the control lease.
- `ANCHOR_ON` is behind anchor-point, heading, and GNSS quality gates.
- STOP/failsafes latch quiet `HOLD` instead of auto-entering manual control.
- On-device HIL `S0..S19` plus `RF0..RF4` and offline `tools/sim` are real validation assets and
  should stay in `main`.
- Native production-path tests now cover the shared
  `RuntimeMotion -> StepperControl -> MotorControl` quiet-output cases for STOP,
  Manual-to-HOLD, Anchor-to-HOLD, and DriftFail.

Important mismatch:

- On-device HIL exercises `AnchorControlLoop` through virtual sensor/actuator
  interfaces, while production actuation goes through the real runtime motion and
  driver classes. HIL pass is useful, but it does not prove powered steering
  direction, torque/current limits, end stops, or water-loaded motor behavior.

## Priority Gaps

### P0 Before Powered Bench

- Keep app command acceptance explicit: operator UI must distinguish command
  sent from telemetry/log-confirmed acceptance, especially for setup and
  acceptance workflows.
- Prove app-visible firmware command-profile rejections on `nh02` and Android so
  release firmware blocking setup/dev/HIL commands cannot look like a silent
  write.
- Resolve paired-mode auth policy for raw safety commands: either document and
  test an intentional emergency exception, or require `SEC_CMD` for all
  control/write commands that refresh safety state.
- Prove or fix GPS-to-compass correction source ownership. Phone GPS fallback is
  display/telemetry only and must not indirectly bias control heading correction.
- Prove BLE command profiles end-to-end on `nh02` and Android: release rejection
  logs, setup positive paths, and acceptance HIL positive paths.
- Identify the exact brushed/collector motor driver with
  `docs/BRUSHED_MOTOR_DRIVER_INTAKE.md`. Current firmware only proves a simple
  `PWM=7` plus `DIR=8/10` command shape; it does not prove the real driver's
  brake/coast, enable, fault, current-limit, thermal, polarity, or safe-idle
  behavior.
- Finish the steering stepper driver and mechanics capture. Current firmware is
  fixed to a DRV8825-compatible STEP/DIR path on `STEP=GPIO6`, `DIR=GPIO16`.
  Geometry uses `StepSpr=200` motor STEP pulses/rev and `StepGear=36` from the
  Vanchor gearbox, producing `7200` output-shaft STEP pulses/rev. Use
  `docs/STEERING_DRIVER_INTAKE.md` to capture
  torque/current limits, enable/sleep/reset wiring, stop behavior, and tests.
- Build a powered-bench acceptance procedure for `PWM=7`, `DIR=8/10`, the actual
  brushed motor driver, and the actual steering drive. Before connecting the
  prop/motor load, prove boot/STOP/HOLD/reconnect/anchor-denial keep outputs at
  safe idle.
- Add current-limited power, fuse/breaker, physical kill path, cable strain relief,
  and polarity proof before any real motor spin.
- Update wiring docs every time hardware truth changes; do not rely on stale
  README snippets.

### P1 Before First Water

- Turn the protected-water checklist into app-driven readiness where practical:
  live-frame age, RSSI, auth/session, GNSS/heading freshness, active firmware
  profile, STOP proof, manual-release proof, and active blockers.
- Keep current anchor controls covered by tests: current-position save,
  map-point save, enable/disable, fixed 1.5 m jog, distance/bearing, and blocked
  reasons.
- Add conservative anchor profiles for water trials: `quiet`, `normal`, `current`,
  with `quiet` as the default for first powered tests.
- Add export/screenshot bundle for water-test sessions after the event shape is
  stable.
- Add battery/power telemetry later; for the first powered bench and protected
  water sessions, `docs/PROTECTED_WATER_TEST_LOG.md` is the required external
  voltage/current/power measurement path. Motor sizing and runtime are not
  product polish; they directly bound safety.
- Remove current main-branch leftovers that confuse release scope: old route UI
  labels, unused persisted knobs such as `ReacqStrat`, and board-specific
  `BOATLOCK_BOARD_JC4832W535` branches unless they are explicitly preserved on a
  feature branch.

### P2 Product Polish After Safe Water Proof

- Add heading-hold as a separate operator concept from position hold in the app.
- Add setup flows for heading offset, steering bow-zero, and boat
  scale style tuning.
- Implement the documented second-controller gate in `docs/MANUAL_CONTROL.md`
  and `docs/BLE_PROTOCOL.md`: per-client identity/auth, read-only telemetry
  clients, single control-owner lease, conservative takeover, remote role
  allowlist, STOP preemption, and two-central tests before accepting a real BLE
  remote.
- Optimize BLE OTA throughput only after the powered path is stable; keep SHA-256,
  auth envelope, abort-on-disconnect, and USB flash recovery.

## Simulation Plan

Keep two layers:

- Offline Python sim for fast environmental sweeps and research-backed scenario
  generation.
- On-device HIL for release-firmware `SIM_*` acceptance, simulated BLE map telemetry, and BLE/app smoke.

Next simulation work:

- Keep schema v2 Russian scenarios with provenance/confidence, loaded mass,
  submerged drag, and windage as the current baseline.
- Improve provenance granularity and confidence calibration only after real
  bench/water logs exist.
- Add yaw moment and heading inertia so current, wind/gust, and wave forcing can
  rotate the hull, not only translate it.
- Add river/reservoir wake events and short steep chop as separate transient
  disturbances instead of folding them into steady wave height.
- Extend current wave/rocking support from hull-motion metrics to sensor-frame
  effects on BNO08x/GNSS samples: roll, pitch, heave, heading jitter, and
  measurement quality changes.
- Extend reports and thresholds for pitch, heave, heading jitter, degraded GNSS
  time, wake/chop active time, and environmental yaw rate.
- Keep motor and steering hardware-calibration simulation work blocked on real
  driver identity and logs.

## Bench Plan

1. Local validation:
   `tools/ci/run_local_prepowered_gate.sh`, or the equivalent manual sequence:
   `cd boatlock && platformio test -e native`,
   `cd boatlock && pio run -e esp32s3`,
   `python3 tools/sim/test_sim_core.py`,
   `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json`,
   `python3 tools/sim/test_soak.py`,
   `python3 tools/sim/run_soak.py --hours 6 --check --json-out tools/sim/soak_report.json`,
   `cd boatlock_ui && flutter test`,
   `pytest tools/ci/test_*.py`.
2. NH02 no-load gate:
   `tools/hw/nh02/flash.sh`,
   `tools/hw/nh02/acceptance.sh --seconds 180`,
   Android `--status`, `--sim`, `--anchor`, `--manual`, `--reconnect`,
   `--esp-reset`, plus GPS smoke when hardware GPS is visible.
3. Output-instrumented gate:
   motor physically disconnected, logic analyzer or meter on `PWM=7`, `DIR=8/10`,
   steering driver outputs observed, prove idle on boot, STOP, HOLD, reconnect,
   anchor denial, SIM start/abort, and OTA begin.
4. Low-power powered gate:
   complete `docs/STEERING_DRIVER_INTAKE.md` for the current DRV8825 steering
   path, then use current-limited supply,
   fuse/breaker, prop removed or safe test load, real driver connected, short
   manual pulses, direction proof, STOP proof, thermal check, no auto anchor.
5. Integrated dry steering gate:
   steering motor attached to the turn mechanism without thrust load, bow-zero
   captured, manual left/right, auto point-to-bearing with compass rotation,
   release/idle behavior, jam/limit observation.
6. Wet/tank/tether gate:
   motor submerged or safe water load, very low thrust cap, manual-only first,
   then anchor enable denial/proof, then short anchor holds.

## First Water Plan

Start only in protected small lake/cove conditions: low wind, minimal current,
two operators, physical recovery available, and a hard way to remove power.
Use `docs/PROTECTED_WATER_TEST_LOG.md` as the session checklist and do not
advance phases after any abort condition.

Phases:

1. Float and telemetry only: no powered output. Verify BLE, GNSS, heading,
   readiness panel, logs, display, and STOP.
2. Manual low-power: short target-angle/PWM windows, TTL expiry, BLE loss,
   phone lock/app background, hardware STOP, reconnect.
3. Steering-only: motor thrust disabled, verify stepper/steering direction,
   bow-zero, release, and no hunting.
4. Anchor preflight denied/accepted: verify denied reasons first, then accepted
   only with `gnssQ=2`, fresh heading, saved anchor, and auth.
5. Short anchor hold with quiet profile: 30-60 seconds, low thrust cap,
   containment close, manual STOP recovery.
6. Longer hold: 5-10 minutes, observe drift track, PWM, heading error, stepper
   movement, battery/current, and no repeated twitching.
7. Controlled disturbances: small wind/current changes, wakes if safe, then
   explicit abort. Do not test flood/current/storm classes until containment and
   powered bench logs are clean.

Abort immediately on unexpected actuation, stale UI control enabled after link
loss, missing STOP effect, compass loss, GNSS jump, repeated steering thrash,
unbounded PWM saturation, water ingress, overheated wiring/driver, or uncertain
operator state.
