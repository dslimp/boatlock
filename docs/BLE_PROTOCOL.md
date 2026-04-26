# BLE Protocol

BoatLock uses one custom GATT service for the app and bench tooling:

- Service UUID `12ab`
- Live state characteristic `34cd`: notify/read, compact binary v3 frame
- Control point characteristic `56ef`: write/write-no-response, printable ASCII commands
- Device log characteristic `78ab`: notify, text log lines
- Firmware OTA characteristic `9abc`: binary write/write-no-response; accepts firmware chunks only after `OTA_BEGIN`

The live path is not a serial/JSON tunnel. The app subscribes to `34cd`, sends `STREAM_START`, and receives fixed-size binary live frames. Large snapshots and legacy parameter reads are not part of the protocol.

## Control Point Commands

Control point payloads are application-level command byte strings, not a text stream. Valid command writes are `1..191` printable ASCII bytes (`0x20..0x7E`). Clients must reject empty, overlong, control-byte, embedded-NUL, and non-ASCII commands before writing `56ef`; firmware rejects the same malformed payloads before queueing.

Command scopes are product boundaries, not wire-level security:

- `release`: normal water UI, app security/session setup, and app telemetry transport.
- `service`: installer, calibration, firmware update, and tuning operations. The normal Flutter app hides these controls unless built with `--dart-define=BOATLOCK_SERVICE_UI=true`; test/service harnesses must opt in explicitly.
- `dev/HIL`: injected sensor data and on-device simulation. These commands are for validation harnesses only and must not appear in the normal water UI. Flutter custom-command callers must opt in explicitly or use `--dart-define=BOATLOCK_DEV_HIL_COMMANDS=true`.

`SEC_CMD` is the security envelope; the effective scope is the scope of the wrapped payload. The Flutter custom-command classifier does not accept raw `SEC_CMD` input; the BLE transport builds the envelope internally after classifying the unwrapped command.

Current classification:

- `release`: `STREAM_START`, `STREAM_STOP`, `SNAPSHOT`, `SET_ANCHOR`, `ANCHOR_ON`, `ANCHOR_OFF`, `STOP`, `HEARTBEAT`, `MANUAL_SET`, `MANUAL_OFF`, `NUDGE_DIR`, `NUDGE_BRG`, `SET_HOLD_HEADING`, `PAIR_SET`, `PAIR_CLEAR`, `AUTH_HELLO`, `AUTH_PROVE`.
- `service`: `SET_ANCHOR_PROFILE`, `SET_COMPASS_OFFSET`, `RESET_COMPASS_OFFSET`, `COMPASS_CAL_START`, `COMPASS_DCD_SAVE`, `COMPASS_DCD_AUTOSAVE_ON`, `COMPASS_DCD_AUTOSAVE_OFF`, `COMPASS_TARE_Z`, `COMPASS_TARE_SAVE`, `COMPASS_TARE_CLEAR`, `SET_STEP_MAXSPD`, `SET_STEP_ACCEL`, `SET_STEPPER_BOW`, `OTA_BEGIN`, `OTA_FINISH`, `OTA_ABORT`.
- `dev/HIL`: `SET_PHONE_GPS`, `SIM_LIST`, `SIM_RUN`, `SIM_STATUS`, `SIM_REPORT`, `SIM_ABORT`.

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
| `SET_ANCHOR_PROFILE:<quiet\|normal\|current>` | profile id | Apply and persist preset `HoldRadius`, `DeadbandM`, `MaxThrustA`, and `ThrRampA` values |
| `PAIR_SET:<ownerSecretHex>` | 32-hex owner secret | Set/replace owner secret while pairing window is open from hardware STOP long-press |
| `PAIR_CLEAR` | none | Clear pairing; accepted only from owner session or while pairing window is physically open |
| `AUTH_HELLO` | none | Start owner auth challenge; firmware generates `secNonce` |
| `AUTH_PROVE:<proofHex>` | 16-hex chars | Complete owner auth for current nonce using owner secret MAC |
| `SEC_CMD:<counterHex>:<sigHex>:<payload>` | secured command envelope | Authenticated command with anti-replay counter and 64-bit MAC |
| `SET_HOLD_HEADING:<0|1>` | integer flag | Enable (`1`) or disable (`0`) heading hold |
| `SET_PHONE_GPS:<lat>,<lon>[,<speedKmh>[,<satellites>]]` | latitude, longitude, optional speed in km/h, optional satellites used in fix | External tooling command; the main app control path does not use phone GPS as an anchor-control source |
| `SET_COMPASS_OFFSET:<deg>` | heading offset in degrees (float) | Set persistent yaw offset for onboard BNO08x heading |
| `RESET_COMPASS_OFFSET` | none | Reset persistent BNO08x heading offset to `0` |
| `COMPASS_CAL_START` | none | SH2-UART build only: enable BNO08x dynamic calibration for accel/gyro/mag/planar sensors with DCD autosave disabled |
| `COMPASS_DCD_SAVE` | none | SH2-UART build only: request immediate BNO08x Dynamic Calibration Data save |
| `COMPASS_DCD_AUTOSAVE_ON` | none | SH2-UART build only: enable BNO08x automatic DCD save |
| `COMPASS_DCD_AUTOSAVE_OFF` | none | SH2-UART build only: disable BNO08x automatic DCD save |
| `COMPASS_TARE_Z` | none | SH2-UART build only: tare Z/heading basis from the current rotation vector; use only during controlled calibration |
| `COMPASS_TARE_SAVE` | none | SH2-UART build only: persist the last tare operation |
| `COMPASS_TARE_CLEAR` | none | SH2-UART build only: clear the persisted tare |
| `SET_STEP_MAXSPD:<float>` | float | Maximum stepper speed (steps/s) |
| `SET_STEP_ACCEL:<float>` | float | Stepper acceleration (steps/s²) |
| `SET_STEPPER_BOW` | none | Save current stepper position as boat-bow zero for anchor pointing |
| `OTA_BEGIN:<size>,<sha256>` | firmware size in bytes and 64-hex SHA-256 | Enter safe stopped state, start writing a new firmware image to the inactive OTA partition, and arm binary chunk writes on `9abc` |
| `OTA_FINISH` | none | Validate byte count and SHA-256, finalize the OTA image, then reboot into the new partition |
| `OTA_ABORT` | none | Abort an active BLE OTA transfer and keep the current firmware active |
| `SIM_LIST` | none | List built-in on-device HIL scenarios (`S0..S19`) |
| `SIM_RUN:<scenario_id>[,<speedup>]` | id + speed mode (`0` fastest, `1` realtime) | Start deterministic closed-loop simulation on device |
| `SIM_STATUS` | none | Return current simulation progress JSON |
| `SIM_REPORT` | none | Return final simulation report JSON (chunked in logs) |
| `SIM_ABORT` | none | Abort currently running simulation |

