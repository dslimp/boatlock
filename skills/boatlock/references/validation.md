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
- Version or release-note change:
  - keep `boatlock/main.cpp`, `CHANGELOG.md`, `docs/releases/`, and CI version checks aligned

## Common Commands

- Firmware build:
  - `cd boatlock && pio run -e esp32s3`
- Alt-board build:
  - `cd boatlock && pio run -e jc4832w535`
- Full native tests:
  - `cd boatlock && platformio test -e native`
- Example targeted native suites:
  - `cd boatlock && platformio test -e native -f test_ble_command_handler`
  - `cd boatlock && platformio test -e native -f test_ble_security`
  - `cd boatlock && platformio test -e native -f test_gnss_quality_gate`
  - `cd boatlock && platformio test -e native -f test_hil_sim`
- Flutter tests:
  - `cd boatlock_ui && flutter test`
- Offline simulation harness:
  - `python3 tools/sim/test_sim_core.py`
  - `python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json`
  - `python3 tools/sim/test_soak.py`
  - `python3 tools/sim/run_soak.py --hours 6 --check --json-out tools/sim/soak_report.json`
- CI helper tests:
  - `pytest tools/ci/test_*.py`

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
- If you change user-visible firmware behavior, update `CHANGELOG.md` and the relevant release note when appropriate.
- When prose docs disagree with code, fix the docs in the same change instead of preserving the mismatch.
