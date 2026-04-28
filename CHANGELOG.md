# Changelog

All notable changes to this project will be documented in this file.

## [0.2.4] - 2026-04-28
### Fixed
- Deferred BLE reconnect/scan teardown while Android is actively uploading OTA
  firmware, preventing stale peer disconnects from interrupting the one-button
  GitHub firmware update path.
- Updated `nh02` app-check helpers so `--ota-latest-release` proves the public
  GitHub Release path without requiring a temporary local manifest.
- Bumped the Android/macOS app package version to `0.2.4+4` so installed phone
  builds can be identified without relying only on APK hashes.

## [0.2.3] - 2026-04-28
### Changed
- Simplified the Android ESP32 firmware update UI to two operator paths:
  selecting a local `firmware.bin` file on the phone or installing the latest
  firmware from the GitHub Release button.
- Removed operator-facing firmware URL/SHA fields and app-check controls from
  the Settings screen; URL/SHA OTA remains only as hidden bench automation.

## [0.2.2] - 2026-04-28
### Changed
- Android GitHub Release APKs now require stable release signing secrets on
  tagged builds so phone-side APK updates keep the same certificate across
  releases.
- Branch and pull-request Android builds may still use debug signing fallback
  for CI coverage, but tagged `v*` releases fail instead of publishing an
  update-incompatible debug-signed APK.

## [0.2.1] - 2026-04-27
### Added
- Added a standard `nh02` phone BLE OTA deploy wrapper (`tools/hw/nh02/deploy.sh`) that builds firmware and the release APK, installs the exact APK, uploads firmware through the phone, and waits for post-update telemetry recovery.
- Added release-app telemetry parsing for the previous v3 live frame so a newly installed app can still bridge an already-deployed v3 firmware to the current v4 firmware over BLE OTA.

### Changed
- Standardized phone/BLE manual control on atomic `MANUAL_TARGET:<angleDeg>,<throttlePct>,<ttlMs>` and `MANUAL_OFF`.
- Manual entry now disables Anchor mode and uses a deadman TTL so app/controller loss cannot resume Anchor unexpectedly.
- Split steering setup into motor steps per revolution and gear ratio, with `StepSpr=200` and `StepGear=36` defaults for the current DRV8825 full-step hardware.
- Raised default stepper speed/acceleration for usable manual steering response.
- Improved `nh02` Android OTA runner logging with artifact hashes, app launch state, progress heartbeats, and early logcat context when OTA does not start.

### Removed
- Removed legacy split manual commands `MANUAL`, `MANUAL_DIR`, and `MANUAL_SPEED` from the accepted command surface.

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
