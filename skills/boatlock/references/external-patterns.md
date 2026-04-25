# External Patterns Reference

## Purpose

This file captures stable external best practices and recurring patterns from marine anchor-watch, GPS-anchor, and small-boat autopilot systems so BoatLock can reuse proven ideas instead of reinventing them.

Use this file for:

- comparative architecture work
- control-loop safety design
- anchor-watch UX and telemetry decisions
- deciding what belongs in `main` vs what is optional

## Source Set

- OpenCPN Anchor Watch: <https://opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:manual_advanced:features:auto_anchor>
- OpenCPN Auto Anchor Mark: <https://opencpn.org/wiki/dokuwiki/doku.php?id=opencpn:manual_advanced:features:anchor_auto>
- ArduPilot Rover Loiter Mode: <https://ardupilot.org/rover/docs/loiter-mode.html>
- ArduPilot Rover Hold Mode: <https://ardupilot.org/rover/docs/hold-mode.html>
- ArduPilot Pre-Arm Safety Checks: <https://ardupilot.org/rover/docs/common-prearm-safety-checks.html>
- ArduPilot Rover Failsafes: <https://ardupilot.org/rover/docs/rover-failsafes.html>
- ArduPilot Rover GPS glitch diagnostics: <https://ardupilot.org/rover/docs/common-diagnosing-problems-using-logs.html>
- ArduPilot Rover Motor and Servo Configuration: <https://ardupilot.org/rover/docs/rover-motor-and-servo-configuration.html>
- ArduPilot Rover Manual Mode: <https://ardupilot.org/rover/docs/manual-mode.html>
- ArduPilot Rover Tuning Speed and Throttle: <https://ardupilot.org/rover/docs/rover-tuning-throttle-and-speed.html>
- ArduPilot Rover parameter reference: <https://ardupilot.org/rover/docs/parameters.html>
- ArduPilot SITL simulator: <https://ardupilot.org/dev/docs/sitl-simulator-software-in-the-loop.html>
- ArduPilot Using SITL: <https://ardupilot.org/dev/docs/using-sitl-for-ardupilot-testing.html>
- ArduPilot Simulation on Hardware: <https://ardupilot.org/dev/docs/sim-on-hardware.html>
- PX4 Simulation: <https://docs.px4.io/main/en/simulation/>
- Signal K Anchor Alarm guide: <https://demo.signalk.org/documentation/Guides/Anchor_Alarm.html>
- Signal K notifications spec: <https://signalk.org/specification/1.7.0/doc/notifications.html>
- Signal K local/remote alerts: <https://signalk.org/2025/signalk-local-remote-alerts/>
- pypilot user manual: <https://pypilot.org/doc/pypilot_user_manual/>
- Garmin Force support docs: <https://support.garmin.com/en-US/marine/faq/AN7tH1M9c10Zp86F0C8IU7/>
- Garmin Force calibration: <https://support.garmin.com/en-US/?faq=LcXdLIgqpo7Kw0Sc1shIp7>
- Garmin Force troubleshooting: <https://support.garmin.com/hr-HR/?faq=KKAL6Htspk0Bw5zsQxwZ1A>
- Minn Kota Spot-Lock: <https://minnkota.johnsonoutdoors.com/us/learn/technology/spot-lock>
- Lowrance Ghost / Recon pages: <https://www.lowrance.com/ghost-trolling-motor/> and <https://www.lowrance.com/recon-trolling-motor>
- MotorGuide Tour Pro / Pinpoint GPS: <https://www.motorguide.com/us/en/tour-pro.html>
- Bluetooth Core GATT specification: <https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-61/out/en/host/generic-attribute-profile--gatt-.html>
- Android BluetoothGatt API reference: <https://developer.android.com/reference/android/bluetooth/BluetoothGatt>
- Android BluetoothGattCharacteristic API reference: <https://developer.android.com/reference/android/bluetooth/BluetoothGattCharacteristic>
- Bluetooth HID Over GATT Profile: <https://www.bluetooth.com/specifications/specs/hid-over-gatt-profile/>
- Material Design Floating Action Button guidance: <https://m1.material.io/components/buttons-floating-action-button.html>
- CEVA BNO08X Datasheet: <https://www.ceva-ip.com/wp-content/uploads/2019/10/BNO080_085-Datasheet.pdf>
- Adafruit BNO085 UART-RVC guide: <https://learn.adafruit.com/adafruit-9-dof-orientation-imu-fusion-breakout-bno085/uart-rvc-for-arduino>
- Adafruit BNO085 pinout/mode-select guide: <https://learn.adafruit.com/adafruit-9-dof-orientation-imu-fusion-breakout-bno085/pinouts>
- Arduino Blink Without Delay example: <https://docs.arduino.cc/built-in-examples/digital/BlinkWithoutDelay/>

