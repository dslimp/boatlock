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
- Android and macOS CI now publish service app artifacts configured with the
  latest-main firmware manifest, and local wrappers can build the same
  one-button update variants.
- GitHub Releases now receive a service-profile `firmware-esp32s3-service`
  asset set plus `manifest.json`, and the app can use authenticated release
  asset API URLs when a token-backed validation client is injected.
- Android app e2e now has a manifest-backed latest-main OTA mode that serves
  `manifest.json` plus `firmware.bin` and uses the app firmware-update client
  before the normal BLE OTA/reconnect verdict.
- Manifest-backed Android OTA e2e has been proven on `nh02` against the
  service firmware profile, including phone HTTP manifest download, BLE upload,
  ESP32 reboot, and app telemetry recovery.
- The Flutter BLE client now binds all BoatLock GATT characteristics before
  enabling data/log notifications, treats the OTA characteristic as required
  for upload, and can clear the Android GATT cache plus rediscover when OTA is
  missing.
- OTA control commands now use explicit service-scope app writes, and CI runs
  the service UI widget test with `BOATLOCK_SERVICE_UI=true`.
- Firmware OTA ownership is tied to the BLE connection handle that starts the
  update, so a non-owner central disconnect no longer aborts an active phone
  OTA transfer.

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
- Keep multi-client controller support deferred until per-client auth/session and
  a single control-owner lease are implemented.
- Add a macOS runtime/update smoke or manual acceptance path for the service app.
- Decide whether Android/macOS service builds should intentionally replace the
  normal app or get distinct app/bundle IDs before operator distribution.
- Add a CI/runtime probe for the public latest-main Pages channel after the
  first real deployment, using the same manifest validation and SHA check as
  the app.

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
- After the CI Pages channel is live, run a service-app OTA proof against the
  public Pages URL instead of the local wrapper-served manifest.

## Defer Or Move Out Of Main

- Route following, waypoint workflows, cruise/fishing convenience modes, and
  arrival-to-anchor flows.
- A second BLE controller or joystick until the control-owner lease model exists.
- Broad tuning/calibration UI in normal water builds; keep it service-gated.
- Hardware-calibrated motor/steering simulation until powered bench logs exist.

## Agent Notes Captured

- `Darwin`: paired auth bypass checklist; implemented and pushed.
- `Carver` / `Russell`: app e2e and service rejection gaps; implemented
  through fast-fail e2e handling, settings rollback, detailed OTA rejections,
  and structured smoke/e2e result fields.
- `Helmholtz`: simulator wake/chop/sensor-frame implementation plan; wave
  steepness and sensor-frame degradation are implemented, wake bursts remain
  deferred until water-test inputs or powered logs exist.
- `Hubble`: motor and steering driver intake gates; hardware-gated.
- `Ohm`: keep-in-main vs defer plan; docs refreshed, unknown-command gate done,
  remaining multi-client and simulator work still open.
- `Jason`: latest-main OTA should use a durable manifest/static channel, not raw
  expiring Actions artifacts; implemented through app manifest client and Pages
  publishing.
- `Epicurus`: latest GitHub release source; implemented through release parsing,
  service-profile release artifacts, release manifest publishing, and
  token-backed asset API support behind `BOATLOCK_FIRMWARE_UPDATE_GITHUB_REPO`.
- `Bacon`: OTA commands must be written through the service-scoped app command
  path; implemented and covered.
- `Leibniz`: one-button latest-main mostly existed, but service UI CI, stale
  OTA throughput docs, and real Pages/macOS proof gaps were identified. CI/docs
  fixes are implemented; Pages/macOS proof remains open.
- `Hume`: Android OTA failure triage pointed at missing OTA characteristic
  detail, service-profile artifact consistency, and stale remote-helper risks.
  The OTA diagnostics/artifact guidance was fixed and `install.sh` was rerun
  before hardware proof.
- `Faraday`: prioritized the remaining autonomous/gated tasks; `nh02`
  manifest-backed OTA and release return-to-bench proof are now complete, while
  Pages, macOS runtime, powered hardware intake, and control-owner lease remain
  open.
