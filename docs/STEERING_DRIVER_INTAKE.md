# BoatLock Steering Driver Intake

Use this checklist before connecting or commanding the real steering drive. The
current firmware path assumes a DRV8825-compatible STEP/DIR driver on
`STEP=GPIO6` and `DIR=GPIO16`, with `StepSpr=200` motor STEP pulses/rev and
`StepGear=36` from the Vanchor gearbox, for `7200` output-shaft STEP pulses per
steering revolution. Capture the remaining hardware facts before powered
steering. If DRV8825 MODE pins are set for microstepping instead of full-step,
multiply `StepSpr` by the active microstep factor and keep `StepGear` as the
mechanical reduction ratio.

This intake is hardware-independent. It should produce enough evidence to decide
whether firmware can safely support the installed driver and mechanics, but it
does not by itself approve powered steering tests.

## Intake Status

- Date:
- Operator:
- Hardware location:
- Photo/video folder or ticket:
- Driver status: `unknown` until all identity, pinout, power, direction, idle,
  limit, and jam checks below are complete.
- Firmware status: STEP/DIR support exists for DRV8825 on `GPIO6/GPIO16` with
  `StepSpr=200`, `StepGear=36`; powered use remains blocked until the remaining
  driver and mechanics facts are captured.

## Ground Rules

- Keep steering motor power disconnected until driver identity, pinout, supply
  voltage, current limit, enable/idle behavior, and stop behavior are known.
- Use a current-limited supply for every powered step, starting below the
  expected operating current.
- Keep thrust disabled and prop load removed while steering is being identified.
- Treat any unlabelled wire, module, or connector as unknown.
- Stop on heat, smell, brownout, unexpected movement, direction ambiguity,
  end-stop uncertainty, or output that survives STOP/release.
- Do not mark the steering-driver TODO complete until the real driver and
  mechanics are identified and the needed firmware/docs changes are made.

## Driver Identity

Record direct evidence, not guesses.

| Field | Value |
| --- | --- |
| Driver board/module marking | |
| Driver IC marking | |
| Seller/manufacturer/link/datasheet | |
| Driver family | `unipolar coil sink`, `bipolar step/dir`, `dual H-bridge`, `servo/ESC`, `closed-loop`, `unknown` |
| Is it A4988-compatible STEP/DIR? | `yes/no/unknown`, with proof |
| Is it ULN2003-compatible 4-phase input? | `yes/no/unknown`, with proof |
| Motor marking/model | |
| Connector names and pin count | |
| Any onboard DIP switches/jumpers/pots | |
| Thermal protection or fault output | |
| Enable/sleep/reset input behavior | |

Required evidence:

- Clear photos of both sides of the driver board.
- Clear photos of motor label, connector, cable colors, and steering linkage.
- Datasheet or seller wiring diagram if available.
- Continuity or pin-trace notes for every signal and power connector.

## Pinout Capture

Do not connect the ESP32 until this table is filled and reviewed.

| Driver pin | Observed label | Direction | Electrical level | Connects to | Proof |
| --- | --- | --- | --- | --- | --- |
| | | `input/output/power/motor/limit` | | | |

Minimum pinout facts:

- Logic supply voltage and tolerance.
- Motor supply voltage and polarity.
- Ground reference and whether logic/motor grounds must be common.
- Enable, sleep, reset, fault, or mode pins and their safe default states.
- Step, direction, phase, PWM, or command pins and their voltage thresholds.
- Motor coil pins or actuator output pins.
- End-stop/limit switch pins if present.
- Any fault/current-sense output and its polarity.

Blocker: if any pin can energize the motor and its active level is unknown, do
not connect it to the firmware board.

## Voltage And Current Limits

| Item | Value | Evidence |
| --- | --- | --- |
| Logic voltage | | |
| Motor/driver supply range | | |
| Starting bench supply voltage | | |
| Starting current limit | | |
| Rated run current | | |
| Rated peak/stall current | | |
| Driver current-limit setting | | |
| Fuse/breaker rating | | |
| Wire/connector current rating | | |
| Max acceptable driver temperature | | |
| Max acceptable motor temperature | | |

