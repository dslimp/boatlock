# TODO — BoatLock Product Readiness

Active backlog for powered bench and protected-water readiness. The detailed
plan lives in `docs/PRODUCT_READINESS_PLAN.md`.

## P0 — Before Powered Bench

- [x] Clear stale app telemetry on BLE disconnect and disable map controls from stale `boatData`.
- [x] Split app anchor flow into explicit save-anchor and enable-anchor actions.
- [x] Add anchor enable preflight checklist: link, auth, saved anchor, hardware GNSS, fresh heading, safety latch, motor readiness, STOP.
- [x] Add a normal `ANCHOR_OFF` UI action separate from emergency STOP.
- [x] Surface command/auth failures from map anchor actions.
- [x] Fix manual sheet wording: red `STOP` must either send real emergency `STOP` or be renamed to manual off.
- [ ] Verify/fix that phone GPS fallback cannot update GPS-to-compass correction used by control heading.
- [ ] Classify BLE commands into release/service/dev surfaces and gate service/dev paths from normal water UI.
- [ ] Identify the exact steering stepper driver/mechanics and update firmware/docs if it is not the current 28BYJ-48 + ULN2003 path.
- [ ] Build a no-load motor output acceptance checklist for `PWM=7`, `DIR=5/10`, boot, STOP, HOLD, reconnect, anchor denial, SIM, and OTA begin.
- [ ] Add powered-bench wiring requirements: current-limited supply, fuse/breaker, physical kill path, polarity proof, strain relief, and thermal check.
- [ ] Update README wiring diagrams/photos from real hardware.

## P1 — Before First Protected-Water Test

- [ ] Add a first-class readiness panel in the app with link age, GNSS/heading freshness, auth, anchor gate, failsafe, manual lease, and motor readiness.
- [ ] Add fixed 1.5 m anchor jog controls to the normal app UI.
- [ ] Show distance and bearing to anchor as primary telemetry.
- [ ] Add battery/power/current visibility or a required external measurement step in water-test logs.
- [ ] Add track/history around anchor events for post-run review.
- [ ] Add `MapPage` tests for disconnect, preflight, auth reject, STOP, and manual-off behavior.
- [x] Remove or branch off leftover non-target surfaces: `BOATLOCK_BOARD_JC4832W535`, route UI labels, and unused `ReacqStrat`.
- [ ] Run full local, `nh02`, Android, and no-load output validation before mounting powered hardware.

## P2 — Simulation And Product Polish

- [ ] Normalize `tools/sim/research/environment_inputs.*` into simulator scenario data.
- [ ] Add windage, waves/rocking, wakes, and current direction changes to simulation.
- [ ] Add brushed motor, battery sag/current limit, thermal derate, and driver deadband to simulation.
- [ ] Add steering backlash/jam/wrong-zero scenarios.
- [ ] Add RF/Russian water-body scenarios: Oka normal, Volga spring flow, Rybinsk fetch, Ladoga storm abort, Baltic/Gulf drift.
- [ ] Add production-path tests or simulation around `RuntimeMotion/MotorControl/StepperControl`.
- [ ] Add heading-hold as a separate operator flow after safe anchor hold is proven.
- [ ] Design explicit multi-client control ownership before accepting a future BLE remote as a second controller.
