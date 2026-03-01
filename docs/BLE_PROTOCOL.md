# BLE Protocol

The firmware receives ASCII commands over its BLE characteristic. The Flutter app in `boatlock_ui` uses the same messages. Each command is sent as a text line without a terminating newline.

| Command | Parameters | Description |
|--------|------------|-------------|
| `SET_ANCHOR:<lat>,<lon>` | `lat` and `lon` decimal degrees (floats) | Save anchor position with current heading |
| `ANCHOR_ON` | none | Enable anchor mode only if a saved anchor point exists and GNSS quality is sufficient |
| `ANCHOR_OFF` | none | Disable anchor mode immediately (always accepted) |
| `STOP` | none | Emergency stop: disable anchor/manual control and stop all motors (highest priority) |
| `HEARTBEAT` | none | Keep-alive command from controller/app; missing heartbeat while Anchor is active triggers failsafe |
| `NUDGE_DIR:<FWD\|BACK\|LEFT\|RIGHT>,<meters>` | direction + distance (1.0..5.0 m) | Shift anchor point in boat frame (allowed only while Anchor is active and safety checks pass) |
| `NUDGE_BRG:<bearingDeg>,<meters>` | absolute bearing + distance (1.0..5.0 m) | Shift anchor point by bearing/distance (allowed only while Anchor is active and safety checks pass) |
| `SET_ANCHOR_PROFILE:<quiet\|normal\|current>` | profile id | Atomically apply preset Anchor control params and persist them |
| `PAIR_SET:<6-digit-code>` | decimal owner code | Set/replace owner code (allowed only while pairing window is open from hardware STOP long-press) |
| `PAIR_CLEAR` | none | Clear pairing (drop owner code and active session) |
| `AUTH_HELLO` | none | Start owner auth challenge; firmware generates `secNonce` |
| `AUTH_PROVE:<proofHex>` | 8-hex chars | Complete owner auth for current nonce |
| `SEC_CMD:<counterHex>:<sigHex>:<payload>` | secured command envelope | Authenticated command with anti-replay counter and signature |
| `SET_HOLD_HEADING:<0|1>` | integer flag | Enable (`1`) or disable (`0`) heading hold |
| `SET_PHONE_GPS:<lat>,<lon>[,<speedKmh>[,<satellites>]]` | latitude, longitude, optional speed in km/h, optional satellites used in fix | Provide phone GPS fix (used as fallback when onboard GPS has no fresh fix) |
| `SET_COMPASS_OFFSET:<deg>` | heading offset in degrees (float) | Set persistent yaw offset for onboard BNO08x heading |
| `RESET_COMPASS_OFFSET` | none | Reset persistent BNO08x heading offset to `0` |
| `SET_STEP_SPR:<int>` | integer | Compatibility command; firmware is fixed to 28BYJ value `4096` |
| `SET_STEP_MAXSPD:<float>` | float | Maximum stepper speed (steps/s) |
| `SET_STEP_ACCEL:<float>` | float | Stepper acceleration (steps/s²) |
| `SET_STEPPER_BOW` | none | Save current stepper position as boat-bow zero for anchor pointing |
| `MANUAL:<0|1>` | integer flag | Toggle manual control mode |
| `MANUAL_DIR:<0|1>` | integer | Rudder direction: `0` = port, `1` = starboard |
| `MANUAL_SPEED:<int>` | integer | Thruster power (-255..255) |

These commands correspond to the implementation in [`boatlock/BleCommandHandler.h`](../boatlock/BleCommandHandler.h) and are used by the mobile application.

## Security Envelope

When `secPaired=1`, control/write commands are accepted only through `SEC_CMD`.

- Pairing is armed only by physical action: hold hardware `STOP` button for `3 s` (opens pairing window for `120 s`).
- During pairing window, app sends `PAIR_SET:<6-digit-code>`.
- Owner authentication flow:
  1. `AUTH_HELLO`
  2. read `secNonce`
  3. `AUTH_PROVE:<proofHex>`
- After successful auth (`secAuth=1`), send control commands wrapped as:
  - `SEC_CMD:<counterHex>:<sigHex>:<payload>`
  - counter must strictly increase (anti-replay)
  - command rate is limited

## Status / Diagnostics

Firmware `status` field can include additional comma-separated reasons:

- `NO_GPS`
- `GNSS_WEAK`
- `NO_COMPASS`
- `DRIFT_ALERT`
- `DRIFT_FAIL`
- `GPS_WEAK`
- `FAILSAFE_LINK_LOST`
- `FAILSAFE_HEARTBEAT_TIMEOUT`

Additional telemetry params:

- `distance` (m)
- `driftSpeed` (m/s)
- `driftAlert` (`0|1`)
- `driftFail` (`0|1`)
- `gnssQ` (`0` = no fix, `1` = weak, `2` = good)
- `anchorProfile` (`0=quiet`, `1=normal`, `2=current`)
- `anchorProfileName` (`quiet|normal|current`)
- `anchorDeniedReason` (`NONE|NO_ANCHOR_POINT|GPS_*|INTERNAL_ERROR`)
- `failsafeReason` (`NONE|STOP_CMD|GPS_WEAK|COMM_TIMEOUT|CONTROL_LOOP_TIMEOUT|SENSOR_TIMEOUT|INTERNAL_ERROR_NAN|COMMAND_OUT_OF_RANGE`)
- `secPaired` (`0|1`)
- `secAuth` (`0|1`)
- `secPairWin` (`0|1`)
- `secNonce` (challenge nonce for `AUTH_PROVE`)
- `secReject` (last security rejection reason)
