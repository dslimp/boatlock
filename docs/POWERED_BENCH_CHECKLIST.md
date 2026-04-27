# BoatLock Powered Bench Checklist

This checklist is the required gate before connecting the brushed motor,
steering motor, prop load, or water load. It complements `docs/HARDWARE_NH02.md`;
use the tracked wrappers there for flash, acceptance, and Android BLE smokes.

## Current Known Outputs

Firmware source of truth is `boatlock/main.cpp`.

- Brushed motor PWM: `GPIO7`
- Brushed motor direction: `GPIO8` and `GPIO10`
- Current steering firmware path: DRV8825-compatible `STEP/DIR` through
  AccelStepper `DRIVER` pins `STEP=GPIO6`, `DIR=GPIO16`
  with `7200` output steps/rev from the Vanchor `36:1` gearbox.
- Hardware STOP button: `GPIO15`, `INPUT_PULLUP`, momentary to `GND`
- BOOT anchor-save button: `GPIO0`

Do not connect a powered brushed motor driver until its driver type, pinout,
brake/coast/enable behavior, current limit, fault/current signals, polarity,
idle behavior, and STOP behavior are documented and tested. Use
`docs/BRUSHED_MOTOR_DRIVER_INTAKE.md` to capture those facts before any powered
thruster connection.

Do not connect powered steering until the DRV8825 pinout, steps/rev, gear ratio,
current limit, torque limit, enable/sleep/reset wiring, idle behavior, and stop
behavior are documented and tested. Use
`docs/STEERING_DRIVER_INTAKE.md` to capture those facts before any powered
steering connection.

## Required Bench Hardware

- Current-limited DC supply for motor testing.
- Fuse or breaker sized below the weakest wire/driver element.
- Physical kill switch or removable power link reachable by the operator.
- Meter or logic analyzer for `PWM=7`, `DIR=8`, `DIR=10`, and steering outputs.
- Separate low-current logic supply proof before motor power is applied.
- Strain relief on battery/motor/driver wires.
- Insulated terminals; no exposed high-current contacts.
- Polarity labels on supply, driver, motor, and any quick disconnects.
- Thermal check method: finger-safe touch check for low power, IR thermometer or
  probe before longer runs.
- Prop removed or the motor mechanically unable to thrust during dry powered
  tests.

## Gate 0: Local And Bench Software

Run these before touching powered outputs:

```bash
tools/ci/run_local_prepowered_gate.sh
```

Equivalent manual sequence:

```bash
cd boatlock && platformio test -e native
cd boatlock && pio run -e esp32s3
python3 tools/sim/test_sim_core.py
python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json
python3 tools/sim/run_sim.py --scenario-set all --check --json-out tools/sim/all_report.json
python3 tools/sim/test_soak.py
python3 tools/sim/run_soak.py --hours 6 --check --json-out tools/sim/soak_report.json
cd boatlock_ui && flutter analyze && flutter test
pytest tools/ci/test_*.py
```

Then flash and run the tracked bench path:

```bash
tools/hw/nh02/flash.sh
tools/hw/nh02/acceptance.sh --seconds 180
tools/hw/nh02/android-run-app-e2e.sh --status --wait-secs 130
tools/hw/nh02/android-run-app-e2e.sh --manual --wait-secs 130
tools/hw/nh02/android-run-app-e2e.sh --anchor --wait-secs 130
tools/hw/nh02/android-run-app-e2e.sh --sim --wait-secs 130
tools/hw/nh02/run-sim-suite.sh
tools/hw/nh02/android-run-app-e2e.sh --reconnect --wait-secs 130
tools/hw/nh02/android-run-app-e2e.sh --esp-reset --wait-secs 130
```

If hardware GPS is visible, also run:

```bash
tools/hw/nh02/android-run-app-e2e.sh --gps --wait-secs 180
```

## Gate 1: No-Load Output Instrumentation

Motor driver power disconnected. Steering motor/driver output disconnected
unless it is passive logic-level observation only.

Record each check with command, expected output, measured output, and pass/fail:

| Case | Expected motor output | Expected steering output |
| --- | --- | --- |
| Boot before BLE connect | `PWM=0`, direction stable/no chatter | idle/no stepping |
| BLE connected, `IDLE` | `PWM=0` | idle |
| `STOP` command | `PWM=0`, no pulse after STOP | idle, any active move cancelled |
| Hardware STOP press | same as `STOP` command | same as `STOP` command |
| `MANUAL_SET:0,0,1000` | `PWM=0` | no steering move |
| `MANUAL_SET:0,20,1000` | bounded PWM pulse only while lease is fresh | no steering move unless steer non-zero |
| Manual lease expiry | `PWM=0` after TTL | idle |
| `MANUAL_OFF` | `PWM=0` | idle |
| `ANCHOR_ON` denied on bench | `PWM=0` | idle |
| `ANCHOR_OFF` | `PWM=0` | idle |
| `SIM_RUN` then `SIM_ABORT` | simulation smoke must not drive real outputs | idle |
| full on-device `sim_suite` | standard HIL suite must not drive real outputs | idle |
| BLE disconnect/reconnect | `PWM=0` | idle |
| ESP reset | `PWM=0` during and after reboot | idle |
| OTA begin/abort | `PWM=0` | idle |

Any unexpected pulse, repeated twitch, direction chatter, or output that
survives STOP is a blocker. Fix firmware/wiring before applying motor power.

## Gate 2: Low-Power Driver Proof

Only after Gate 1 passes:

1. Complete `docs/STEERING_DRIVER_INTAKE.md` for the current DRV8825 steering
   wiring and motor/mechanics.
2. Complete `docs/BRUSHED_MOTOR_DRIVER_INTAKE.md` for the actual brushed motor
   driver unless it has already been captured against this firmware commit.
3. Connect the driver with current-limited supply and no prop/load.
4. Verify polarity before energizing.
5. Set the supply current limit low enough that a wiring mistake cannot damage
   the driver or wiring.
6. Boot into `IDLE` and confirm no motion.
7. Send short press-and-hold manual pulses only.
8. Verify direction labels: forward/reverse and steering left/right.
9. Release controls and verify motion stops within the manual TTL.
10. Send BLE `STOP` and press hardware STOP; both must stop immediately.
11. Power-cycle and verify there is no auto-anchor or stored actuation.
12. Touch/measure driver, motor, wiring, and connectors after each short run.

Abort on heat, smell, voltage sag, unstable reset, reverse direction ambiguity,
or any output that continues after release/STOP.

## Gate 3: Dry Steering Integration

Do not enable thrust. Steering can be attached to the turn mechanism only if the
mechanism cannot hit a hard stop at speed.

- Mark physical bow-zero.
- Use service setup to capture bow-zero only after the mechanics are known.
- Check manual left/right and release-to-idle.
- Command small target bearing changes and watch for hunting.
- Check cable wrap, mechanical limits, and jam behavior manually before powered
  steering.

If the steering hardware is not the current DRV8825-compatible STEP/DIR path,
stop here and add the real driver support before continuing.

## Gate 4: Wet Or Tethered Low-Power Test

- Use protected water or tank/tether.
- Keep thrust cap low and start manual-only.
- Verify STOP, app background/lock, BLE loss, reconnect, and manual TTL.
- Only then try anchor preflight, denial cases, and short quiet-profile holds.
- Keep a hard power-removal path within reach.

## Required Log Fields

Every powered-bench run should record:

- Date, firmware commit, app commit, and operator.
- Supply voltage/current limit and fuse/breaker rating.
- Brushed motor driver intake reference, steering intake reference, and wiring
  photo reference.
- Motor and steering driver temperatures before/after each phase.
- Exact BLE/e2e command or wrapper used.
- Measured `PWM=7`, `DIR=8`, `DIR=10`, and steering output behavior.
- Any reset, brownout, latch, STOP, or unexpected motion.

Use `docs/PROTECTED_WATER_TEST_LOG.md` for powered bench, tank, tether, and
protected-water sessions. It is the required external voltage/current/power
measurement path until firmware publishes real power telemetry.
