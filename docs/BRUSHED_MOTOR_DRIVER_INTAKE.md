# BoatLock Brushed Motor Driver Intake

Use this checklist before connecting or commanding the real brushed/collector
thruster motor driver. The current firmware path assumes one PWM output on
`GPIO7` and two direction outputs on `GPIO8` and `GPIO10`. Idle in the current
code is `PWM=0` with both direction outputs `LOW`.

This document captures hardware facts. It does not approve powered motor tests
by itself.

## Intake Status

- Date:
- Operator:
- Hardware location:
- Photo/video folder or ticket:
- Driver status: `unknown` until identity, pinout, power, idle, fault, current,
  direction, and STOP checks below are complete.
- Firmware status: unsupported unless the captured facts match the current
  `PWM + DIR1 + DIR2` path or a new explicit firmware path is added and tested.

## Ground Rules

- Keep motor power disconnected until driver identity, pinout, supply voltage,
  current limit, enable/idle behavior, fault behavior, and STOP behavior are
  known.
- Remove the prop or mechanically prevent thrust during dry powered tests.
- Use a current-limited supply for every powered step.
- Keep steering disabled while identifying the thruster driver.
- Treat any unlabelled wire, module, or connector as unknown.
- Stop on heat, smell, brownout, unexpected movement, direction ambiguity,
  driver fault, voltage sag, or output that survives release/STOP.
- Do not rely on app preflight "motor ready" until the real driver is captured
  here and the firmware/docs match it.

## Driver Identity

Record direct evidence, not guesses.

| Field | Value |
| --- | --- |
| Driver board/module marking | |
| Driver IC or controller marking | |
| Seller/manufacturer/link/datasheet | |
| Driver family | `H-bridge`, `half-bridge pair`, `ESC`, `serial controller`, `relay`, `unknown` |
| Input mode | `PWM+DIR`, `IN/IN`, `EN+DIR+PWM`, `RC/servo pulse`, `UART/serial`, `unknown` |
| Compatible with current firmware pins? | `yes/no/unknown`, with proof |
| Motor marking/model | |
| Motor voltage rating | |
| Motor no-load current | |
| Motor stall current | |
| Driver continuous/peak current rating | |
| Cooling method | |
| Any onboard DIP switches/jumpers/pots | |
| Enable/sleep/reset input behavior | |
| Brake/coast control behavior | |
| Fault output present | |
| Current-sense output present | |
| Thermal output or derate behavior | |

Required evidence:

- Clear photos of both sides of the driver board.
- Clear photos of motor label, connector, cable colors, fuse/breaker, and power
  wiring.
- Datasheet or seller wiring diagram if available.
- Continuity or pin-trace notes for every signal and power connector.

## Pinout Capture

Do not connect the ESP32 until this table is filled and reviewed.

| Driver pin | Observed label | Direction | Electrical level | Connects to | Proof |
| --- | --- | --- | --- | --- | --- |
| | | `input/output/power/motor/fault/sense` | | | |

Minimum pinout facts:

- Logic supply voltage and tolerance.
- Motor supply voltage and polarity.
- Ground reference and whether logic/motor grounds must be common.
- Enable, sleep, reset, brake/coast, fault, current-sense, or mode pins and
  their safe default states.
- PWM, direction, IN/IN, RC pulse, UART, or command pins and their voltage
  thresholds.
- Motor output pins and wire polarity.
- Fuse/breaker and kill-switch location.

Blocker: if any pin can energize the motor and its active level is unknown, do
not connect it to the firmware board.

## Voltage, Current, And Thermal Limits

| Item | Value | Evidence |
| --- | --- | --- |
| Logic voltage | | |
| Motor supply range | | |
| Starting bench supply voltage | | |
| Starting current limit | | |
| Rated run current | | |
| Rated peak/stall current | | |
| Driver current-limit setting | | |
| Fuse/breaker rating | | |
| Wire/connector current rating | | |
| Max acceptable driver temperature | | |
| Max acceptable motor temperature | | |
| Low-voltage cutoff behavior | | |
| Over-current behavior | | |
| Over-temperature behavior | | |

Current-limit proof:

- Record whether current limiting is handled by the bench supply, driver
  hardware, firmware setting, DIP switch, trimpot, or fixed controller behavior.
- Measure driver idle current with output disabled and enabled.
- Measure motor no-load current during the shortest safe pulse.
- Record whether current sense is valid during drive, brake, coast, and current
  limiting.
- Record whether STOP, manual lease expiry, BLE disconnect, OTA begin, and ESP
  reset remove drive command and leave the driver in a safe state.

Blocker: no low-power spin if stall current can exceed the driver, fuse, wire,
connector, or supply limit before protection acts.

## Brake, Coast, Enable, And Idle Behavior

Different brushed drivers treat `PWM=0`, both inputs low, sleep, and enable
pins differently. Capture the actual result.