Current built-in HIL groups:
- `S0..S3`: baseline hold/current/gust behavior
- `S4..S9`: GNSS/control-loop/NaN safety regressions
- `S10..S19`: randomized + hardware-failure emulation (compass/power/display/actuator)

These commands correspond to the implementation in [`boatlock/BleCommandHandler.h`](../boatlock/BleCommandHandler.h). The normal mobile application exposes the release subset; service and dev/HIL commands are accepted only through explicit service/test paths on the app side.

Manual control is intentionally a single atomic command instead of separate mode/direction/speed writes. Each accepted `MANUAL_SET` refreshes the deadman TTL for the current controller source; a different source cannot take over until the lease expires or `MANUAL_OFF`/`STOP` clears it. If updates stop, firmware exits Manual mode and the normal quiet-output path stops motion. `MANUAL_SET` is a control command and must be wrapped in `SEC_CMD` when pairing/auth is enabled.

## Multi-Client Control Ownership

This section is a design contract for future phone-plus-remote operation. It is
not a claim that simultaneous control is implemented today.

BLE may keep advertising while connected and may allow multiple centrals to
subscribe to telemetry. Multiple clients do not imply multiple controllers.
Firmware must keep an explicit controller identity per connected central and an
explicit single control-owner lease before any future BLE remote is accepted as
a second controller.

Controller identity is server-owned. It must be derived from connection-local
BLE metadata such as connection handle, sanitized peer address when available,
and a monotonic connection generation. Command payloads must not let clients
choose or spoof the controller id. Reconnect creates a new identity; any previous
auth and lease state is invalid for that connection.

Allowed read-only clients:

- subscribe to `34cd` live telemetry
- subscribe to `78ab` logs
- send transport-only `STREAM_START`, `STREAM_STOP`, and `SNAPSHOT`
- do not hold a control lease
- cannot refresh control heartbeat or manual TTL for another client
- cannot send actuation, mode, settings, pairing-clear, OTA, `SIM_*`, or
  injected-sensor commands