## What OpenCPN Gets Right

- Treat anchor watch as a boundary/alarm feature, not as motorized control.
- Require enough context before showing the feature at all: active GPS, small chart scale, ownship near the mark.
- Use a simple editable radius with clear visualization.
- Support a second "danger side" watch point to cover shore or obstacles, not only the anchor circle.
- Alarm on GPS loss, not only on radius breach.
- Auto-drop an anchor mark only after a conservative heuristic says the boat really transitioned from cruising to anchoring.

Implication for BoatLock:
- Keep "anchor watch / drag alarm" separate from "anchor hold".
- Support more than one safety boundary concept:
  - anchor radius
  - shore/obstacle exclusion point or exclusion boundary
- Auto-created anchor points should use conservative heuristics and deduping.
- A persisted anchor point must not be reported as saved unless the storage commit succeeds; failed persistence should preserve the previous live point.

## What Signal K Gets Right

- The device/server owns alarm monitoring; apps configure and display it.
- Track history around anchor is considered part of the feature, not a debugging extra.
- Anchor workflow has phases:
  - disarmed
  - anchor dropped / rode settling
  - armed
- Anchor position can be shifted after arming.
- Notifications are multi-channel and multi-level:
  - warning
  - alarm
  - emergency
- Informational operator acknowledgements should not be collapsed into warning/alarm health state.
- Phone push should not be the only alarm path.

Implication for BoatLock:
- Keep safety logic authoritative on-device.
- App should configure and visualize, not be the source of truth for monitoring.
- Treat latitude/longitude telemetry as a pair. Do not publish one valid coordinate together with an invalid/default companion.
- Add phase/state modeling instead of a binary `AnchorEnabled` worldview.
- Preserve and expose anchor track history.
- Support at least two severity zones:
  - warning radius / drift alert
  - emergency containment breach
- Keep health status severity separate from informational status reason tokens.
- Phone/BLE-facing status reason fields should stay structured and bounded: do not let separator or control characters from a reason string create extra reason tokens or corrupt client parsing.

## What ArduPilot Gets Right

- Pre-arm / pre-enable checks are first-class and should not be casually bypassed.
- There is a dedicated quiet state (`Hold`) for stopping actuation.
- Failsafe recovery is explicit: after link recovery, operator action is still required before normal manual control resumes.
- Boat loiter behavior is radius-based:
  - inside radius, drift
  - outside radius, correct in a bounded way
- Correction logic minimizes unnecessary rotation by choosing the shorter turn solution.
- Speed/thrust is capped and tied to distance from the allowed zone.
- ESC deadband compensation is treated as a real tuning concern, not ignored.
- Throttle slew limiting is a first-class actuator protection concept.
- GPS glitches are diagnosed through quality evidence such as HDOP and satellite count changes, not just the presence of coordinates.
- Failsafes are explicit, typed, and mapped to deterministic actions.
- Motor tests should start at low throttle and with the platform made mechanically safe.

