# Validation Reference

## Pick The Smallest Relevant Check

- Firmware logic change:
  - run targeted native tests first
  - build the affected firmware env if the change touches production code
- BLE command or telemetry change:
  - native BLE/security tests
  - Flutter tests for parsing/UI
  - update `docs/BLE_PROTOCOL.md`
- GNSS, anchor-control, or safety change:
  - native tests
  - HIL simulation checks if behavior is control-loop sensitive
- Settings/schema change:
  - update `docs/CONFIG_SCHEMA.md`
  - keep `Settings::VERSION` and CI schema checks aligned
- NVS/EEPROM write-policy change:
  - audit all `settings.save()` call sites, not only the file currently being changed
  - remember that `Settings::load()` can persist on boot during migration/CRC/normalization flows
  - cover both `test_settings` and the command/runtime tests that exercise the changed write paths
- Version or release-note change:
  - keep `boatlock/main.cpp`, `CHANGELOG.md`, `docs/releases/`, and CI version checks aligned
- Release process change:
  - keep `docs/RELEASE_PROCESS.md`, `.github/workflows/ci.yml`, release branch checks, and service app firmware source defines aligned

## Common Commands

- Local pre-powered validation gate:
  - `tools/ci/run_local_prepowered_gate.sh`
- Firmware build:
  - `cd boatlock && pio run -e esp32s3`
- Explicit firmware profile builds:
  - `cd boatlock && pio run -e esp32s3_release`
  - `cd boatlock && pio run -e esp32s3_service`
  - `cd boatlock && pio run -e esp32s3_acceptance`
- Debug Wi-Fi/OTA firmware build:
  - `cd boatlock && BOATLOCK_WIFI_SSID=... BOATLOCK_WIFI_PASS=... BOATLOCK_OTA_PASS=... pio run -e esp32s3_debug_wifi_ota`
- BLE OTA phone bridge after a USB seed flash:
  - build `boatlock/.pio/build/esp32s3_service/firmware.bin`
  - publish/serve the binary from a trusted URL and copy its SHA-256
  - in the app Settings screen, use Firmware OTA URL + SHA-256 to upload over BLE
  - bench automation: `tools/hw/nh02/android-run-app-e2e.sh --ota --ota-firmware boatlock/.pio/build/esp32s3_service/firmware.bin`
- Latest-release phone bridge:
  - service app builds use `BOATLOCK_FIRMWARE_UPDATE_GITHUB_REPO=dslimp/boatlock`
  - local shortcuts: `tools/android/build-app-apk.sh --latest-release-service` and `tools/macos/build-app.sh --latest-release-service`
  - local manifest-backed bench automation: `tools/hw/nh02/android-run-app-e2e.sh --ota-latest-release --ota-firmware boatlock/.pio/build/esp32s3_service/firmware.bin`
- Full native tests:
  - `cd boatlock && platformio test -e native`
- Do not run multiple `platformio test -e native ...` commands in parallel against the same checkout/build directory. PlatformIO shares `.pio/build/native`, and parallel suites can kill or corrupt each other; run suites sequentially or use isolated worktrees/build dirs.
- Example targeted native suites:
  - `cd boatlock && platformio test -e native -f test_ble_command_handler`
  - `cd boatlock && platformio test -e native -f test_ble_security`
  - `cd boatlock && platformio test -e native -f test_gnss_quality_gate`
  - `cd boatlock && platformio test -e native -f test_hil_sim`
  - `cd boatlock && platformio test -e native -f test_settings`
- Flutter tests:
  - `cd boatlock_ui && flutter test`
- Offline simulation harness:
  - `python3 tools/sim/test_sim_core.py`
  - `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json`
  - `python3 tools/sim/test_soak.py`
  - `python3 tools/sim/run_soak.py --hours 6 --check --json-out tools/sim/soak_report.json`
- NH02 hardware bench:
  - `tools/hw/nh02/install.sh`
  - `tools/hw/nh02/flash.sh`
  - `tools/hw/nh02/flash.sh --profile release`
  - `tools/hw/nh02/flash.sh --profile service`
  - `tools/hw/nh02/flash.sh --profile acceptance`
  - `tools/hw/nh02/acceptance.sh`
  - `pytest -q tools/hw/nh02/test_acceptance.py`
  - `tools/hw/nh02/monitor.sh`
  - `tools/hw/nh02/status.sh`
  - `tools/hw/nh02/android-install.sh`
  - `tools/hw/nh02/android-status.sh`
  - `tools/hw/nh02/android-wifi-debug.sh`
  - rerun `install.sh` after changing the tracked service unit or remote flash helper