| Case | Expected safe result | Observed |
| --- | --- | --- |
| Driver powered, ESP off | no movement; safe idle, disabled, or known brake state | |
| ESP boot | no movement before BLE connect | |
| BLE connected `IDLE` | no movement; safe current level | |
| `PWM=0`, direction pins low | `coast`, `brake`, or `disabled`, explicitly recorded | |
| Manual release/TTL expiry | no movement; safe current level | |
| `MANUAL_OFF` | no movement; safe current level | |
| BLE `STOP` | no movement; safe current level | |
| Hardware STOP | no movement; safe current level | |
| BLE disconnect | no movement; safe current level | |
| OTA begin/abort | no movement; safe current level | |
| ESP reset/brownout | no unexpected pulse during or after boot | |

Record whether the product needs active braking, coast, or disabled output for a
safe stop. If the driver cannot provide the required idle behavior with the
current pins, firmware must not drive it through the current `MotorControl`
path.

## Direction And Polarity Proof

Perform direction proof only after identity, pinout, and current limit are
complete. Keep prop/load removed.

| Step | Command/input | Expected | Observed | Pass |
| --- | --- | --- | --- | --- |
| Logic-only forward | | expected signal polarity only | | |
| Logic-only reverse | | expected signal polarity only | | |
| Low-power forward pulse | | motor output direction marked as forward | | |
| Low-power reverse pulse | | motor output direction marked as reverse | | |
| Release/deadman expiry | | output returns to safe idle | | |
| BLE STOP | | output returns to safe idle | | |
| Hardware STOP | | output returns to safe idle | | |
| ESP reset | | no unexpected movement during or after boot | | |

Record:

- Which motor rotation or thrust direction corresponds to positive manual
  throttle.
- Whether firmware polarity must be inverted.
- Whether anchor auto forward direction matches the physical forward thrust.
- Whether the driver twitches on enable, sleep exit, reset, or mode change.

Blocker: if forward/reverse is ambiguous, do not proceed to water, anchor auto,
or higher-power bench runs.

## Fault And Telemetry Inputs

| Field | Value |
| --- | --- |
| Fault output present | `yes/no/unknown` |
| Fault active polarity | |
| Fault is latched or auto-retry | |
| Current-sense output present | `yes/no/unknown` |
| Current-sense scale/offset | |
| Voltage telemetry present | `yes/no/unknown` |
| Temperature telemetry present | `yes/no/unknown` |
| Needs pull-up or filtering | |
| Firmware pin available/assigned | |

Required proof:

- Measure fault idle and active levels without connecting to the ESP32 first.
- If current sense exists, measure zero-current offset and short-pulse current.
- Record whether current sense is valid during braking/current limiting.
- Record whether faults disable outputs, latch until cleared, or auto-retry.

Blocker: if a fault can auto-retry into repeated motor pulses, powered bench
must use a physical current limit and operator-visible fault capture until
firmware handles the fault mode explicitly.

## Firmware And Docs Follow-Up

Once the real hardware facts are known, update the release path in one coherent
change. Required decision points:

- `boatlock/main.cpp`: confirm or replace `PWM=7`, `DIR=8`, `DIR=10`, and any
  new enable/fault/current pins.
- `boatlock/MotorControl.h`: add an explicit driver profile if the actual input
  mode is not the current `PWM + DIR1 + DIR2` H-bridge behavior.
- Runtime safety: define safe idle, brake/coast, polarity, current/fault
  handling, ramp limits, and STOP/reset behavior.
- Settings/schema: add service settings only if the hardware needs configurable
  inversion, limits, or fault pins; update `docs/CONFIG_SCHEMA.md` with any
  schema change.
- BLE protocol/telemetry: update `docs/BLE_PROTOCOL.md` only if commands,
  status fields, or driver-health telemetry change.
- Flutter readiness: do not show motor driver health as OK until firmware can
  report configured/known driver state.
- Tests: add native coverage for idle output, forward/reverse polarity, STOP,
  manual TTL expiry, anchor-denial quiet output, OTA begin quiet output, and
  fault/current behavior where applicable.
- `docs/POWERED_BENCH_CHECKLIST.md`: replace unknown-driver gates with the real
  supported-driver procedure.
- `docs/PRODUCT_READINESS_PLAN.md` and `boatlock/TODO.md`: keep the brushed
  motor driver item open until firmware, docs, and validation match the real
  hardware.
- README/wiring docs/photos: update any stale pinout or driver references from
  real hardware evidence.

Do not preserve compatibility with obsolete motor-driver assumptions unless
there is an explicit product reason to support multiple physical builds.

## Intake Completion Criteria

This intake is complete only when all of these are true:

- Driver IC/module and motor are identified from markings, photos, or
  datasheets.
- Pinout, voltage/current limits, brake/coast/enable behavior, and fault/current
  signals are measured or sourced.
- Direction, polarity, safe idle, STOP, release, disconnect, OTA begin, reset,
  and brownout behavior are proven at low power.
- Every blocker above is either resolved in hardware or listed as a required
  firmware/docs change before powered bench.
- The real hardware facts are linked from the powered bench log or ticket.

Completion of this intake unblocks firmware planning and the hardware test
plan; it does not mean the current firmware is safe to drive the hardware.