Implication for BoatLock:
- `ANCHOR_ON` should stay behind a hard pre-enable gate.
- `SAFE_HOLD` / `HOLD` should become a real runtime mode, not an inferred combination of flags and reasons.
- The anchor controller should remain zone-based, not continuously aggressive at the center.
- Keep explicit typed failsafe reasons and deterministic actions.
- Failsafe core code should keep local minimum timeout floors. Upstream configuration clamps are useful but must not be the only thing preventing a disabled safety timer.
- Containment/drift thresholds must fail deterministic even if settings are invalid; sanitize non-finite thresholds before comparisons.
- Motion telemetry derived from position deltas must expire or reset after long sample gaps.
- Account for actuator deadband in shipped tuning defaults.
- Treat `stop()` as a full actuator state reset, not only a PWM write.
- Keep hard actuator stop distinct from ordinary controller idle output; failsafe/STOP should clear hidden state, while zone-control idle should preserve anti-hunt timing.
- Keep rate limiting, max thrust, anti-windup, and anti-hunt windows in the core motor path.
- Keep actuator tuning explicit and evidence-based. Do not hide self-adaptive gain changes or gain persistence inside the runtime motor path.
- Keep manual and auto actuator state isolated. Manual throttle output must not seed automatic throttle ramp state after a mode transition.
- Stop/zero output paths should put both PWM and direction pins into a known idle state.
- Actuator tuning inputs must be finite before clamp/ramp math. If a tuning value is `NaN` or otherwise invalid, fail closed rather than deriving a motor output from undefined math.
- Runtime motion arbitration must reject non-finite auto-control settings before heading alignment or thrust policy uses them.
- Manual control must be an explicit source-owned deadman lease; competing sources must not overwrite a live lease.
- Supervisor configs and command limits must fail closed on non-finite or out-of-range values before they affect failsafe decisions.
- Stepper/servo outputs should have explicit neutral/idle behavior. Release or reduce holding current when idle if holding torque is not required, and make re-enable behavior deliberate.
- Stepper idle-release timers should track active state separately from timestamp values; `0` is a valid `millis()` sample and must not prevent coil release.
- GNSS quality checks should require fresh quality evidence and fail closed when evidence disappears.
- Phone or app-provided positions are not equivalent to onboard GNSS for control. Keep fallback/display source state isolated from hardware-source filters, speed baselines, and jump baselines.
- Runtime GNSS source handlers should validate coordinate values before updating live fix state; raw parser/app "valid" flags are not enough at the control boundary.
- GNSS source transitions should reset stale hardware baselines rather than comparing a new hardware fix against old phone/no-fix state.
- GNSS quality gates should reject invalid/non-finite thresholds and samples at the gate boundary; corrupted config or `NaN` sensor evidence must not loosen auto-mode entry checks.
- Angle/heading normalization in safety paths must use bounded modulo-style math, not unbounded loops over input magnitude.
- Serial/GNSS watchdogs should be non-blocking and based on elapsed-time checks; direct `now > then` comparisons are not rollover-safe.
- Serial/GNSS watchdog restarts should start a new no-data observation window, and restart cooldowns should use explicit "restart seen" state instead of treating timestamp `0` as "never".
- Serial/GNSS watchdog timing config needs local minimum floors so zero or corrupted intervals cannot cause busy warning/restart loops.
- Compass/RVC watchdogs follow the same rule: track event presence separately from timestamps, then calculate age with unsigned subtraction. A timestamp value of `0` is a valid sample time.
- Compass/RVC parsers should validate the full packet contract at the boundary: sync/header, checksum, and datasheet value ranges before changing live heading state.
- Compass health checks should be fail-closed with local timeout floors; a zero or tiny timeout input must not silently disable heading-event loss detection.
- Diagnostics follow the same rule as watchdogs: never use timestamp `0` as "not started"; use explicit seen flags and unsigned elapsed-time math.
- Saturation diagnostics should validate the configured actuator limit before comparing output against it.
- HIL/control-loop timers follow the same rule as watchdogs and diagnostics: use explicit seen flags plus unsigned elapsed-time math, and treat timestamp `0` as a valid sample.
- HIL core behavior needs direct boundary tests in addition to end-to-end scenarios; scenario coverage alone can miss timer sentinel bugs.
- HIL command surfaces should be narrow and deterministic: accept only documented commands and reject malformed payloads instead of guessing alternate encodings.
- HIL logs are part of the test interface. Keep log records single-line, bounded, and neutralize CR/LF/control characters before logging command-derived fields.
- Periodic UI/BLE/sensor cadence should use one shared non-blocking elapsed-time helper. Floor interval `0` instead of allowing same-timestamp bursts, and test unsigned rollover directly instead of relying only on normal interval scenarios.
- One-shot UI/diagnostic banners follow the same timer rule: store the start timestamp and duration, then compare unsigned elapsed time. Do not store absolute expiry timestamps as the authority.
- Do not auto-enter manual control from failsafe. After a fault clears, operator control must be an explicit new action.
- Treat non-finite sensor/control values like unavailable inputs at module boundaries, not as values to normalize later inside actuator code.
- In auto/position-control modes, invalid distance/position evidence should disable the control input rather than masquerade as a zero-error measurement.
- Treat physical button input as an unsafe raw signal. Debounce it before edges, and reserve state-changing actions such as anchor save or pairing for stable long-press paths.
- For BLE/manual control, keep GATT as a small transport and put safety in the app protocol:
  - use acknowledged writes for control commands where the client needs a write result
  - keep live telemetry on notify/read characteristics
  - make manual actuation updates atomic instead of split mode/direction/speed writes
  - use a short deadman/lease on manual input so app crash, disconnect, or controller loss goes quiet
  - reject invalid manual controller sources and do not refresh a deadman lease from malformed packets
  - map future BLE HID/HOGP joystick input into the same internal manual-control state instead of creating a second actuator path
  - keep/restart connectable advertising while a client is connected when future phone + remote discovery is required
  - never treat multi-central BLE transport as enough for simultaneous control; control ownership still needs an explicit lease/arbitration rule
  - suppress or downgrade high-frequency heartbeat/debug traffic in normal serial logs so diagnostics keep event lines intact and readable
  - log BLE connect/disconnect events as short key=value lines and keep raw reason codes in decimal plus hex for layer-specific NimBLE/HCI triage
  - format runtime logs into a bounded buffer, then write the known formatted byte count instead of relying on unbounded C-string scans
  - build queued BLE log payloads with bounded source scans and explicit zero-fill; do not call C string functions on length-unknown buffers
  - clamp binary telemetry values before integer rounding/casting so invalid runtime values cannot corrupt the wire representation
  - sanitize direct numeric telemetry fields before packing fixed binary characteristics; out-of-range or non-finite floats must not be cast directly into integers
  - keep fixed binary telemetry frames length-stable and map enum/flag fields from one explicit table rather than duplicated branching
  - treat text/log characteristic values as length-delimited byte strings, not implicit C strings; trim defensive padding at the client boundary