Exactly one control owner is allowed. The lease records controller identity,
client role (`app` or `remote`), authenticated session identity when paired,
acquire time, last refresh time, and expiry. The lease is acquired only by an
eligible control command when no live owner exists. Owner commands and owner
`HEARTBEAT` refresh it. Commands from any other client while the lease is live
are rejected before parsing can mutate settings, modes, security state, OTA
state, simulation state, injected sensor state, or outputs.

Takeover rules:

- same owner may continue sending allowed commands while the lease is live
- non-owner control commands are rejected while the owner lease is live
- a new owner may acquire only after owner lease expiry, owner disconnect cleanup,
  or emergency `STOP`
- owner disconnect during Manual or Anchor is a control-link loss and must quiet
  outputs or latch the existing failsafe path before any other connected client
  can acquire control
- a telemetry-only client never becomes owner merely because the current owner
  disconnected

Security is per client. `AUTH_HELLO`, `secNonce`, `AUTH_PROVE`, secure-command
counter, replay window, and `secAuth` must be scoped to the controller identity.
When `secPaired=1`, every control/write command from every client must be inside
that client's valid `SEC_CMD`; one authenticated app session cannot authorize a
remote session. `PAIR_CLEAR` remains accepted only from an owner session or while
the hardware STOP pairing window is physically open.

Phone app and remote constraints are intentionally different. The phone app is
the full product controller for anchor save/enable/disable, jog, hold-heading,
manual, STOP, and service flows in service builds. A future BLE remote starts
with a narrower allowlist: `MANUAL_SET`, `MANUAL_OFF`, `ANCHOR_OFF`,
`HEARTBEAT`, and `STOP`. It must not save or jog anchor points, change security,
change settings, start OTA, run HIL, inject sensors, or use service/dev commands
unless those surfaces are explicitly added to the remote role and covered by the
same ownership and security tests.

Manual control is nested under the control-owner lease. The first accepted
`MANUAL_SET` may acquire control and enter Manual atomically when no owner
exists. After that, only the control owner can refresh the manual deadman TTL or
send `MANUAL_OFF`. Manual TTL expiry stops Manual output but does not grant
takeover. Control lease expiry, owner disconnect, or `STOP` clears the manual
lease.

STOP priority:

- physical STOP always wins, opens the pairing long-press path when held, and
  clears control ownership, Manual, Anchor, and outputs
- BLE `STOP` is accepted from any authenticated client even if it is not the
  current control owner
- when paired, BLE `STOP` still goes through `SEC_CMD`; raw BLE `STOP` is not an
  auth bypass

Required validation before implementation is accepted:

- unit tests for controller identity creation, reconnect invalidation, lease
  acquire/refresh/expiry, busy rejection, and disconnect cleanup
- security tests proving per-client nonce/counter/auth isolation and rejection
  of secondary-client secure commands
- command-scope tests proving non-owner commands cannot mutate modes, settings,
  pairing, OTA, simulation, injected sensors, or outputs
- manual tests for same-owner refresh, non-owner rejection, TTL quieting,
  `MANUAL_OFF`, disconnect, and reconnect
- role tests proving the remote allowlist is narrower than the app allowlist
- STOP tests proving hardware STOP and authenticated non-owner BLE STOP preempt a
  live owner
- two-central bench or Android smoke with simultaneous telemetry subscribers and
  one owner, including observable busy rejection for the loser

## Firmware Scope Gate

Firmware builds enforce the command scope selected by the PlatformIO profile. The default `esp32s3` environment is release-compatible; explicit `esp32s3_release`, `esp32s3_service`, and `esp32s3_acceptance` environments select the same release, service, and acceptance command profiles used by the `nh02` flash wrapper.

Profile rules:

- `release`: normal water image. It accepts only `release` commands plus the security envelope around allowed payloads. It rejects `service` and `dev/HIL` commands before they can mutate settings, start OTA, inject sensor data, start simulation, or actuate outputs.
- `service`: maintenance image. It accepts `release` plus `service` commands, including BLE OTA, calibration, tuning, and installer flows. It rejects `dev/HIL`, including `SET_PHONE_GPS` and all `SIM_*` commands.
- `acceptance`: bench validation image. It accepts known `release`, `service`, and `dev/HIL` commands so `SIM_LIST`, `SIM_RUN`, `SIM_STATUS`, `SIM_REPORT`, and `SIM_ABORT` remain available to the on-device HIL and Android smoke wrappers. Unknown commands remain rejected in every profile.

`esp32s3_bno08x_sh2_uart` and `esp32s3_debug_wifi_ota` extend the service profile. Debug probe builds are not command-profile release artifacts.

Gate behavior expectations:

