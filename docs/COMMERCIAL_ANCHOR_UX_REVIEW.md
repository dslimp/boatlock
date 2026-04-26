# Commercial Anchor UX Review

Date: 2026-04-26

This review maps official commercial GPS-anchor behavior to BoatLock product
decisions before powered bench and protected-water tests. It is product planning,
not a claim that BoatLock has equivalent field performance.

## Official Source Set

- Minn Kota Spot-Lock: https://minnkota.johnsonoutdoors.com/us/learn/technology/spot-lock
- Minn Kota Advanced GPS Navigation help: https://minnkota-help.johnsonoutdoors.com/hc/en-us/articles/23607178243991-Using-Advanced-GPS-Navigation-Features-and-Manual-2023-present
- Garmin Force owner manual, Holding Your Position: https://www8.garmin.com/manuals/webhelp/forcetrollingmotor/EN-US/GUID-F0CDCEE2-E27A-492C-A51A-79709BD86156.html
- Garmin Force getting started support: https://support.garmin.com/en-US/marine/faq/AN7tH1M9c10Zp86F0C8IU7/
- Lowrance Ghost: https://www.lowrance.com/lowrance/type/trolling-motor/ghost-47/
- Lowrance Recon: https://www.lowrance.com/recon-trolling-motor/
- MotorGuide Tour Pro: https://www.motorguide.com/us/en/tour-pro.html

## Repeated Product Patterns

Commercial GPS-anchor systems make anchor lock a primary operator action, not a
developer diagnostic. The core action usually exists on more than one control
surface: remote, foot pedal, app or chartplotter.

Short-distance jog is expected. Garmin documents 1.5 m / 5 ft anchor-position
adjustments, Minn Kota exposes Spot-Lock Jog behind heading-sensor support, and
MotorGuide lists Anchor, Jog, Heading Lock, and Cruise Control together.

Heading hold is adjacent to anchor lock but not the same mode. Lowrance and
Garmin expose separate anchor and heading controls. BoatLock should keep this
separation and avoid shipping heading-hold as a hidden variant of anchor hold.

Calibration and installation are product prerequisites. Garmin requires trolling
motor calibration before anchor lock, and the setup surfaces include compass
calibration plus anchor/navigation gain controls. BoatLock should make readiness
and calibration blockers visible rather than treating them as service trivia.

Map/chart integration is common. Lowrance describes anchoring at the current
location, a waypoint, or a map point. BoatLock now has current-position save and
map-point save; future route/waypoint navigation should still stay out of `main`
until basic anchor hold is proven.

Operator review matters. A practical water-test workflow needs anchor state,
jog requests, blocked reasons, telemetry transitions, and failsafe exits visible
after the run. The app-side history is the first local layer; persistent voyage
logs can wait until the powered path proves useful data shape.

## BoatLock Alignment

Already aligned for first protected-water preparation:

- Explicit save-anchor and enable-anchor actions.
- Anchor enable preflight with blocked reasons.
- Normal `ANCHOR_OFF` separated from emergency STOP.
- Fixed 1.5 m anchor jog controls.
- Distance and bearing to anchor in the main status surface.
- Session-local app history for anchor requests, telemetry transitions, blocked
  preflight, and allowlisted firmware event logs.
- Manual control is a dedicated sheet, not an accidental one-tap map action.

Still weak before powered or water confidence:

- Quiet first-water defaults must be the actual default, not only a service
  profile operators have to remember to apply.
- Steering driver and mechanics are not yet identified against the real boat
  hardware.
- Calibration/setup UX is not yet a first-class operator path. Bow zero,
  compass readiness, and steering configuration still feel like service details.
- Control ownership for a future remote/controller is not designed yet.
- Firmware-side service/dev/HIL command gating is implemented, but release vs
  service/acceptance rejection still needs powered bench and Android proof.
- Simulation still needs provenance/confidence metadata, loaded boat mass,
  yaw/windage refinement, wake/chop events, and hardware-calibrated motor data.

## Product Decisions

Keep in `main`:

- App connect/reconnect, live status, anchor save/enable/disable, emergency STOP,
  manual deadman control, fixed anchor jog, distance/bearing, app-side event
  history, simulation, tests, and acceptance tooling.

Keep service-only:

- Anchor profile selection, gain/tuning, compass offset/calibration, steering
  bow-zero, OTA, and HIL commands.

Do not add to `main` yet:

- Route following, arrival-to-anchor workflows, broad waypoint navigation, or
  convenience fishing features that are not needed to prove safe anchor hold.

Next autonomous work that does not require powered hardware:

1. Prove the firmware command-profile gate on `nh02` with release rejection logs
   and service/acceptance positive paths.
2. Make first-water quiet values the default with a safe settings migration.
3. Turn command-profile rejection into visible app/operator feedback where it
   affects service or acceptance workflows.
4. Tighten simulator backlog wording around the remaining physics and provenance
   gaps.
5. Design multi-client control ownership before a second BLE controller exists.
