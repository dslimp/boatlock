# Manual Control Mode

The firmware and Flutter app support steering and throttle control directly from the main screen.

## Enabling Manual Mode
1. Start the Flutter app and connect to the device via BLE.
2. On the map screen, tap the **Ручной режим** button to toggle manual mode.
3. While enabled, automatic path following and anchor holding are paused.

## Steering the Boat
- Hold the **left** or **right** rotation buttons to turn the rudder.
- Release the button to stop at the current position.

## Adjusting Speed
- Move the slider to set the motor speed from -255 to 255.
- Positive values rotate forward, negative values reverse.

Tap the manual mode button again to return to automatic control.