- For manual UI, avoid making actuation look like a primary one-tap FAB action. Use an explicit control surface such as a toolbar entry plus sheet/pad, and keep movement tied to press-and-hold/deadman semantics.

## What Commercial GPS Anchors Get Right

- Small-step "jog" reposition is standard.
- Distance/bearing to the locked point is user-facing telemetry.
- Heading hold and anchor lock are separate but adjacent operator concepts.
- Installation and calibration quality are treated as mandatory prerequisites.
- GPS-anchor products document real GPS limitations instead of pretending sub-meter truth.
- Multiple control surfaces exist:
  - pedal
  - handheld remote
  - display/chartplotter
- Users expect anchor lock from current position, waypoint, or map point.

Implication for BoatLock:
- Keep nudging/reposition as a first-class feature.
- Keep jog/nudge distance fixed and small unless there is a tested product reason to expose distance control.
- Anchor jog/nudge projection should be fail-atomic: compute and validate the whole next point before publishing or persisting it.
- Expose distance/bearing-to-anchor in the main UX.
- Keep heading hold separate from position hold in both UI and state model.
- Surface calibration / sensor readiness as enable-blocking status, not hidden service detail.
- Avoid claiming precision beyond consumer GNSS reality.

## What pypilot And Similar Open Autopilots Get Right

- Always keep a basic mode that remains valid when advanced inputs disappear.
- Higher modes may fall back to simpler modes, but the fallback chain must be explicit.
- Core mode layering matters more than feature count.

