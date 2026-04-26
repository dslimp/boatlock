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

## Common Commands

- Firmware build:
  - `cd boatlock && pio run -e esp32s3`
- Debug Wi-Fi/OTA firmware build:
  - `cd boatlock && BOATLOCK_WIFI_SSID=... BOATLOCK_WIFI_PASS=... BOATLOCK_OTA_PASS=... pio run -e esp32s3_debug_wifi_ota`
- BLE OTA phone bridge after a USB seed flash:
  - build `boatlock/.pio/build/esp32s3/firmware.bin`
  - publish/serve the binary from a trusted URL and copy its SHA-256
  - in the app Settings screen, use Firmware OTA URL + SHA-256 to upload over BLE
  - bench automation: `tools/hw/nh02/android-run-app-e2e.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin`
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
  - `tools/hw/nh02/acceptance.sh`
  - `pytest -q tools/hw/nh02/test_acceptance.py`
  - `tools/hw/nh02/monitor.sh`
  - `tools/hw/nh02/status.sh`
  - `tools/hw/nh02/android-install.sh`
  - `tools/hw/nh02/android-status.sh`
  - `tools/hw/nh02/android-wifi-debug.sh`
  - rerun `install.sh` after changing the tracked service unit or remote flash helper
- Android USB + BLE smoke:
  - `tools/android/status.sh`
  - `tools/android/build-app-apk.sh`
  - `tools/android/run-app-e2e.sh`
  - `tools/android/build-smoke-apk.sh`
  - `tools/android/run-smoke.sh`
  - `tools/hw/nh02/android-run-app-e2e.sh --manual --wait-secs 130`
  - `tools/hw/nh02/android-run-app-e2e.sh --compass --wait-secs 130`
  - `tools/hw/nh02/android-run-app-e2e.sh --gps --wait-secs 180`
  - `tools/hw/nh02/android-run-app-e2e.sh --ota --ota-firmware boatlock/.pio/build/esp32s3/firmware.bin`
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

## Planned Service/Dev/HIL Gate Validation

This section is for the open firmware-side command-scope gate and is implementation planning only. Do not treat it as proof that the gate exists.

The gate may land only with explicit PlatformIO profile rules and `nh02` wrapper support:

- `release` profile: accepts only release commands and rejects service plus dev/HIL commands before side effects.
- `service` profile: accepts release plus service commands, including BLE OTA, and rejects dev/HIL.
- `acceptance` profile: accepts release, service, and dev/HIL so on-device `SIM_*` remains available for bench acceptance.
- Existing environments such as `esp32s3`, `esp32s3_bno08x_sh2_uart`, and `esp32s3_debug_wifi_ota` must be mapped to one of those profiles or replaced by unambiguous profile-specific names before the firmware gate merges.

Minimum validation order for the gate rollout:

1. Local docs/static checks:
   - `git diff --check -- docs/BLE_PROTOCOL.md docs/HARDWARE_NH02.md skills/boatlock/references/validation.md boatlock/TODO.md`
   - `rg -n "release|service|dev/HIL|SIM_|OTA|BOATLOCK_PIO_ENV|PlatformIO|nh02" docs/BLE_PROTOCOL.md docs/HARDWARE_NH02.md skills/boatlock/references/validation.md boatlock/TODO.md`
2. Local firmware/unit checks, run sequentially in one build directory:
   - native BLE command/security tests cover each profile's allow/deny matrix
   - native HIL tests cover `SIM_*` only for the acceptance profile
   - each PlatformIO firmware profile builds without relying on stale `.pio` artifacts
3. Release-profile bench checks:
   - flash the release profile through `tools/hw/nh02/flash.sh`
   - run `tools/hw/nh02/acceptance.sh`
   - prove a representative service command, `OTA_BEGIN`, and `SIM_RUN` are rejected with stable gate logs and no output motion
4. Service-profile bench checks:
   - flash the service profile
   - run `tools/hw/nh02/android-run-app-e2e.sh --ota --ota-firmware boatlock/.pio/build/<service-env>/firmware.bin`
   - prove `SIM_*` is rejected with stable gate logs
5. Acceptance-profile bench checks:
   - flash the acceptance profile
   - run `tools/hw/nh02/acceptance.sh`
   - run Android `sim` smoke/e2e and require `SIM_LIST`, `SIM_RUN`, `SIM_STATUS`, `SIM_REPORT`, and `SIM_ABORT` through the normal BLE command path
6. Recovery check:
   - flash the release profile again and confirm the normal BLE reconnect/manual/anchor readiness smoke still uses the release surface only

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
