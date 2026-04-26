# BLE And UI Reference

## Canonical Files

- `docs/BLE_PROTOCOL.md`: documented command/status surface
- `boatlock/BleCommandHandler.h`: current accepted command handlers
- `boatlock/BLEBoatLock.cpp`: BLE service, characteristics, advertising, queues
- `boatlock/RuntimeBleLiveFrame.h`: live binary telemetry frame encoder and enum mapping
- `boatlock/RuntimeBleParams.h`: typed runtime telemetry provider
- `boatlock_ui/lib/ble/ble_boatlock.dart`: scan/connect/auth/write behavior
- `boatlock_ui/lib/ble/ble_ids.dart`: Flutter BLE device name, service UUID, and characteristic UUID constants
- `boatlock_ui/lib/ble/ble_device_match.dart`: adapter readiness and BoatLock advertisement matching
- `boatlock_ui/lib/ble/ble_discovery_check.dart`: required GATT characteristic completeness checks
- `boatlock_ui/lib/ble/ble_scan_config.dart`: scan service filter and scan timing constants
- `boatlock_ui/lib/ble/ble_rssi_throttle.dart`: app-side RSSI read throttling policy
- `boatlock_ui/lib/ble/ble_commands.dart`: pure command builders and value allowlists for app-originated commands
- `boatlock_ui/lib/ble/ble_command_scope.dart`: app-side release/service/dev command-surface classifier and compile-time service/dev UI gates
- `boatlock_ui/lib/ble/ble_security_codec.dart`: owner-secret normalization, owner auth proof, and secure command envelope formatting
- `boatlock_ui/lib/ble/ble_live_frame.dart`: live binary telemetry decoder
- `boatlock_ui/lib/ble/ble_log_line.dart`: log characteristic byte-string decoder
- `boatlock_ui/lib/models/boat_data.dart`: telemetry fields parsed by Flutter
- `boatlock_ui/lib/smoke/ble_smoke_mode.dart`: Android smoke APK mode enum and compile-time mode parser
- `boatlock_ui/lib/smoke/ble_smoke_logic.dart`: pure Android smoke result/stage and telemetry verdict helpers

## BLE Identity

- Device name: `BoatLock`
- Service UUID: `12ab`
- Characteristics:
  - `34cd`: live state notify/read, 72-byte little-endian binary v3 frame
  - `56ef`: control point write/write-no-response
  - `78ab`: log notify
  - `9abc`: firmware OTA binary write/write-no-response, armed only by `OTA_BEGIN`
