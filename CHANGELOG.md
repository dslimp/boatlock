# Changelog

All notable changes to this project will be documented in this file.

## [0.2.0] - 2026-03-02
### Added
- Deterministic on-device HIL simulation loop (`SIM_LIST`, `SIM_RUN`, `SIM_STATUS`, `SIM_REPORT`, `SIM_ABORT`) with virtual clock and seeded PRNG.
- Extended built-in scenario pack from `S0..S9` to `S0..S19`, including random-current/noisy-GNSS profiles and hardware-failure emulation:
  - compass loss/restore
  - power loss/restore
  - display loss/restore
  - actuator derate windows
- Sticky simulation status badge on device UI (`SIM LIVE` during run, result banner after completion).
- Expanded HIL unit coverage, including required-failsafe scenarios and full default-scenario pass sweep.

### Changed
- BLE link handling hardened for CoreBluetooth (stable MTU/connection params, notify subscription tracking, queued command/log processing).
- HIL report generation now memory-bounded (event truncation + counters) and robust to long event streams.
- Scenario-specific control tuning for hard-current case (`S2`) using max-thrust override.
- `S9` policy aligned with safety intent: expected NaN-triggered failsafe is treated as pass.

### Fixed
- Firmware reboot under heavy `SIM_REPORT` payloads (`bad_alloc`/abort in report serialization path).
- Repeated GNSS jump-event storms after quality degradation windows.
- BLE write/notify instability under high traffic (`Resources are insufficient`, disconnect churn on repeated report pulls).

## [0.1.0] - 2026-03-01
### Removed
- Route subsystem from firmware, BLE protocol, and Flutter UI (`SET_ROUTE`, `START_ROUTE`, `STOP_ROUTE` and route pages).
- SD route logging commands and UI (`EXPORT_LOG`, `CLEAR_LOG`, log pages and related storage/service code).
- Phone heading emulation path (`EMU_COMPASS`, `SET_HEADING`) and compass calibration compatibility command (`CALIB_COMPASS`).

### Changed
- Heading source is now onboard BNO08x only; phone GPS fallback remains for position only.
- BLE/UI simplified to anchor hold, manual control, stepper tuning, compass offset, and core diagnostics.

## [0.1.5] - 2026-02-28
### Fixed
- Aligned BLE `SET_ANCHOR` command format in Flutter app with firmware protocol (`SET_ANCHOR:<lat>,<lon>`).
- Removed duplicate BLE stream subscriptions and unsafe global BLE disconnect behavior in the mobile app.
- Added PlatformIO native test config and expanded BLE command handler unit coverage.
- Updated protocol/docs links and command descriptions (`MANUAL_SPEED`, `EXPORT_LOG`, `CLEAR_LOG`).

## [0.1.4] - 2025-07-18
### Added
- Manual rudder and motor speed control via BLE commands and Flutter UI.

## [0.1.3] - 2025-07-18
### Added
- Support for the HC 160A S2 motor controller with dual direction pins.

## [0.1.2] - 2025-07-17
### Fixed
- Register BLE distance parameter after initialization so the mobile app receives the correct value

## [0.1.1] - 2025-07-17
### Added
- Persistent calibration for the HMC5883 compass stored in EEPROM
