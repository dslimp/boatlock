# Agent Task Backlog

Status after the 2026-04-26 autonomous pass.

## Done In Main

- Paired-mode raw safety/control commands now require `SEC_CMD`; raw `STOP`,
  `ANCHOR_OFF`, and `HEARTBEAT` no longer bypass paired auth.
- Unknown BLE commands now fail the firmware command-profile gate in every
  profile.
- Product readiness docs were refreshed around current app, simulator, and
  hardware blockers.
- Flutter service OTA can consume a validated latest-main firmware manifest and
  expose `Обновить до main`.
- CI now generates and deploys an OTA-capable `esp32s3_service` latest-main
  firmware channel under GitHub Pages.
- The app can also resolve the latest GitHub release as a firmware source,
  preferring a release manifest asset and falling back to release assets with
  matching SHA-256 metadata.
- Flutter service settings now roll back optimistic values when firmware
  accepts the BLE write but then rejects the command by profile in diagnostics.
- OTA upload failures in service UI now show the rejected command, active
  profile, and required profile instead of a generic rejection.
- Smoke and production-app e2e result JSON now include structured
  `commandReject*` fields for firmware profile rejections.
- Offline Russian-water simulation now reports wave steepness, roll-rate, and
  GNSS/heading sensor-frame degradation metrics/events with thresholds for
  Rybinsk fetch and Ladoga storm scenarios.

## Active Background Work

- None. `Epicurus` left the GitHub latest-release source in the shared tree; it
  was reviewed, completed, and moved into main work.

## Next Autonomous Tasks

- Prove the new latest-main Pages channel through an authenticated CI/Pages
  check on the first real `main` run:
  confirm `https://dslimp.github.io/boatlock/firmware/main/manifest.json`,
  `firmware.bin`, `SHA256SUMS`, and `BUILD_INFO.txt` are public and match.
  The unauthenticated probe on 2026-04-26 returned `404` for the private repo,
  so the remaining gate is the Actions/Pages deployment result and visibility
  setting, not local app code.
- Build service app variants with:
  `--dart-define=BOATLOCK_SERVICE_UI=true` and
  `--dart-define=BOATLOCK_FIRMWARE_UPDATE_MANIFEST_URL=https://dslimp.github.io/boatlock/firmware/main/manifest.json`.
- Add production app e2e fast-fail for firmware profile rejections:
  `SIM_RUN`, compass service commands, `OTA_BEGIN`, and `OTA_FINISH`.
- Keep multi-client controller support deferred until per-client auth/session and
  a single control-owner lease are implemented.

## Hardware-Gated Tasks

- Complete brushed motor driver intake before powered thruster tests:
  driver identity, pinout, safe idle, polarity, voltage/current limits,
  fault/current outputs, STOP/reset behavior.
- Complete steering driver intake before powered steering tests:
  actual driver type, steps-per-degree or gear ratio, travel limits, idle/hold
  current behavior, hard-stop/jam handling, and direction inversion.
- Do not change firmware motor/steering profiles for the real boat until those
  facts are captured. The current firmware assumes PWM plus two direction pins
  for the brushed motor and 28BYJ-48 plus ULN2003-style `HALF4WIRE` steering.
- Run `nh02` release/service/acceptance profile proof and Android app e2e after
  the CI Pages channel is live.

## Defer Or Move Out Of Main

- Route following, waypoint workflows, cruise/fishing convenience modes, and
  arrival-to-anchor flows.
- A second BLE controller or joystick until the control-owner lease model exists.
- Broad tuning/calibration UI in normal water builds; keep it service-gated.
- Hardware-calibrated motor/steering simulation until powered bench logs exist.

## Agent Notes Captured

- `Darwin`: paired auth bypass checklist; implemented and pushed.
- `Carver` / `Russell`: app e2e and service rejection gaps; still open.
- `Helmholtz`: simulator wake/chop/sensor-frame implementation plan; still open.
- `Hubble`: motor and steering driver intake gates; hardware-gated.
- `Ohm`: keep-in-main vs defer plan; docs refreshed, unknown-command gate done,
  remaining multi-client and simulator work still open.
- `Jason`: latest-main OTA should use a durable manifest/static channel, not raw
  expiring Actions artifacts; implemented through app manifest client and Pages
  publishing.
- `Epicurus`: latest GitHub release source; implemented through pure release
  parsing plus `FirmwareUpdateClient` fallback behind
  `BOATLOCK_FIRMWARE_UPDATE_GITHUB_REPO`.
