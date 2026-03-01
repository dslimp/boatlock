# Changelog

All notable changes to this project will be documented in this file.

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
