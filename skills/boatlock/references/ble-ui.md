# BLE And UI Reference

## Canonical Files

- `docs/BLE_PROTOCOL.md`: documented command/status surface
- `boatlock/BleCommandHandler.h`: current accepted command handlers
- `boatlock/BLEBoatLock.cpp`: BLE service, characteristics, advertising, queues
- `boatlock/main.cpp`: published telemetry params and security state
- `boatlock_ui/lib/ble/ble_boatlock.dart`: scan/connect/auth/write behavior
- `boatlock_ui/lib/models/boat_data.dart`: telemetry fields parsed by Flutter

## BLE Identity

- Device name: `BoatLock`
- Service UUID: `12ab`
- Characteristics:
  - `34cd`: data notify/read
  - `56ef`: command write/write-no-response
  - `78ab`: log notify
- Firmware boot logs should include BLE init and advertising result lines.

## Flutter Scan And Connect Behavior

- A device matches when:
  - advertised or platform name equals/contains `boatlock`, or
  - advertised service UUID contains `12ab`
- Scan is stopped immediately before connect.
- If nothing is found, the app retries after `3 s`.
- On disconnect, the app clears characteristics and schedules reconnect.
- After service discovery the app subscribes to:
  - data char `34cd`
  - log char `78ab`
- The app requests `"all"` params after connect.
- Heartbeat is sent every `1 s` once connected.

## Command Surface

- High-traffic command groups:
  - anchor: `SET_ANCHOR`, `ANCHOR_ON`, `ANCHOR_OFF`, `NUDGE_*`, `SET_ANCHOR_PROFILE`, `SET_HOLD_HEADING`
  - safety and link: `STOP`, `HEARTBEAT`
  - GPS and compass: `SET_PHONE_GPS`, `SET_COMPASS_OFFSET`, `RESET_COMPASS_OFFSET`
  - manual control: `MANUAL`, `MANUAL_DIR`, `MANUAL_SPEED`, `SET_STEPPER_BOW`
  - stepper tuning: `SET_STEP_MAXSPD`, `SET_STEP_ACCEL`, `SET_STEP_SPR`
  - simulation: `SIM_LIST`, `SIM_RUN`, `SIM_STATUS`, `SIM_REPORT`, `SIM_ABORT`
- `SET_STEP_SPR` is compatibility-only and remains fixed to `4096`.
- Route commands, phone-heading commands, and log-export commands are removed and should stay no-op unless intentionally restored.

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

- Flutter `BoatData` currently parses these fields:
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
- Firmware also publishes more fields that are not fully modeled in `BoatData`, including:
  - `gnssQ`
  - `anchorProfile`, `anchorProfileName`
  - `anchorDeniedReason`, `failsafeReason`
  - `secNonce`
  - `simState`, `simScenario`, `simProgress`, `simPass`
- `status` is a short health summary (`OK|WARN|ALERT`), while `mode` is the runtime mode and `statusReasons` carries comma-separated detail flags.
- If you add or rename telemetry, update firmware `registerBleParams()`, Flutter parsing, any affected widgets/tests, and `docs/BLE_PROTOCOL.md`.

## Manual UI Semantics

- Manual mode button toggles `MANUAL:<0|1>`.
- Steering buttons drive `MANUAL_DIR`.
- Speed slider drives `MANUAL_SPEED` in range `-255..255`.
- `SET_STEPPER_BOW` stores the current stepper position as bow zero.
- Stepper geometry is fixed to `4096` steps per revolution.