- microSD logger hardware proof:
  - flash a build with a FAT-formatted microSD inserted
  - require `[SD] logger ready=1`
  - verify display redraws still work
  - verify `/boatlock/*.jsonl` grows on the card
  - for rotation/low-space work, test file rotation or low-space deletion on real media before water use
- Android USB + BLE smoke:
  - `tools/android/status.sh`
  - `tools/android/build-app-apk.sh`
  - `tools/android/run-app-e2e.sh`
  - `tools/android/build-smoke-apk.sh`
  - `tools/android/run-smoke.sh`
  - `tools/hw/nh02/android-run-app-e2e.sh --manual --wait-secs 130`
  - `tools/hw/nh02/android-run-app-e2e.sh --compass --wait-secs 130`
  - `tools/hw/nh02/android-run-app-e2e.sh --gps --wait-secs 180`
  - `tools/hw/nh02/android-run-app-e2e.sh --ota --ota-firmware boatlock/.pio/build/esp32s3_service/firmware.bin`
  - `tools/hw/nh02/android-run-app-e2e.sh --sim-suite --wait-secs 1800`
  - `tools/hw/nh02/run-sim-suite.sh`
  - `tools/hw/nh02/android-run-app-e2e.sh --reconnect --wait-secs 130`
  - `tools/hw/nh02/android-run-app-e2e.sh --esp-reset --wait-secs 130`
  - `tools/hw/nh02/android-run-smoke.sh --manual --wait-secs 130`
  - `tools/hw/nh02/android-run-smoke.sh --compass --wait-secs 130`
  - `tools/hw/nh02/android-run-smoke.sh --gps --wait-secs 180`
  - `tools/hw/nh02/android-run-smoke.sh --reconnect --wait-secs 130`
  - `tools/hw/nh02/android-run-smoke.sh --esp-reset --wait-secs 130`
- Smoke APK mode constants and parser live in `boatlock_ui/lib/smoke/ble_smoke_mode.dart`.
- Production-app e2e builds use `BOATLOCK_APP_E2E_MODE` and `lib/main.dart`; do not count the smoke entrypoint as production-app acceptance.
- Shell smoke wrapper mode allowlist lives in `tools/android/common.sh`; do not duplicate hard-coded mode case lists in individual wrappers.
- When the smoke mode contract changes, run its Flutter unit test, `pytest -q tools/ci/test_android_smoke_modes.py`, and `tools/android/build-smoke-apk.sh --mode <mode>` at minimum.
- CI helper tests:
  - `pytest tools/ci/test_*.py`
- Quick audit for persistence call sites:
  - `rg -n "settings\\.save\\(" boatlock`

## Service/Dev/HIL Gate Validation

Firmware command enforcement is implemented in the shared BLE command path. PlatformIO profile aliases and the `nh02` flash wrapper profile report select which command scopes are compiled into the firmware command gate.

Profile rules:

- `release` profile: accepts release commands, including `SIM_*`, and rejects service plus dev/HIL commands before side effects.
- `service` profile: accepts release plus service commands, including BLE OTA and `SIM_*`, and rejects dev/HIL such as `SET_PHONE_GPS`.
- `acceptance` profile: accepts release, service, and dev/HIL for broad bench validation.
- `esp32s3_release`, `esp32s3_service`, and `esp32s3_acceptance` are the explicit profile aliases.
- `tools/hw/nh02/flash.sh --profile release|service|acceptance` maps to those explicit aliases.
- `BOATLOCK_PIO_ENV=release|service|acceptance` maps to those explicit aliases; an empty or unknown `BOATLOCK_PIO_ENV` is an error.
- Legacy `esp32s3` is a release-compatible environment for existing local and CI workflows.
- `esp32s3_bno08x_sh2_uart` and `esp32s3_debug_wifi_ota` are treated as service profile environments by the flash wrapper.
- `gpio_probe` and `uart_rvc_probe_rx12` are debug probe builds with no release/service/acceptance command-scope profile.

Minimum validation order after profile-gate changes:

1. Local docs/static checks:
   - `bash -n tools/hw/nh02/flash.sh`
   - `git diff --check -- boatlock/platformio.ini tools/hw/nh02/flash.sh docs/BLE_PROTOCOL.md docs/HARDWARE_NH02.md skills/boatlock/references/validation.md boatlock/TODO.md`
   - `rg -n "esp32s3_release|esp32s3_service|esp32s3_acceptance|release|service|dev/HIL|SIM_|OTA|BOATLOCK_PIO_ENV|PlatformIO|nh02" boatlock/platformio.ini tools/hw/nh02/flash.sh docs/BLE_PROTOCOL.md docs/HARDWARE_NH02.md skills/boatlock/references/validation.md boatlock/TODO.md`
2. Local firmware/unit checks, run sequentially in one build directory:
   - native BLE command/security tests cover each profile's allow/deny matrix
   - native HIL tests cover `SIM_*` as a release-scope safe simulation mode
   - each PlatformIO firmware profile builds without relying on stale `.pio` artifacts
3. Release-profile bench checks:
   - flash the release profile through `tools/hw/nh02/flash.sh --profile release`
   - run `tools/hw/nh02/acceptance.sh`
   - prove a representative service command and `OTA_BEGIN` are rejected with stable gate logs and no output motion
   - run Android `sim` smoke/e2e and verify `SIM` telemetry without motor/stepper output
4. Service-profile bench checks:
   - flash the service profile through `tools/hw/nh02/flash.sh --profile service`
   - run `tools/hw/nh02/android-run-app-e2e.sh --ota --ota-firmware boatlock/.pio/build/esp32s3_service/firmware.bin`
   - prove `SET_PHONE_GPS` is rejected with stable gate logs
5. Release-profile full HIL bench checks:
   - run `tools/hw/nh02/run-sim-suite.sh` when the standard full on-device HIL suite is required; it runs `SIM_LIST`, every listed scenario through `SIM_RUN:<id>,0`, `SIM_STATUS`, and `SIM_REPORT` on the release profile
6. Recovery check:
   - flash the release profile again through `tools/hw/nh02/flash.sh --profile release` and confirm the normal BLE reconnect/manual/anchor readiness smoke still uses the release surface only

Risk review items for the rollout:

- `SEC_CMD` must classify and gate the wrapped payload, not the raw envelope.
- A release image without service commands removes BLE OTA from that image; the USB seed and service-profile update path must remain documented.
- A service image must not accidentally expose `SIM_*` or phone-GPS injection.
- An acceptance image must not be left on the boat for water use because it exposes injected sensor and HIL commands.
- Wrapper output must distinguish profile-gated rejection from BLE timeout, auth failure, and parser failure.

## Useful Test Inventory

- Native firmware test directories live under `boatlock/test/`.
- High-signal suites for protocol/runtime work:
  - `test_ble_command_handler`
  - `test_ble_security`
  - `test_gnss_quality_gate`
  - `test_anchor_supervisor`
  - `test_hil_sim`
  - `test_settings`
- Flutter tests live under `boatlock_ui/test/`.

## Version And Schema Sync

- Firmware version string lives in `boatlock/main.cpp` as `kFirmwareVersion`.
- Version consistency is checked by `tools/ci/check_firmware_version.py`.
- Release tags use `vX.Y.Z`, are cut from `release/vX.Y.x`, and are checked by `tools/ci/check_release_ref.py`.
- GitHub Releases, not GitHub Pages, are the standard release delivery channel.
- Settings schema version lives in `boatlock/Settings.h` as `Settings::VERSION`.
- Schema consistency is checked by `tools/ci/check_config_schema_version.py`.
- Release-note generation helpers live under `tools/ci/generate_release_notes.py`.

## Documentation Sync Rules

- If you change commands, telemetry fields, status reasons, or security behavior, update `docs/BLE_PROTOCOL.md`.
- If you change settings defaults, ranges, or schema version, update `docs/CONFIG_SCHEMA.md`.
- If you change NVS/EEPROM write policy or persistence semantics, update the skill references and any operator docs that mention save behavior.
- If you change user-visible firmware behavior, update `CHANGELOG.md` and the relevant release note when appropriate.
- When prose docs disagree with code, fix the docs in the same change instead of preserving the mismatch.

## Wrapper Hygiene

- Repo bash wrappers must work under macOS bash 3.2 with `set -u`; do not expand empty arrays directly as `"${ARGS[@]}"`. Branch on `${#ARGS[@]}` before passing optional arg arrays.