Current-limit proof:

- Record how the limit is set: bench supply limit, sense resistor, trimpot,
  firmware parameter, DIP switch, or fixed driver behavior.
- Measure idle current with motor enabled and disabled.
- Measure short low-speed movement current.
- Record whether hold current remains applied after movement stops.
- Record whether STOP, manual lease expiry, BLE disconnect, and ESP reset remove
  or reduce hold current.

## Motor And Mechanics

| Field | Value |
| --- | --- |
| Motor type | `stepper`, `DC gearmotor`, `servo`, `linear actuator`, `unknown` |
| Motor steps/rev or encoder counts/rev | |
| Microstep setting, if any | |
| Gear ratio before steering output | |
| Steering output degrees per motor rev | |
| Total steering travel in degrees | |
| Backlash or dead band | |
| Coupler/slip clutch behavior | |
| Cable-wrap risk | |
| Mechanical hard stops | |
| Expected operating speed | |
| Expected steering torque/load | |

If the driver is stepper-based, derive:

- Motor full steps per rev.
- Microsteps per full step.
- Gearbox ratio.
- Steering output degrees per motor step or microstep.
- Safe max speed and acceleration at low voltage/current.

If the actuator is not stepper-based, capture the closed-loop or feedback model
needed before firmware support is designed.

## End Stops And Limits

| Field | Value |
| --- | --- |
| Physical port-side stop | |
| Physical starboard-side stop | |
| Electrical end stops present | `yes/no/unknown` |
| Limit switch type | `normally closed/normally open/analog/driver fault/none` |
| Limit active polarity | |
| Limit debounce/filter requirement | |
| Limit failure-safe state | |
| Travel margin before hard stop | |
| Manual recovery path after limit hit | |

Required proof:

- With power off, move the mechanism by hand and mark both safe travel limits.
- Confirm cable routing cannot bind before either travel limit.
- If switches exist, measure their idle and active states at the firmware-side
  connector or driver output.
- Prefer normally-closed limit wiring for fail-closed detection when hardware
  allows it.
- Record whether the driver itself stops on a limit or only reports it.

Blocker: no powered steering movement if a hard stop can be hit before firmware
can detect or limit travel.

## Direction Proof

Perform direction proof only after identity, pinout, and current limit are
complete.

| Step | Command/input | Expected | Observed | Pass |
| --- | --- | --- | --- | --- |
| Logic-only left | | no powered motion; expected signal polarity only | | |
| Logic-only right | | no powered motion; expected signal polarity only | | |
| Low-power single left pulse | | steering output moves left/port | | |
| Low-power single right pulse | | steering output moves right/starboard | | |
| Release/deadman expiry | | movement stops and idle state is safe | | |
| STOP command | | movement stops and output is safe | | |
| Hardware STOP | | movement stops and output is safe | | |
| ESP reset | | no unexpected movement during or after boot | | |

Record:

- Which observed direction corresponds to positive `MANUAL_TARGET` angle.
- Whether firmware direction must be inverted.
- Whether driver enable must be asserted only during movement or may hold after
  movement.
- Whether the mechanism twitches on driver enable, sleep exit, or reset.

Blocker: if left/right is ambiguous at the steering output, do not proceed to
bow-zero or anchor pointing.

## Idle And Hold Behavior

| Case | Expected safe result | Observed |
| --- | --- | --- |
| Driver powered, ESP off | no movement; safe idle or disabled output | |
| ESP boot | no movement before BLE connect | |
| BLE connected `IDLE` | no movement; safe current level | |
| Manual release/TTL expiry | no movement; safe current level | |
| `MANUAL_OFF` | no movement; safe current level | |
| `STOP` | no movement; safe current level | |
| BLE disconnect | no movement; safe current level | |
| ESP reset/brownout | no movement; safe current level | |

Record whether hold current is required to keep the steering angle against load.
If hold current is required, define the maximum allowed hold current, thermal
limit, timeout policy, and STOP behavior before firmware support is accepted.

## Jam And Limit Proof

