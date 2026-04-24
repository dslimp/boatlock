# BLE Protocol

BoatLock uses one custom GATT service for the app and bench tooling:

- Service UUID `12ab`
- Live state characteristic `34cd`: notify/read, compact binary v2 frame
- Control point characteristic `56ef`: write/write-no-response, ASCII commands
- Device log characteristic `78ab`: notify, text log lines

The live path is not a serial/JSON tunnel. The app subscribes to `34cd`, sends `STREAM_START`, and receives fixed-size binary live frames. Large snapshots and legacy parameter reads are not part of the protocol.

## Control Point Commands

| Command | Parameters | Description |
|--------|------------|-------------|
| `STREAM_START` | none | Enable periodic live-state notifications and immediately emit one live frame |
| `STREAM_STOP` | none | Disable periodic live-state notifications and clear queued live frames |
| `SNAPSHOT` | none | Emit one live-state frame without changing stream state |
| `SET_ANCHOR:<lat>,<lon>` | valid non-zero `lat` and `lon` decimal degrees (floats) | Save anchor position with current heading; does not enable Anchor mode |
| `ANCHOR_ON` | none | Enable anchor mode only if a saved anchor point exists, onboard heading is available, and GNSS quality is sufficient |
| `ANCHOR_OFF` | none | Disable anchor mode immediately, stop current actuation, and clear any latched `HOLD` state |
| `STOP` | none | Emergency stop: disable anchor/manual control, stop all motors, and latch runtime `HOLD` mode (highest priority) |
| `HEARTBEAT` | none | Keep-alive command from controller/app; missing heartbeat while Anchor is active triggers failsafe |
| `MANUAL_SET:<steer>,<throttlePct>,<ttlMs>` | `steer=-1..1`, `throttlePct=-100..100`, `ttlMs=100..1000` | Atomically enter/refresh Manual mode from the active controller; this disables Anchor mode and acts as a deadman lease |
| `MANUAL_OFF` | none | Stop Manual mode and zero manual stepper/thruster output |
| `NUDGE_DIR:<FWD\|BACK\|LEFT\|RIGHT>` | direction | Shift anchor point by the fixed 1.5 m jog step in boat frame (allowed only while Anchor is active and safety checks pass) |
| `NUDGE_BRG:<bearingDeg>` | absolute bearing | Shift anchor point by the fixed 1.5 m jog step on the given bearing (allowed only while Anchor is active and safety checks pass) |
| `SET_ANCHOR_PROFILE:<quiet\|normal\|current>` | profile id | Atomically apply preset Anchor control params and persist them |
| `PAIR_SET:<ownerSecretHex>` | 32-hex owner secret | Set/replace owner secret while pairing window is open from hardware STOP long-press |
| `PAIR_CLEAR` | none | Clear pairing; accepted only from owner session or while pairing window is physically open |
| `AUTH_HELLO` | none | Start owner auth challenge; firmware generates `secNonce` |
| `AUTH_PROVE:<proofHex>` | 16-hex chars | Complete owner auth for current nonce using owner secret MAC |
| `SEC_CMD:<counterHex>:<sigHex>:<payload>` | secured command envelope | Authenticated command with anti-replay counter and 64-bit MAC |
| `SET_HOLD_HEADING:<0|1>` | integer flag | Enable (`1`) or disable (`0`) heading hold |
| `SET_PHONE_GPS:<lat>,<lon>[,<speedKmh>[,<satellites>]]` | latitude, longitude, optional speed in km/h, optional satellites used in fix | External tooling command; the main app control path does not use phone GPS as an anchor-control source |
| `SET_COMPASS_OFFSET:<deg>` | heading offset in degrees (float) | Set persistent yaw offset for onboard BNO08x heading |
| `RESET_COMPASS_OFFSET` | none | Reset persistent BNO08x heading offset to `0` |
| `SET_STEP_MAXSPD:<float>` | float | Maximum stepper speed (steps/s) |
| `SET_STEP_ACCEL:<float>` | float | Stepper acceleration (steps/s²) |
| `SET_STEPPER_BOW` | none | Save current stepper position as boat-bow zero for anchor pointing |
| `SIM_LIST` | none | List built-in on-device HIL scenarios (`S0..S19`) |
| `SIM_RUN:<scenario_id>[,<speedup>]` | id + speed mode (`0` fastest, `1` realtime) | Start deterministic closed-loop simulation on device |
| `SIM_STATUS` | none | Return current simulation progress JSON |
| `SIM_REPORT` | optional scenario id | Return final simulation report JSON (chunked in logs) |
| `SIM_ABORT` | none | Abort currently running simulation |

Current built-in HIL groups:
- `S0..S3`: baseline hold/current/gust behavior
- `S4..S9`: GNSS/control-loop/NaN safety regressions
- `S10..S19`: randomized + hardware-failure emulation (compass/power/display/actuator)

These commands correspond to the implementation in [`boatlock/BleCommandHandler.h`](../boatlock/BleCommandHandler.h) and are used by the mobile application.

Manual control is intentionally a single atomic command instead of separate mode/direction/speed writes. Each `MANUAL_SET` refreshes the deadman TTL; if updates stop, firmware exits Manual mode and the normal quiet-output path stops motion. `MANUAL_SET` is a control command and must be wrapped in `SEC_CMD` when pairing/auth is enabled.

## Live State Frame

`34cd` notifications carry one 70-byte little-endian binary frame:

- bytes `0..1`: magic `BL`
- byte `2`: protocol version, currently `2`
- byte `3`: frame type, `1` for live telemetry
- bytes `4..5`: sequence `uint16`
- bytes `6..7`: flags `uint16`
- byte `8`: mode code (`0=IDLE`, `1=HOLD`, `2=ANCHOR`, `3=MANUAL`, `4=SIM`)
- byte `9`: status code (`0=OK`, `1=WARN`, `2=ALERT`)
- bytes `10..25`: `lat`, `lon`, `anchorLat`, `anchorLon` as signed `deg * 1e7`
- bytes `26..31`: `anchorHead`, `distance`, `heading` scaled as `deg*10`, `cm`, `deg*10`
- byte `32`: battery percent
- bytes `33..38`: `stepSpr`, `stepMaxSpd`, `stepAccel`
- bytes `39..55`: compass heading/raw/offset/quality/motion fields
- byte `56`: security reject code
- bytes `57..60`: status reason bitmask
- bytes `61..68`: security nonce `uint64`
- byte `69`: GNSS quality (`0` none, `1` weak, `2` good)

Flag bits:

- bit `0`: `holdHeading`
- bit `1`: `secPaired`
- bit `2`: `secAuth`
- bit `3`: `secPairWin`

Reason flags are decoded by the Flutter app into `statusReasons` strings such as `NO_GPS`, `NO_COMPASS`, `DRIFT_FAIL`, `CONTAINMENT_BREACH`, `COMM_TIMEOUT`, and GNSS gate reasons like `GPS_DATA_STALE`, `GPS_HDOP_MISSING`, and `GPS_POSITION_JUMP`. The firmware encoder is `boatlock/RuntimeBleLiveFrame.h`; the Flutter decoder is `boatlock_ui/lib/ble/ble_live_frame.dart`.

## Security Envelope

When `secPaired=1`, control/write commands are accepted only through `SEC_CMD`.

- Pairing is armed only by physical action: hold hardware `STOP` button for `3 s` (opens pairing window for `120 s`).
- During pairing window, app sends `PAIR_SET:<ownerSecretHex>`.
- Owner authentication flow:
  1. `AUTH_HELLO`
  2. receive `secNonce` in the next live frame or `SNAPSHOT`
  3. `AUTH_PROVE:<proofHex>`
- After successful auth (`secAuth=1`), send control commands wrapped as:
  - `SEC_CMD:<counterHex>:<sigHex>:<payload>`
  - counter must strictly increase (anti-replay)
  - command rate is limited
- `PAIR_CLEAR` is accepted only while the pairing window is open or from an authenticated owner session.

## Status / Diagnostics

Firmware status contract is now split:

- `status`: short health summary
  - `OK`
  - `WARN`
  - `ALERT`
- `statusReasons`: comma-separated detail flags when present, such as:
  - `NO_GPS`
  - `NO_COMPASS`
  - `DRIFT_ALERT`
  - `DRIFT_FAIL`
  - `CONTAINMENT_BREACH`
  - GNSS gate reasons like `GPS_HDOP_MISSING`, `GPS_HDOP_TOO_HIGH`, `GPS_DATA_STALE`, `GPS_POSITION_JUMP`
  - current `failsafeReason`
  - active short-lived safety banner reasons like `NUDGE_OK`

Firmware `mode` is currently one of:

- `IDLE`
- `HOLD`
- `ANCHOR`
- `MANUAL`
- `SIM`

Telemetry outside the 70-byte live frame should be added as an explicit new frame type or event, not as ad-hoc JSON on the live characteristic.
