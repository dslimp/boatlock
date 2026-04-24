# BLE And UI Reference

## Canonical Files

- `docs/BLE_PROTOCOL.md`: documented command/status surface
- `boatlock/BleCommandHandler.h`: current accepted command handlers
- `boatlock/BLEBoatLock.cpp`: BLE service, characteristics, advertising, queues
- `boatlock/RuntimeBleLiveFrame.h`: live binary telemetry frame encoder and enum mapping
- `boatlock/RuntimeBleParams.h`: typed runtime telemetry provider
- `boatlock_ui/lib/ble/ble_boatlock.dart`: scan/connect/auth/write behavior
- `boatlock_ui/lib/ble/ble_live_frame.dart`: live binary telemetry decoder
- `boatlock_ui/lib/models/boat_data.dart`: telemetry fields parsed by Flutter

## BLE Identity

- Device name: `BoatLock`
- Service UUID: `12ab`
- Characteristics:
  - `34cd`: live state notify/read, 70-byte little-endian binary v2 frame
  - `56ef`: control point write/write-no-response
  - `78ab`: log notify
- Firmware boot logs should include BLE init and advertising result lines.
- Firmware runs a BLE advertising watchdog:
  - if the server has no connected clients and advertising is stopped, restart advertising
  - if firmware state says connected but the server has no clients, clear stale BLE stream/subscription state and restart advertising
- Do not enable continuous advertising while a client is connected until the firmware has a real multi-client model. The current control/session state is global.

## Flutter Scan And Connect Behavior

- A device matches when:
  - advertised or platform name equals/contains `boatlock`, or
  - advertised service UUID contains `12ab`
- Scan is stopped immediately before connect.
- If nothing is found, the app retries after `3 s`.
- On disconnect, the app clears characteristics and schedules reconnect.
- The app watches Bluetooth adapter state while running:
  - adapter off/unavailable stops scanning and clears stale GATT state
  - adapter on schedules a fresh scan
- On app resume, the app schedules a fresh scan unless an active data/control link already exists.
- Heartbeat write failure is treated as a link loss and schedules reconnect.
- After service discovery the app subscribes to:
  - data char `34cd`
  - log char `78ab`
- The app sends `STREAM_START`, then `SNAPSHOT`, after connect.
- Heartbeat is sent every `1 s` once connected.

## Command Surface

- High-traffic command groups:
  - stream/control point: `STREAM_START`, `STREAM_STOP`, `SNAPSHOT`
  - anchor: `SET_ANCHOR`, `ANCHOR_ON`, `ANCHOR_OFF`, `NUDGE_*`, `SET_ANCHOR_PROFILE`, `SET_HOLD_HEADING`
  - manual control: `MANUAL_SET`, `MANUAL_OFF`
  - safety and link: `STOP`, `HEARTBEAT`
  - GPS and compass: `SET_PHONE_GPS`, `SET_COMPASS_OFFSET`, `RESET_COMPASS_OFFSET`
  - stepper service: `SET_STEPPER_BOW`
  - stepper tuning: `SET_STEP_MAXSPD`, `SET_STEP_ACCEL`
  - simulation: `SIM_LIST`, `SIM_RUN`, `SIM_STATUS`, `SIM_REPORT`, `SIM_ABORT`
- Do not keep compatibility-only BLE commands in firmware or Flutter. If a command is obsolete, remove it from command handling, UI, tests, and docs in the same change.
- Route commands, phone-heading commands, and log-export commands are removed and should stay no-op unless intentionally restored.

## Multi-Client Rule

- Future hardware may need a phone plus a remote/controller connected over BLE.
- Do not implement this by simply advertising while connected.
- Required multi-client design before enabling that:
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

## Telemetry Coupling

- Live telemetry is a fixed binary v2 frame, not JSON and not a parameter-read tunnel.
- Firmware builds one typed `RuntimeBleLiveTelemetry` snapshot and encodes it through `RuntimeBleLiveFrame.h`.
- Flutter decodes `34cd` notifications through `ble_live_frame.dart`; `BoatData.fromJson()` remains only for model-level unit tests and non-BLE helpers.
- Flutter `BoatData` currently receives these fields from the live frame:
  - `lat`, `lon`
  - `anchorLat`, `anchorLon`, `anchorHead`
  - `distance`, `heading`
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
- If you add telemetry, update `RuntimeBleLiveFrame.h`, `ble_live_frame.dart`, affected tests, and `docs/BLE_PROTOCOL.md` together.

## Manual UI Semantics

- Manual control is core product scope for both the phone app and future BLE joystick/remotes.
- Manual control must be source-agnostic internally: every controller feeds the same manual-control state model.
- Phone BLE control uses `MANUAL_SET:<steer>,<throttlePct>,<ttlMs>`:
  - `steer=-1..1`
  - `throttlePct=-100..100`
  - `ttlMs=100..1000`
- Each `MANUAL_SET` is an atomic command and deadman refresh. If updates stop, firmware exits Manual and quiets outputs.
- `MANUAL_SET` disables Anchor on entry so timeout, reconnect, or app crash cannot resume Anchor unexpectedly.
- `MANUAL_OFF` stops Manual output explicitly.
- Do not reintroduce split legacy commands such as `MANUAL`, `MANUAL_DIR`, or `MANUAL_SPEED`.
- `SET_STEPPER_BOW` stores the current stepper position as bow zero.
- Stepper geometry is fixed to `4096` steps per revolution.
