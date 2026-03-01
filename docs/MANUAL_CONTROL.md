# Manual Control Mode

The firmware and Flutter app support steering and throttle control directly from the main screen.

## Enabling Manual Mode
1. Start the Flutter app and connect to the device via BLE.
2. On the map screen, tap the **Ручной режим** button to toggle manual mode.
3. When enabled, any automatic rudder movement is cancelled and automatic anchor
   holding is paused.

## Steering the Boat
- Hold the **left** or **right** rotation buttons to turn the rudder.
- Release the button to stop at the current position.
- To calibrate a 28BYJ+ULN2003 pointer, rotate it until it looks at the boat bow,
  then press **Сохранить "нос лодки"**. The app sends `SET_STEPPER_BOW`.
- Stepper geometry is fixed to 28BYJ (`4096` steps/rev).

## Adjusting Speed
- Move the slider to set the motor speed from -255 to 255.
- Positive values rotate forward, negative values reverse.

Tap the manual mode button again to return to automatic control.