- Flutter UUID/name constants live in `ble_ids.dart`; do not hardcode these identifiers in scan, discovery, or tests.
- Flutter UUID matching should go through `isBoatLockUuid()` from `ble_ids.dart`, accepting only the exact 16-bit form or the Bluetooth base UUID expansion. Do not use substring matching for services or characteristics.
- Service discovery should inspect BoatLock characteristics only under service `12ab`; a matching characteristic UUID under another service is not a valid BoatLock control/data/log endpoint.
- A connected GATT service is usable only when data `34cd`, command `56ef`, and log `78ab` characteristics are all found. Partial discovery must clear the link and retry instead of leaving the app half-connected.
- BLE OTA is optional for basic connection but required for app-driven firmware update. The app must discover `9abc` before allowing an OTA upload.
- The app should bind data, command, log, and OTA characteristic references before enabling data/log notifications. Notifications can arrive immediately after subscription and may start e2e logic that needs the OTA characteristic.
- On Android, if data/command/log are present but OTA is missing, the app may clear the GATT cache and rediscover once before declaring the link incomplete.
- Live notify payload length must be validated before `setValue()`; producer-side packets whose length is not exactly the live-frame size are dropped rather than padded, truncated, or read past the packet buffer.
- Flutter must reject live-frame payloads whose length is not exactly 72 bytes; accepting padded frames hides characteristic-value bugs and can mask protocol drift.
- Flutter live-frame decoding must reject unknown mode/status/security-reject enum codes while the frame version is unchanged; `UNKNOWN` display fallbacks hide firmware/app schema drift.
- Flutter `BoatData` is populated from the binary live-frame decoder only; do not keep a parallel JSON telemetry parser unless a real shipped protocol path is restored end-to-end.
- Log characteristic values are byte strings with explicit length. Firmware must set the exact text length, and Flutter must ignore trailing NUL padding defensively.
- Flutter log decoding belongs in `ble_log_line.dart`; keep it length-delimited and tolerate malformed UTF-8 as display-only diagnostics.
- BLE log queue writes must build bounded, NUL-terminated payload slots without `strlen()` on untrusted or length-unknown input.
- BLE log notifications must stay single-line: trim trailing CR/LF and neutralize embedded ASCII control bytes before enqueueing or publishing over `78ab`.
- BLE command-derived log fields must go through `RuntimeBleCommandLog`: suppress high-rate `HEARTBEAT`, redact `PAIR_SET`, `AUTH_PROVE`, and `SEC_CMD`, neutralize control bytes, and bound field length.
- BLE command queue writes must reject overlong commands before enqueueing; do not truncate a command into a different syntactically valid command.
- BLE command queue writes must accept printable ASCII command bytes only; embedded NUL/control/non-ASCII bytes must fail before any C-string conversion.
- Flutter must validate the same command byte contract before writing `56ef`: non-empty, max `191` printable ASCII bytes.
- Firmware boot logs should include BLE init and advertising result lines.
- Firmware runs a BLE advertising watchdog:
  - if the server has no connected clients and advertising is stopped, restart advertising
  - if firmware state says connected but the server has no clients, clear stale BLE stream/subscription state and restart advertising
  - if at least one client is connected and advertising is stopped, restart advertising without clearing active stream/subscription state
- BLE callback code must not clear active stream/notify state on a second central connect or on disconnect while another central remains connected.
- BLE connection/disconnection logs should use short key=value fields, sanitize address text to bounded hex/colon tokens, and preserve raw disconnect reasons in decimal plus hex for NimBLE/HCI triage.

## Flutter Scan And Connect Behavior

- A device matches when:
  - advertised or platform name equals/contains `boatlock`, or
  - advertised service UUID contains `12ab`
- Device matching belongs in `ble_device_match.dart` and should stay testable without starting a BLE scan.
- Active Flutter scan should use the BoatLock service UUID filter from `ble_scan_config.dart`, then keep the name/service advertisement matcher as a defensive app-side validation before connecting.
- Scan timeout and completion wait constants belong in `ble_scan_config.dart`; do not scatter scan durations in transport code.
- Scan is stopped immediately before connect.
- If nothing is found, the app retries after `3 s`.
- On disconnect, the app clears characteristics and schedules reconnect.
- The app watches Bluetooth adapter state while running:
  - adapter off/unavailable stops scanning and clears stale GATT state
  - adapter on schedules a fresh scan
- On app resume, the app schedules a fresh scan unless an active data/control link already exists.
- Heartbeat write failure is treated as a link loss and schedules reconnect.
- RSSI reads should be throttled through `ble_rssi_throttle.dart`; do not call GATT RSSI reads on every telemetry notification.
- Firmware logs should suppress high-frequency `HEARTBEAT` command lines while preserving operator/service commands.
- Immediate BLE transport commands are exact-match only: `STREAM_START`, `STREAM_STOP`, and `SNAPSHOT`. Prefixes, suffixes, and decorated variants must fall through to the normal command path and be rejected there if invalid.
- App command builders should remain pure top-level functions in `ble_commands.dart`; do not hide value formatting/range checks inside the stateful BLE transport class.
- Security envelope formatting should remain pure top-level functions in `ble_security_codec.dart`; `BleBoatLock` owns session state and write order, not reusable codec internals.
- After service discovery the app subscribes to:
  - data char `34cd`
  - log char `78ab`
- The app sends `STREAM_START`, then `SNAPSHOT`, after connect.
- Heartbeat is sent every `1 s` once connected.
- A successful app-side BLE write only proves that a command request was sent.
  Operator history and UI copy must not call it firmware acceptance; acceptance
  comes from later telemetry transitions or allowlisted device event logs.

## Android Smoke App

