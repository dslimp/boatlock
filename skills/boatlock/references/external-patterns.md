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
- Phone push should not be the only alarm path.

Implication for BoatLock:
- Keep safety logic authoritative on-device.
- App should configure and visualize, not be the source of truth for monitoring.
- Add phase/state modeling instead of a binary `AnchorEnabled` worldview.
- Preserve and expose anchor track history.
- Support at least two severity zones:
  - warning radius / drift alert
  - emergency containment breach

## What ArduPilot Gets Right

- Pre-arm / pre-enable checks are first-class and should not be casually bypassed.
- There is a dedicated quiet state (`Hold`) for stopping actuation.
- Boat loiter behavior is radius-based:
  - inside radius, drift
  - outside radius, correct in a bounded way
- Correction logic minimizes unnecessary rotation by choosing the shorter turn solution.
- Speed/thrust is capped and tied to distance from the allowed zone.
- ESC deadband compensation is treated as a real tuning concern, not ignored.
- Failsafes are explicit, typed, and mapped to deterministic actions.

Implication for BoatLock:
- `ANCHOR_ON` should stay behind a hard pre-enable gate.
- `SAFE_HOLD` / `HOLD` should become a real runtime mode, not an inferred combination of flags and reasons.
- The anchor controller should remain zone-based, not continuously aggressive at the center.
- Keep explicit typed failsafe reasons and deterministic actions.
- Account for actuator deadband in shipped tuning defaults.

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

## Explicit Non-Goals

- Do not import ArduPilot's full mode matrix or parameter sprawl into `main`.
- Do not move anchor safety decisions into the phone app.
- Do not widen `main` with route navigation, broad autopilot scope, or fishing-market convenience features unless explicitly requested.