Perform jam/limit proof at the lowest current and torque that can produce
measurable movement. Do not use water load or thrust during this stage.

| Case | Method | Expected | Observed | Pass |
| --- | --- | --- | --- | --- |
| Soft manual resistance | hand-applied load within safe range | no chatter, no runaway current | | |
| Port limit approach | slow movement toward marked limit | stop before hard contact | | |
| Starboard limit approach | slow movement toward marked limit | stop before hard contact | | |
| Limit switch active | activate switch without hard contact | driver/firmware stops or blocks motion | | |
| Simulated jam | safe fixture or low-force block | current/timeout/fault stops motion | | |
| Recovery from limit/jam | release or opposite command | only safe recovery direction allowed | | |

Record:

- Current at first missed step, stall, or fault.
- Whether the driver exposes a fault signal.
- Whether firmware needs a movement timeout, current/fault input, soft limits,
  or homing before normal operation.
- Whether mechanical stops are strong enough for accidental contact; do not rely
  on this as a normal control method.

Blocker: if a jam can keep the driver energized without a fault, timeout, or
operator-visible stop condition, powered bench cannot proceed.

## Bow-Zero Procedure

Bow-zero is a setup step, not an automatic calibration.

1. Disable thrust and keep the boat/mount restrained.
2. Confirm driver identity, direction, idle behavior, and travel limits are
   already proven.
3. Mark the physical bow-forward line on the steering output and mount.
4. Move to bow-forward using the smallest low-power manual movement that is
   already proven safe, or by hand if the mechanics permit it.
5. Confirm there is travel margin to both sides and no cable bind at zero.
6. Capture the zero only through the supported setup path for the actual
   driver. The existing `SET_STEPPER_BOW` path is valid only after the firmware
   driver model and steps-per-degree mapping are correct for this hardware.
7. Power-cycle and verify the mechanism does not move unexpectedly and that the
   stored zero is still meaningful.
8. Record the zero mark photo and the exact firmware commit used.

Blocker: do not use bow-zero as a workaround for unknown direction, unknown gear
ratio, slipping linkage, or missing end-stop behavior.

## Firmware And Docs Follow-Up

Once the real hardware facts are known, update the release path in one coherent
change. Required decision points:

- `boatlock/main.cpp`: replace or confirm the steering pin map and any driver
  construction constants.
- Steering driver abstraction: add an explicit path if the driver is not the
  current DRV8825-compatible STEP/DIR stack.
- Motion/runtime logic: apply measured steps-per-degree, max speed,
  acceleration, travel limits, idle/hold-current policy, and fault/limit
  handling.
- Settings/schema: add or remove user/setup settings only if the hardware
  needs configurable limits; update `docs/CONFIG_SCHEMA.md` with any schema
  change.
- BLE protocol: update `docs/BLE_PROTOCOL.md` only if commands, telemetry, or
  settings accepted over BLE change.
- Tests: add native or HIL coverage for direction inversion, quiet idle,
  STOP/lease expiry, limit/jam handling, and bow-zero persistence as applicable.
- `docs/POWERED_BENCH_CHECKLIST.md`: replace unknown-driver gates with the real
  supported-driver procedure.
- `docs/PRODUCT_READINESS_PLAN.md` and `boatlock/TODO.md`: keep the steering
  item open until firmware, docs, and validation match the real hardware.
- README/wiring docs/photos: update any stale pinout or driver references from
  real hardware evidence.

Do not preserve compatibility with obsolete steering-driver assumptions unless
there is an explicit product reason to support multiple physical builds.

## Intake Completion Criteria

This intake is complete only when all of these are true:

- Driver IC/module and motor/mechanics are identified from markings, photos, or
  datasheets.
- Pinout and voltage/current limits are measured or sourced.
- End-stop, idle/hold-current, direction, bow-zero, and jam/limit behavior are
  proven at low power.
- Every blocker above is either resolved in hardware or listed as a required
  firmware/docs change before powered steering.
- The real hardware facts are linked from the powered bench log or ticket.

Completion of this intake unblocks firmware planning and the hardware test plan;
it does not mean the current firmware is safe to drive the hardware.