- Scope is evaluated on the unwrapped command payload. `SEC_CMD` must not become a bypass; the effective scope is the wrapped payload scope.
- Rejected commands fail closed: no setting write, no OTA session, no simulation state change, no injected GNSS update, no motor or stepper output, and no transition out of the current safe runtime mode.
- Rejections are observable in the log as `[BLE] command rejected reason=profile profile=<release|service|acceptance> scope=<service|dev_hil|unknown> command=<payload>` so wrappers can distinguish "command gated by profile" from "command parser bug" or "BLE link lost".
- `OTA_BEGIN`, `OTA_FINISH`, and `OTA_ABORT` remain `service` commands. A release image that excludes service commands must document that BLE OTA is unavailable from that image, and the service update path must remain recoverable through USB seed flash.
- `SIM_*` and `SET_PHONE_GPS` remain `dev/HIL` commands. They must be absent from normal water firmware unless the explicit acceptance profile is flashed.
- During active OTA, the existing OTA safety rule still applies inside any profile that allows OTA: firmware rejects runtime commands except `HEARTBEAT`, `OTA_FINISH`, and `OTA_ABORT`.

## BLE Firmware OTA

BLE OTA uses the phone as the bridge:

1. The app downloads `firmware.bin` and verifies it against an operator-provided SHA-256.
2. The app sends `OTA_BEGIN:<size>,<sha256>` over `56ef`. When `secPaired=1`, this command must be wrapped in `SEC_CMD`.
3. Firmware disables Anchor, stops Manual and all outputs, latches `HOLD`, and logs `[OTA] begin ok ...`.
4. The Android app requests high connection priority and a larger MTU for the transfer, then writes firmware bytes sequentially to `9abc` in chunks up to `MTU - 3`. It uses write-without-response when the OTA characteristic supports it, with an explicit pacing window/backpressure; otherwise it falls back to acknowledged writes. The app emits `BOATLOCK_OTA_PROGRESS` logcat lines while updating the Settings progress view.
5. The app sends `OTA_FINISH` over `56ef`. Firmware checks received byte count and SHA-256 before finalizing the OTA image.
6. On success firmware logs `[OTA] finish ok ...`, waits briefly so the phone can receive the log ACK, and restarts.

During active OTA, firmware rejects all runtime commands except `HEARTBEAT`, `OTA_FINISH`, and `OTA_ABORT`. Chunk writes before a successful `OTA_BEGIN` are ignored. If the BLE link disconnects during an active transfer, firmware aborts the update and keeps the current boot partition.

The current phone bridge intentionally uses acknowledged BLE writes for the first reliable acceptance path. This is slow on the bench, roughly sub-1 KB/s, so a 700 KB firmware image can take around 15 minutes; throughput optimization should be a separate change that preserves abort/retry safety.

## Live State Frame

`34cd` notifications carry exactly one 72-byte little-endian binary frame. Receivers reject shorter or longer payloads, wrong magic/version/type, and unknown enum codes rather than treating the live path as a padded or forward-compatible stream:

- bytes `0..1`: magic `BL`
- byte `2`: protocol version, currently `3`
- byte `3`: frame type, `1` for live telemetry
- bytes `4..5`: sequence `uint16`
- bytes `6..7`: flags `uint16`
- byte `8`: mode code (`0=IDLE`, `1=HOLD`, `2=ANCHOR`, `3=MANUAL`, `4=SIM`)
- byte `9`: status code (`0=OK`, `1=WARN`, `2=ALERT`)
- bytes `10..25`: `lat`, `lon`, `anchorLat`, `anchorLon` as signed `deg * 1e7`
- bytes `26..33`: `anchorHead`, `distance`, `anchorBearing`, `heading` scaled as `deg*10`, `cm`, `deg*10`, `deg*10`
- byte `34`: battery percent
- bytes `35..40`: `stepSpr`, `stepMaxSpd`, `stepAccel`
- bytes `41..57`: compass heading/raw/offset/quality/motion fields
- byte `58`: security reject code
- bytes `59..62`: status reason bitmask
- bytes `63..70`: security nonce `uint64`
- byte `71`: GNSS quality (`0` none, `1` weak, `2` good)

`distance` and `anchorBearing` are display telemetry from the currently published position to the saved anchor point. They can be populated from the same UI-visible fix as `lat/lon`; they are not proof that the hardware GNSS quality gate is satisfied for anchor control.

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

Telemetry outside the 72-byte live frame should be added as an explicit new frame type or event, not as ad-hoc JSON on the live characteristic.
