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

## Current Fit

What matches the target:

- Firmware orchestration is now mostly glue in `boatlock/main.cpp`, with mode
  arbitration, GNSS, motion, telemetry cadence, anchor gate, sim execution, and
  status split into narrower modules.
- BLE protocol uses fixed UUIDs, fixed-size live telemetry, command validation,
  pairing/auth envelope, logs, and OTA partition safety.
- Manual control uses atomic `MANUAL_SET:<steer>,<throttlePct>,<ttlMs>` and
  `MANUAL_OFF`, with a short deadman TTL and source-owned lease.
- `ANCHOR_ON` is behind anchor-point, heading, and GNSS quality gates.
- STOP/failsafes latch quiet `HOLD` instead of auto-entering manual control.
- On-device HIL `S0..S19` and offline `tools/sim` are real validation assets and
  should stay in `main`.

Important mismatch:

- On-device HIL exercises `AnchorControlLoop` through virtual sensor/actuator
  interfaces, while production actuation goes through
  `RuntimeMotion -> StepperControl -> MotorControl`. HIL pass is useful, but it
  does not prove the exact production stepper/motor path.

## Priority Gaps

### P0 Before Powered Bench

- Fix stale app state on BLE disconnect: when `BleBoatLock` clears its internal
  last data, `MapPage` must also receive `null` or an explicit disconnected
  state so anchor/STOP/settings actions are not enabled from stale telemetry.
- Split app anchor flow into explicit `Save anchor` and `Enable anchor` actions.
  `Enable` must show a preflight checklist: link live, auth ready when paired,
  saved anchor, hardware GNSS gate, fresh BNO08x heading, no latched HOLD/failsafe,
  motors configured, and STOP reachable.
- Add a normal `Anchor off` action to the main UI. Emergency STOP must remain
  separate from ordinary anchor disable.
- Rename the red `STOP` inside the manual sheet to `Manual off` or wire it to the
  real emergency `STOP`. Right now it sends `MANUAL_OFF`, which is unsafe wording.
- Surface write-command failures on the main map. Anchor/map actions currently
  ignore false returns from secure/session failures.
- Prove or fix GPS-to-compass correction source ownership. Phone GPS fallback is
  display/telemetry only and must not indirectly bias control heading correction.
- Classify BLE commands into `release`, `service`, and `dev/HIL`. Keep actuation
  and settings writes authenticated once paired, and keep service/dev commands
  visibly outside the normal water UI.
- Decide the exact steering stepper driver and mechanics. Current firmware is
  fixed to 28BYJ-48 + ULN2003 HALF4WIRE. If the real steering drive is not that
  stack and is also not A4988 STEP/DIR, add an explicit supported driver path with
  pinout, steps/rev, gear ratio, torque/current limits, stop behavior, and tests.
- Build a powered-bench acceptance procedure for `PWM=7`, `DIR=5/10`, the actual
  brushed motor driver, and the actual steering drive. Before connecting the
  prop/motor load, prove boot/STOP/HOLD/reconnect/anchor-denial keep outputs at
  safe idle.
- Add current-limited power, fuse/breaker, physical kill path, cable strain relief,
  and polarity proof before any real motor spin.
- Update wiring docs every time hardware truth changes; do not rely on stale
  README snippets.

### P1 Before First Water

- Add a first-class readiness panel to the app, not only diagnostics:
  live-frame age, BLE state, RSSI, auth state, GNSS quality, hardware GPS age,
  satellites/HDOP when available, heading freshness/quality, anchor gate result,
  current mode, failsafe latch, manual lease state, and motor/stepper configured.
- Add user-visible anchor controls expected from commercial systems:
  current-position save, map-point save, enable/disable, fixed 1.5 m jog controls,
  distance/bearing to anchor, and clear blocked reasons.
- Add conservative anchor profiles for water trials: `quiet`, `normal`, `current`,
  with `quiet` as the default for first powered tests.
- Add track/history around anchor events so drift, correction, and failsafe exits
  can be reviewed after a run.
- Add battery/power telemetry or at least a required external measurement step
  for bench and water logs. Motor sizing and runtime are not product polish; they
  directly bound safety.
- Add `MapPage` widget tests for stale disconnect state, anchor preflight, auth
  reject, STOP, and manual-off wording.
- Remove current main-branch leftovers that confuse release scope: old route UI
  labels, unused persisted knobs such as `ReacqStrat`, and board-specific
  `BOATLOCK_BOARD_JC4832W535` branches unless they are explicitly preserved on a
  feature branch.

### P2 Product Polish After Safe Water Proof

- Add heading-hold as a separate operator concept from position hold in the app.
- Add service-only setup flows for heading offset, steering bow-zero, and boat
  scale style tuning.
- Add optional second-controller design: read-only multi-client telemetry first,
  then per-client auth/control ownership before accepting two real controllers.
- Optimize BLE OTA throughput only after the powered path is stable; keep SHA-256,
  auth envelope, abort-on-disconnect, and USB flash recovery.

## Simulation Plan

Keep two layers:

- Offline Python sim for fast environmental sweeps and research-backed scenario
  generation.
- On-device HIL for firmware-visible `SIM_*` acceptance and BLE/app smoke.

Next simulation work:

- Convert `tools/sim/research/environment_inputs.*` into a normalized scenario
  schema with motor class, loaded boat mass, wind, current, wave, wake, GNSS, and
  compass fields.
- Add windage and yaw moment to the world model.
- Add waves/rocking: significant wave height, peak wave, period, direction,
  roll/pitch/heave perturbations, and their effect on BNO08x/GNSS samples.
- Add river/reservoir wake events and short steep chop.
- Add brushed motor dynamics: voltage, max current, battery sag, current limit,
  thermal derate, deadband/stiction, reverse asymmetry, prop/weed load.
- Add steering mechanics: gear ratio, backlash, current/torque limit, jam/stall,
  wrong bow-zero, and delayed response.
- Add candidate scenarios from Russian water-body research:
  `river_oka_normal_55lb`, `volga_spring_flow_80lb`,
  `rybinsk_fetch_55lb`, `ladoga_storm_abort`, `baltic_gulf_drift`.
- Add a production-path actuator simulation or native tests around
  `RuntimeMotion/MotorControl/StepperControl`, because current HIL does not prove
  those classes together.

## Bench Plan

1. Local validation:
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
   motor physically disconnected, logic analyzer or meter on `PWM=7`, `DIR=5/10`,
   steering driver outputs observed, prove idle on boot, STOP, HOLD, reconnect,
   anchor denial, SIM start/abort, and OTA begin.
4. Low-power powered gate:
   current-limited supply, fuse/breaker, prop removed or safe test load, real
   driver connected, short manual pulses, direction proof, STOP proof, thermal
   check, no auto anchor.
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

Phases:

1. Float and telemetry only: no powered output. Verify BLE, GNSS, heading,
   readiness panel, logs, display, and STOP.
2. Manual low-power: short press-and-hold pulses, release-to-stop, BLE loss,
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