- Smoke APK mode selection is compiled through `BOATLOCK_SMOKE_MODE`.
- Supported smoke mode names are `basic`, `reconnect`, `manual`, `status`, `sim`, and `anchor`.
- The mode enum and parser belong in `ble_smoke_mode.dart`, not in `main_smoke.dart` or the smoke page widget.
- `main_smoke.dart` should stay entrypoint glue: read the compile-time define, parse it through the pure helper, and construct `BleSmokePage`.
- Smoke result and stage line encoding belongs in `ble_smoke_logic.dart` so wrapper parsing can be covered by unit tests before Android device runs.

## Production App E2E

- Production-app bench tests must build `lib/main.dart`, not `lib/main_smoke.dart`.
- Main-app e2e hooks are enabled only by the compile-time `BOATLOCK_APP_E2E_MODE` define; normal app builds leave the hook disabled.
- Main-app e2e verdicts intentionally reuse `BOATLOCK_SMOKE_STAGE` and `BOATLOCK_SMOKE_RESULT` lines so the canonical adb/logcat runner remains the single acceptance path.
- Use `tools/android/build-app-apk.sh --e2e-mode <mode>` to build a debuggable main-app APK with the hook enabled.
- Use `tools/hw/nh02/android-run-app-e2e.sh --<mode> --wait-secs 130` for nh02 production-app BLE acceptance.
- Keep mode names aligned with `BleSmokeMode` unless the production app needs a flow that cannot share the existing safe command contract.

## Command Surface

- Command scope groups:
  - release: stream/control point, explicit anchor save/enable/disable, manual deadman, STOP/heartbeat, anchor jog, hold-heading, and pairing/auth commands.
  - service: anchor tuning profiles, heading offset/reset, SH2 compass calibration/DCD/tare, stepper bow/tuning, and BLE OTA.
  - dev/HIL: `SET_PHONE_GPS` and `SIM_*`.
- The normal Flutter water UI must expose only release commands.
- Service UI is hidden unless the app is built with `--dart-define=BOATLOCK_SERVICE_UI=true` or a test/service harness passes `allowService: true` to `sendCustomCommand()`.
- Dev/HIL commands are hidden unless the app is built with `--dart-define=BOATLOCK_DEV_HIL_COMMANDS=true` or a validation harness passes `allowDevHil: true` to `sendCustomCommand()`.
- `SEC_CMD` is the security envelope; the effective scope is the scope of the wrapped payload. Flutter custom-command callers should pass the unwrapped command and let `BleBoatLock` build the envelope after classification.
- Do not keep compatibility-only BLE commands in firmware or Flutter. If a command is obsolete, remove it from command handling, UI, tests, and docs in the same change.
- Route commands, phone-heading commands, and log-export commands are removed and should stay no-op unless intentionally restored.

## Multi-Client Rule

- Future hardware may need a phone plus a remote/controller connected over BLE.
- Advertising while connected is allowed so future phone + remote setups can discover the ESP32 without a power-cycle.
- Multi-central transport support does not replace control-source arbitration.
- During BLE OTA, the firmware must bind OTA ownership to the connection handle that armed the update. Non-owner disconnects must not abort an active OTA transfer; owner disconnects or validation failures must abort.
- Required multi-client design before accepting real simultaneous control:
  - multiple read-only telemetry subscribers are allowed
  - at most one control owner/lease can send actuation or settings commands
  - pairing/auth/session state must be per-client or command writes from secondary clients must be rejected
  - tests must cover two centrals attempting telemetry and control at the same time

## Security Envelope

- Pairing is opened only by holding the hardware STOP button for `3 s`.
- Pairing window duration is `120 s`.
- Pairing uses `PAIR_SET:<ownerSecretHex>` where the owner secret is 32 hex chars.
- Owner authentication flow:
  1. `AUTH_HELLO`
  2. read `secNonce`
  3. `AUTH_PROVE:<proofHex>`
- `PAIR_CLEAR` is accepted only from owner session or while the pairing window is still physically open.
- When paired, control/write commands must be wrapped as:
  - `SEC_CMD:<counterHex>:<sigHex>:<payload>`
