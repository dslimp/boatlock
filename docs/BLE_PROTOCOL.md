# BLE Protocol

The firmware receives ASCII commands over its BLE characteristic. The Flutter app in `boatlock_ui` uses the same messages. Each command is sent as a text line without a terminating newline.

| Command | Parameters | Description |
|--------|------------|-------------|
| `SET_ANCHOR:<lat>,<lon>` | `lat` and `lon` decimal degrees (floats) | Save anchor position with current heading |
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
