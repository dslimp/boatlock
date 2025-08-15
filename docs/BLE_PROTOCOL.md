# BLE Protocol

The firmware receives ASCII commands over its BLE characteristic. The Flutter app in `boatlock_ui` uses the same messages. Each command is sent as a text line without a terminating newline.

| Command | Parameters | Description |
|--------|------------|-------------|
| `SET_ANCHOR:<lat>,<lon>` | `lat` and `lon` decimal degrees (floats) | Save anchor position with current heading |
| `SET_HOLD_HEADING:<0|1>` | integer flag | Enable (`1`) or disable (`0`) heading hold |
| `SET_ROUTE:<lat1>,<lon1>;...` | semicolon separated latitude/longitude pairs | Load route waypoints |
| `START_ROUTE` | none | Begin following the uploaded route |
| `STOP_ROUTE` | none | Halt route following |
| `CALIB_COMPASS` | none | Start compass calibration |
| `SET_HEADING:<deg>` | heading in degrees (float) | Provide external heading when emulating the compass |
| `EMU_COMPASS:<0|1>` | integer flag | Enable (`1`) or disable (`0`) use of emulated heading |
| `SET_STEP_SPR:<int>` | integer | Stepper steps per revolution |
| `SET_STEP_MAXSPD:<float>` | float | Maximum stepper speed (steps/s) |
| `SET_STEP_ACCEL:<float>` | float | Stepper acceleration (steps/s²) |
| `MANUAL:<0|1>` | integer flag | Toggle manual control mode |
| `MANUAL_DIR:<0|1>` | integer | Rudder direction: `0` = port, `1` = starboard |
| `MANUAL_SPEED:<int>` | integer | Thruster power (0–100) |

These commands correspond to the implementation in [`boatlock/include/BleCommandHandler.h`](../boatlock/include/BleCommandHandler.h) and are used by the mobile application.