- Flutter handles this in `_ensureSecureSession()`, `_writeCommand()`, and `buildSecureCommand()`.
- `OTA_BEGIN`, `OTA_FINISH`, and `OTA_ABORT` are control/write commands. When paired, they go through the same `SEC_CMD` envelope; raw binary chunks are accepted only after an authenticated `OTA_BEGIN` has armed the OTA writer.

## Telemetry Coupling

- Live telemetry is a fixed binary v3 frame, not JSON and not a parameter-read tunnel.
- Firmware builds one typed `RuntimeBleLiveTelemetry` snapshot and encodes it through `RuntimeBleLiveFrame.h`.
- Telemetry snapshot builders must validate coordinate pairs as pairs; if an anchor position is invalid/default, publish the whole anchor position and heading as neutral `0`.
- Snapshot builders should sample volatile readiness predicates once per frame so related fields come from one consistent readiness state.
- Flutter decodes `34cd` notifications through `ble_live_frame.dart`; `BoatData.fromJson()` remains only for model-level unit tests and non-BLE helpers.
- `distance` and `anchorBearing` are display telemetry from the published position to the saved anchor point; they must not be treated as proof that the hardware-only anchor-control GNSS gate is ready.
- Flutter `BoatData` currently receives these fields from the live frame:
  - `lat`, `lon`
  - `anchorLat`, `anchorLon`, `anchorHead`
  - `distance`, `anchorBearing`, `heading`
  - `battery`, `status`, `statusReasons`, `mode`, `rssi`
  - `holdHeading`
  - `stepSpr`, `stepMaxSpd`, `stepAccel`
  - `headingRaw`, `compassOffset`
  - `compassQ`, `magQ`, `gyroQ`
  - `rvAcc`, `magNorm`, `gyroNorm`
  - `pitch`, `roll`
  - `secPaired`, `secAuth`, `secPairWin`, `secReject`
- The auth nonce is carried as binary `uint64` in the live frame and rendered by Flutter as 16 hex chars.
- `status` is a short health summary (`OK|WARN|ALERT`), while `mode` is the runtime mode and `statusReasons` carries comma-separated detail flags.
- Binary live-frame reason flags should be generated from one explicit token-to-bit table and matched as exact CSV tokens, not substring checks.
- Firmware live-frame scaling must clamp in floating-point space before integer rounding/casting; non-finite values map to neutral `0`.
- Firmware telemetry snapshot builders must sanitize direct integer fields before assigning them into the live-frame struct; do not rely on later encoder casts to make bad float settings safe.
- Telemetry quality ordinals are bounded enums: compass/mag/gyro quality must be `0..3`, GNSS quality must be `0..2`, and out-of-range values publish as `0` rather than a high-confidence value.
- If you add telemetry, update `RuntimeBleLiveFrame.h`, `ble_live_frame.dart`, affected tests, and `docs/BLE_PROTOCOL.md` together.
- Pure range-hardening inside existing fields does not require a frame-version bump when the byte layout and decoder contract are unchanged.

## Manual UI Semantics

- Manual control is core product scope for both the phone app and future BLE joystick/remotes.
- Manual control must be source-agnostic internally: every controller feeds the same manual-control state model.
- Phone BLE control uses `MANUAL_SET:<steer>,<throttlePct>,<ttlMs>`:
  - `steer=-1..1`
  - `throttlePct=-100..100`
  - `ttlMs=100..1000`
- Do not log high-frequency `HEARTBEAT` as an operator command; keep serial/BLE diagnostics focused on state-changing commands and events.
- Each `MANUAL_SET` is an atomic command and deadman refresh. If updates stop, firmware exits Manual and quiets outputs.
- `MANUAL_SET` disables Anchor on entry so timeout, reconnect, or app crash cannot resume Anchor unexpectedly.
- `MANUAL_OFF` stops Manual output explicitly.
- Do not reintroduce split legacy commands such as `MANUAL`, `MANUAL_DIR`, or `MANUAL_SPEED`.
- `SET_STEPPER_BOW` stores the current stepper position as bow zero.
- Stepper geometry is fixed to `4096` steps per revolution.
