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

## Active Background Work

- `Epicurus`: GitHub latest-release firmware source for the app. Scope is pure
  OTA/release parsing and tests under `boatlock_ui/lib/ota/` and
  `boatlock_ui/test/`; it must not touch the current latest-main Pages workflow.

## Next Autonomous Tasks

- Prove the new latest-main Pages channel on the first real `main` CI run:
  confirm `https://dslimp.github.io/boatlock/firmware/main/manifest.json`,
  `firmware.bin`, `SHA256SUMS`, and `BUILD_INFO.txt` are public and match.
- Build service app variants with:
  `--dart-define=BOATLOCK_SERVICE_UI=true` and
  `--dart-define=BOATLOCK_FIRMWARE_UPDATE_MANIFEST_URL=https://dslimp.github.io/boatlock/firmware/main/manifest.json`.
- Add production app e2e fast-fail for firmware profile rejections:
  `SIM_RUN`, compass service commands, `OTA_BEGIN`, and `OTA_FINISH`.
- Add service settings rollback for async profile rejections after successful BLE
  writes: `SET_COMPASS_OFFSET`, `RESET_COMPASS_OFFSET`, `SET_STEP_MAXSPD`,
  `SET_STEP_ACCEL`.
- Add structured OTA failure details in the app instead of generic
  `OTA отклонено`, including rejected command, active profile, and required
  profile.
- Add structured command-rejection fields to smoke/e2e result payloads so wrapper
  parsing does not depend on the last raw device log line.
- Implement the next offline simulator slice: wake/short-chop events plus
  BNO08x/GNSS sensor-frame degradation metrics and thresholds.
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