Implication for BoatLock:
- A basic quiet-safe mode must always be available.
- Any future advanced behavior should degrade toward simpler, safer states instead of staying half-active.

## What Autopilot Simulation Gets Right

- ArduPilot SITL runs the real autopilot code as a native executable and feeds it simulated sensor data from a flight dynamics model.
- ArduPilot SITL is used to change environments, simulate failure modes, and configure optional components before real-world tests.
- ArduPilot Simulation-on-Hardware runs both control software and simulation on the autopilot hardware, which is useful for checking mission structure, control-surface/output movement, physical failsafes, and real communications traffic without vehicle risk.
- Simulation-on-Hardware explicitly warns to remove or disable dangerous actuators before allowing real outputs to move.
- PX4 treats simulation as the safe pre-real-world test layer and separates SITL from HITL.
- PX4's internal SIH path is headless, zero-dependency, and fast; this matches BoatLock's need for cheap regression loops.
- PX4 lockstep/speed-factor guidance means simulation speed is part of the contract; faster-than-realtime runs must remain deterministic instead of racing timers unpredictably.

Implication for BoatLock:
- Keep on-device HIL and offline simulation in `main`; they are safety validation infrastructure, not optional product bloat.
- Exercise the real control code paths in simulation rather than building a parallel fake controller.
- Keep simulated sensors and fault injection explicit and typed so failures map to the same failsafe reasons as hardware.
- Keep `SIM_*` commands narrow, deterministic, and documented; reject malformed payloads rather than guessing operator intent.
- Failed or malformed simulation commands must not mutate live runtime state. State-clearing side effects belong only to a successfully started run.
- Chunked reports and logs are a test interface contract and need direct unit tests plus a BLE smoke once the end-to-end SIM path is phone-visible.
- On real hardware, simulation acceptance must keep dangerous actuator movement disabled or explicitly zero-throttle unless a powered bench procedure is defined.

## What ESP32 Storage Guidance Gets Right

- Arduino-ESP32 documents `Preferences`/NVS as the ESP32 replacement for the Arduino EEPROM library for retained small values.
- ESP-IDF NVS is append-oriented and includes flash wear levelling, but changes are only guaranteed persistent after an explicit commit.
- Espressif's NVS FAQ treats flash writes as bounded-lifetime operations and notes that flash writes have runtime constraints.

Implication for BoatLock:
- Avoid write amplification in settings paths even when the backend has wear levelling.
- A settings save with no semantic change should be a no-op.
- A settings object should load safe RAM defaults before any storage read and must not commit flash from its constructor.
- Normalize values by declared type before dirtying/persisting so integer and bool-like settings cannot remain fractional in RAM or storage.
- Treat flash commits as fallible. A failed commit must not be logged as saved or clear dirty state.
- Runtime objects backed by persisted settings should restore previous RAM values when a commit fails instead of exposing uncommitted state as the active operating point.
- Boot migration, CRC recovery, and value normalization are the only expected boot-time settings writes.
- Non-finite config values must fail closed before reaching persisted storage.

## Best-Practice Decisions For BoatLock

These are the durable takeaways that should influence future changes:

1. Do not merge anchor watch, anchor alarm, and motorized anchor hold into one blurred feature.
2. Treat pre-enable checks as product behavior, not as developer convenience.
3. Use zone-based control:
   - drift inside allowed radius
   - bounded correction outside radius
4. Make containment breach a hard exit condition.
5. Add a first-class `SAFE_HOLD` / `HOLD` state.
6. Keep monitoring and failsafe evaluation on-device.
7. Offer reposition/jog controls in small explicit steps.
8. Record and expose track history around anchor events.
9. Use multi-level notifications and more than one alarm path.
10. Surface real-world GNSS and calibration limitations in UX and docs.
11. Persist settings only on actual state changes; no-op saves should not commit flash.

## Explicit Non-Goals

- Do not import ArduPilot's full mode matrix or parameter sprawl into `main`.
- Do not move anchor safety decisions into the phone app.
- Do not widen `main` with route navigation, broad autopilot scope, or fishing-market convenience features unless explicitly requested.
