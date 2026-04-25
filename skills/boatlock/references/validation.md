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
- EEPROM/write-policy change:
  - audit all `settings.save()` call sites, not only the file currently being changed
  - remember that `Settings::load()` can persist on boot during migration/CRC/normalization flows
  - cover both `test_settings` and the command/runtime tests that exercise the changed write paths
- Version or release-note change:
  - keep `boatlock/main.cpp`, `CHANGELOG.md`, `docs/releases/`, and CI version checks aligned

## Common Commands

- Firmware build:
  - `cd boatlock && pio run -e esp32s3`
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
- If you change EEPROM write policy or persistence semantics, update the skill references and any operator docs that mention save behavior.
- If you change user-visible firmware behavior, update `CHANGELOG.md` and the relevant release note when appropriate.
- When prose docs disagree with code, fix the docs in the same change instead of preserving the mismatch.

## Wrapper Hygiene

- Repo bash wrappers must work under macOS bash 3.2 with `set -u`; do not expand empty arrays directly as `"${ARGS[@]}"`. Branch on `${#ARGS[@]}` before passing optional arg arrays.
