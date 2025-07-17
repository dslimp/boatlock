# Manual Control Mode

The firmware and Flutter app support steering and throttle control directly from the main screen.

## Enabling Manual Mode
1. Start the Flutter app and connect to the device via BLE.
2. On the map screen, enable **Ручной режим** (Manual mode) by checking the box.
3. While enabled, automatic path following and anchor holding are paused.

## Steering the Boat
- Use the eight arrow buttons to turn the stepper-driven rudder in 45° increments.
- Press the stop icon to keep the current heading.

## Adjusting Speed
- Move the slider to set the motor speed from -255 to 255.
- Positive values rotate forward, negative values reverse.

Uncheck the manual mode box to return to automatic control.
