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
| No-load output instrumentation passed |  |  |
| Prop/load safe for current phase |  |  |
| STOP command tested before launch |  |  |
| Hardware STOP tested before launch |  |  |
| Manual release-to-stop tested before launch |  |  |
| BLE reconnect tested before launch |  |  |
| GNSS fix visible |  |  |
| Heading fresh |  |  |
| Anchor preflight blocks/accepts as expected |  |  |

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
