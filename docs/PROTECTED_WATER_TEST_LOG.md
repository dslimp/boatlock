# BoatLock Protected-Water Test Log

Use one copy of this log per powered bench, tank, tether, or protected-water
session. This is the required external measurement path until BoatLock has
real battery/current/power telemetry.

## Session

- Date/time:
- Location/water body:
- Operators:
- Recovery method:
- Weather/wind/current/wave estimate:
- Firmware commit:
- App commit:
- Boat/load mass estimate:
- Motor driver model:
- Steering driver/mechanics:
- Battery/supply:
- Fuse/breaker rating:
- Physical kill path:

## Pre-Run Gate

| Check | Result | Notes |
| --- | --- | --- |
| `docs/POWERED_BENCH_CHECKLIST.md` Gate 0 passed |  |  |
| `tools/ci/run_local_prepowered_gate.sh` passed on this commit |  |  |
| Correct firmware profile flashed for this phase |  |  |
| `nh02` acceptance passed after flash |  |  |
| Android status/manual/reconnect smokes passed after flash |  |  |
| No-load output instrumentation passed |  |  |
| DRV8825/Vanchor steering intake complete |  |  |
| Prop/load safe for current phase |  |  |
| STOP command tested before launch |  |  |
| Hardware STOP tested before launch |  |  |
| Manual release-to-stop tested before launch |  |  |
| BLE reconnect tested before launch |  |  |
| microSD inserted and `[SD] logger ready=1` seen after boot |  |  |
| GNSS fix visible |  |  |
| Heading fresh |  |  |
| Anchor preflight blocks/accepts as expected |  |  |

## Phase Advancement Rules

Do not advance to the next phase in the same session unless every previous phase
has a clean pass, the operators agree the recovery path is still available, and
the battery/supply, driver, wiring, and motor temperatures remain within the
recorded limits. A phase with any abort condition becomes the last phase of the
session.

| Phase | Entry condition | Maximum first-run exposure | Pass condition | Next phase allowed |
| --- | --- | --- | --- | --- |
| Float telemetry only | Outputs disabled or physically unable to thrust | 5 min | BLE, GNSS, heading, readiness, logs, display, and STOP behave as expected with no actuation | Manual low-power |
| Manual low-power pulses | Current limit set low, hard power removal ready | 1 s pulses, then 3 s pulses | Press-and-hold only, release-to-stop, BLE loss quiets output, reconnect does not resume motion | Steering-only |
| Steering-only | Thrust disabled; bow-zero and travel limits proven | 10 small left/right moves | Direction labels correct, no hunting, no hard-stop contact, idle/hold behavior safe | Anchor preflight |
| Anchor preflight | Saved anchor and current fix visible; quiet profile selected | No sustained thrust until denied cases are observed | Denied reasons are correct; accepted only with fresh heading and good GNSS | Quiet anchor 30-60 s |
| Quiet anchor 30-60 s | Recovery boat/tether ready; low thrust cap | One 30 s hold, then one 60 s hold | No repeated twitching, no saturation, STOP/manual recovery works | Longer hold |
| Longer hold | Previous quiet holds clean | 5 min, then 10 min | Drift stays bounded, thermal/current behavior stable, no failsafe except intentional abort | Controlled disturbance |
| Controlled disturbance | Operators explicitly approve disturbance | One small disturbance at a time | System remains bounded or aborts quietly; STOP still preempts | End session or repeat later |

## Required Captures

- Photo of wiring, fuse/breaker, kill path, battery/supply, motor driver, and
  steering driver before power is applied.
- Screenshot or exported app state before each phase: readiness, GNSS,
  heading, anchor, failsafe, manual lease, and motor readiness.
- microSD `/boatlock/*.jsonl` logs for the whole session, plus Serial/BLE logs
  for boot markers, connection state, and STOP tests.
- External voltage/current readings for idle, pulse, peak, and post-run states.
- Track/drift notes for every anchor phase, even if the run is aborted early.
- Weather and water notes: wind estimate, current estimate, wave/wake exposure,
  and nearby traffic.

## Power Measurements

Record external meter/clamp/supply readings at each phase. If using a bench
supply, record both set limits and observed values.

| Phase | Duration | Voltage start/end | Current idle/peak | Power peak | Fuse/breaker | Battery/supply temp | Driver temp | Motor temp | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Float telemetry only |  |  |  |  |  |  |  |  |  |
| Manual low-power pulses |  |  |  |  |  |  |  |  |  |
| Steering-only |  |  |  |  |  |  |  |  |  |
| Anchor preflight denied |  |  |  |  |  |  |  |  |  |
| Quiet anchor 30-60 s |  |  |  |  |  |  |  |  |  |
| Longer hold |  |  |  |  |  |  |  |  |  |
| Disturbance/abort |  |  |  |  |  |  |  |  |  |

## Event Log

Record every command, mode change, STOP, disconnect, failsafe, or unexpected
motion. Use exact wrapper/command names where possible.

| Time | Command/event | App mode/status | GNSS Q | Heading Q | Distance/bearing | PWM/current | Result |
| --- | --- | --- | --- | --- | --- | --- | --- |
|  |  |  |  |  |  |  |  |

## Abort Conditions

Abort the session immediately on any of these:

- unexpected actuation
- control remains active after release or STOP
- hardware STOP has no immediate effect
- BLE loss leaves controls enabled from stale state
- compass loss or repeated heading jumps
- GNSS jump or stale fix during anchor enable
- steering hunting, jam, cable wrap, or hard-stop contact
- unbounded PWM/current saturation
- voltage sag, brownout, reset, heat, smell, water ingress, or wiring movement
- any operator uncertainty

## Post-Run Review

- Final battery/supply voltage:
- Max observed current:
- Max observed power:
- Max driver/motor/wiring temperature:
- Any fuse/breaker/kill switch action:
- Any reset/brownout:
- Any failsafe reason:
- Track/drift notes:
- Firmware/app changes required before next run:
- Next allowed phase:
- Simulator calibration data extracted:
- Decision: repeat same phase / advance / stop until fix:
