# BLE Protocol

BoatLock uses one custom GATT service for the app and bench tooling:

- Service UUID `12ab`
- Live state characteristic `34cd`: notify/read, compact binary v5 frame
- Control point characteristic `56ef`: write/write-no-response, printable ASCII commands
- Device log characteristic `78ab`: notify, text log lines
- Firmware OTA characteristic `9abc`: binary writes accepted only after `OTA_BEGIN`

The live path is not a serial/JSON tunnel. The app subscribes to `34cd`, sends
`STREAM_START`, and receives fixed-size binary live frames. Large snapshots and
parameter reads are not part of the protocol.

## Control Point Commands

Control point payloads are application-level command byte strings, not a text
stream. Valid command writes are `1..191` printable ASCII bytes
(`0x20..0x7E`). Clients reject empty, overlong, control-byte, embedded-NUL, and
non-ASCII commands before writing `56ef`; firmware rejects the same malformed
payloads before queueing.

There is no BoatLock BLE command wrapper in the release protocol. The app writes
the command listed below directly to `56ef`; firmware safety gates remain
authoritative for motion, Anchor enable, Manual deadman, OTA integrity, and
profile command scope.

Current classification:

- `release`: `STREAM_START`, `STREAM_STOP`, `SNAPSHOT`, `SET_ANCHOR`,
  `ANCHOR_ON`, `ANCHOR_OFF`, `STOP`, `HEARTBEAT`, `MANUAL_TARGET`,
  `MANUAL_OFF`, `NUDGE_DIR`, `NUDGE_BRG`, `SET_HOLD_HEADING`,
  `SET_ANCHOR_PROFILE`, `SET_COMPASS_OFFSET`, `RESET_COMPASS_OFFSET`,
  `COMPASS_CAL_START`, `COMPASS_DCD_SAVE`, `COMPASS_DCD_AUTOSAVE_ON`,
  `COMPASS_DCD_AUTOSAVE_OFF`, `COMPASS_TARE_Z`, `COMPASS_TARE_SAVE`,
  `COMPASS_TARE_CLEAR`, `SET_STEP_MAXSPD`, `SET_STEP_ACCEL`, `SET_STEP_SPR`,
  `SET_STEP_GEAR`, `SET_STEPPER_BOW`, `OTA_BEGIN`, `OTA_FINISH`, `OTA_ABORT`,
  `SIM_LIST`, `SIM_RUN`, `SIM_STATUS`, `SIM_REPORT`, `SIM_ABORT`.
- `dev/HIL`: `SET_PHONE_GPS`.

| Command | Parameters | Description |
|--------|------------|-------------|
| `STREAM_START` | none | Enable periodic live-state notifications and emit one live frame |
| `STREAM_STOP` | none | Disable periodic live-state notifications and clear queued live frames |
| `SNAPSHOT` | none | Emit one live-state frame without changing stream state |
| `SET_ANCHOR:<lat>,<lon>` | valid non-zero decimal degrees | Save anchor position with current heading; does not enable Anchor mode |
| `ANCHOR_ON` | none | Enable Anchor only with a saved point, onboard heading, and sufficient GNSS quality |
| `ANCHOR_OFF` | none | Disable Anchor immediately, stop actuation, and clear latched `HOLD` state |
| `STOP` | none | Emergency stop: disable Anchor/Manual, stop all motors, and latch `HOLD` |
| `HEARTBEAT` | none | Keep-alive from the app; missing heartbeat while Anchor is active triggers failsafe |
| `MANUAL_TARGET:<angleDeg>,<throttlePct>,<ttlMs>` | `angleDeg=-90..90`, `throttlePct=-100..100`, `ttlMs=100..1000` | Atomically enter/refresh Manual; disables Anchor and acts as a deadman lease |
| `MANUAL_OFF` | none | Stop Manual mode and zero manual outputs |
| `NUDGE_DIR:<FWD\|BACK\|LEFT\|RIGHT>` | direction | Shift anchor point by the fixed jog step while Anchor is active and safety checks pass |
| `NUDGE_BRG:<bearingDeg>` | absolute bearing | Shift anchor point by the fixed jog step on a bearing while Anchor is active |
| `SET_ANCHOR_PROFILE:<quiet\|normal\|current>` | profile id | Apply and persist Anchor preset values |
| `SET_HOLD_HEADING:<0|1>` | integer flag | Enable or disable heading hold |
| `SET_PHONE_GPS:<lat>,<lon>[,<speedKmh>[,<satellites>]]` | external fix | Dev/HIL injection only; normal app control does not use phone GPS for anchor control |
| `SET_COMPASS_OFFSET:<deg>` | float degrees | Set persistent BNO08x heading offset |
| `RESET_COMPASS_OFFSET` | none | Reset persistent BNO08x heading offset to `0` |
| `COMPASS_CAL_START` | none | SH2-UART build only: enable BNO08x dynamic calibration |
| `COMPASS_DCD_SAVE` | none | SH2-UART build only: request BNO08x DCD save |
| `COMPASS_DCD_AUTOSAVE_ON` | none | SH2-UART build only: enable BNO08x automatic DCD save |
| `COMPASS_DCD_AUTOSAVE_OFF` | none | SH2-UART build only: disable BNO08x automatic DCD save |
| `COMPASS_TARE_Z` | none | SH2-UART build only: tare Z/heading basis |
| `COMPASS_TARE_SAVE` | none | SH2-UART build only: persist last tare |
| `COMPASS_TARE_CLEAR` | none | SH2-UART build only: clear persisted tare |
| `SET_STEP_MAXSPD:<float>` | steps/s | Maximum stepper speed |
| `SET_STEP_ACCEL:<float>` | steps/s² | Stepper acceleration |
| `SET_STEP_SPR:<int>` | steps/rev | Motor STEP pulses per motor revolution |
| `SET_STEP_GEAR:<float>` | ratio | Mechanical reduction from stepper motor to steering output shaft |
| `SET_STEPPER_BOW` | none | Save current stepper position as boat-bow zero |
| `OTA_BEGIN:<size>,<sha256>` | bytes + 64-hex SHA-256 | Stop outputs, arm inactive OTA partition, and accept binary chunks on `9abc` |
| `OTA_FINISH` | none | Validate byte count and SHA-256, finalize OTA image, then reboot |
| `OTA_ABORT` | none | Abort active BLE OTA and keep current firmware |
| `SIM_LIST` | none | List built-in HIL scenarios |
| `SIM_RUN:<scenario_id>[,<speedup>]` | id + speed mode | Start deterministic on-device simulation |
| `SIM_STATUS` | none | Return simulation progress JSON |
| `SIM_REPORT` | none | Return final simulation report JSON in logs |
| `SIM_ABORT` | none | Abort currently running simulation |

`SIM_*` is release-scope because simulation is a quiet software mode inside the
ordinary firmware. Real sensor values are ignored for control/display while the
scenario is active, actuator outputs are quieted, and live BLE telemetry carries
the simulated boat position/heading. `SET_PHONE_GPS` remains dev/HIL only.

Manual control is intentionally a single atomic command instead of separate
mode/direction/speed writes. Each accepted `MANUAL_TARGET` refreshes the
deadman TTL for the current controller source. If updates stop, firmware exits
Manual mode and stops output.

## Control Ownership

The current release path has one normal product controller: the phone app.
Multiple telemetry subscribers may be allowed by BLE transport, but a second
actuating controller must not be treated as implemented until it has explicit
controller identity, a single-owner lease, role allowlists, and bench tests.

The existing lease helper models that future work for Manual/Anchor ownership:
read-only transport commands do not acquire ownership, eligible app control
commands acquire/refresh a lease, non-owner control is busy until the lease
expires, and `STOP` preempts. It does not use a BLE command-wrapper/session
layer.

## Firmware Scope Gate

Firmware builds enforce the command scope selected by the PlatformIO profile.
The default `esp32s3` environment is the normal release firmware and includes
BLE OTA plus hardware setup commands. `esp32s3_acceptance` additionally accepts
dev/HIL sensor injection.

Gate behavior expectations:

- Unknown commands and disallowed profile commands fail closed before settings,
  modes, OTA, simulation, injected GNSS, motor, or stepper state can change.
- Rejections are logged as `[BLE] command rejected reason=profile ...`.
- `OTA_BEGIN`, `OTA_FINISH`, and `OTA_ABORT` are release commands. The normal
  firmware must remain OTA-capable so moved hardware can be updated through the
  phone BLE path.
- During active OTA, firmware rejects runtime commands except `HEARTBEAT`,
  `OTA_FINISH`, and `OTA_ABORT`.

## BLE Firmware OTA

BLE OTA uses the phone as the bridge:

1. The app either reads a phone-local `firmware.bin` and computes SHA-256, or
   downloads the latest GitHub Release firmware and verifies it against the
   release manifest SHA-256.
2. The app sends `OTA_BEGIN:<size>,<sha256>` over `56ef`.
3. Firmware disables Anchor, stops Manual and all outputs, latches `HOLD`, and
   logs `[OTA] begin ok ...`.
4. The Android app requests high connection priority and larger MTU, then writes
   firmware bytes to `9abc` in chunks up to `MTU - 3`.
5. The app sends `OTA_FINISH`; firmware checks received byte count and SHA-256.
6. On success firmware logs `[OTA] finish ok ...`, waits briefly, and restarts.

Chunk writes before a successful `OTA_BEGIN` are ignored. If the BLE link
disconnects during an active transfer, firmware aborts the update and keeps the
current boot partition.

The release app uses the latest GitHub Release for one-button updates. A release
should include `manifest.json` plus `firmware-esp32s3.bin`; the manifest must
describe a `release/vX.Y.x` branch `esp32s3` binary with
`commandProfile=release`, positive size, HTTPS binary URL, and a 64-hex
SHA-256.

## Live State Frame

`34cd` notifications carry exactly one 65-byte little-endian binary frame.
Receivers reject shorter or longer payloads, wrong magic/version/type, and
unknown enum codes.

- bytes `0..1`: magic `BL`
- byte `2`: protocol version, currently `5`
- byte `3`: frame type, `1` for live telemetry
- bytes `4..5`: sequence `uint16`
- bytes `6..7`: flags `uint16`
- byte `8`: mode code (`0=IDLE`, `1=HOLD`, `2=ANCHOR`, `3=MANUAL`, `4=SIM`)
- byte `9`: status code (`0=OK`, `1=WARN`, `2=ALERT`)
- bytes `10..25`: `lat`, `lon`, `anchorLat`, `anchorLon` as signed `deg * 1e7`
- bytes `26..33`: `anchorHead`, `distance`, `anchorBearing`, `heading`
- byte `34`: battery percent
- bytes `35..42`: `stepSpr`, `stepGear*10`, `stepMaxSpd`, `stepAccel`
- bytes `43..59`: compass heading/raw/offset/quality/motion fields
- bytes `60..63`: status reason bitmask
- byte `64`: GNSS quality (`0` none, `1` weak, `2` good)

Flag bits:

- bit `0`: `holdHeading`

Reason flags are decoded by the Flutter app into `statusReasons` strings such as
`NO_GPS`, `NO_COMPASS`, `DRIFT_FAIL`, `CONTAINMENT_BREACH`, `COMM_TIMEOUT`, and
GNSS gate reasons like `GPS_DATA_STALE`, `GPS_HDOP_MISSING`, and
`GPS_POSITION_JUMP`.

Firmware `mode` is currently one of `IDLE`, `HOLD`, `ANCHOR`, `MANUAL`, `SIM`.
Telemetry outside the live frame should be added as an explicit new frame type
or event, not as ad-hoc JSON on the live characteristic.
