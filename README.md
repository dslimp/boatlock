# Boatlock

This repository contains firmware for an ESP32-S3 based lock and a Flutter application used to control it.

## Device Functions

Boatlock combines the ESP32-S3 firmware with a companion mobile app to automate
anchoring and thruster control. Major capabilities include:

- saving the current location as an anchor point with heading
- automatically holding position or following an uploaded route
- manual rudder and motor control from the app
- compass calibration and heading emulation

The app and firmware communicate through a simple text protocol; see
[docs/BLE_PROTOCOL.md](docs/BLE_PROTOCOL.md) for the full command list.

## Photos

![Boatlock device](https://via.placeholder.com/400x300?text=Boatlock+device)

*Replace the placeholder above with a photo of the assembled hardware.*

## Connection Diagram

![Connection diagram](https://via.placeholder.com/600x400?text=Wiring+diagram)

*Replace the placeholder above with the actual wiring diagram.*

## Typical Scenarios

### Anchoring
1. Position the boat and select **Set Anchor** in the app.
2. The firmware stores the coordinates and current heading.

### Holding Position
1. Enable heading hold in the app.
2. The rudder and thruster adjust automatically to keep the boat near the saved point.

### Return to Point
1. Upload a route containing the desired waypoint and start it.
2. The boat navigates back to the specified location.

## Building the Firmware (Arduino IDE)

The firmware is now intended to be built with the Arduino IDE.

### Requirements
- Arduino IDE 2.x
- ESP32 board support package (Espressif Systems)
- An ESP32‑S3 development board

### Build and Upload
1. Open the Arduino IDE and install the **esp32** board package (Espressif Systems).
2. Create a new sketch and copy the contents of `boatlock/src/main.cpp` into it.
3. Add the headers from `boatlock/include` to the sketch folder.
4. Select your ESP32‑S3 board and the correct serial port.
5. Build and upload from the Arduino IDE toolbar.

### ESP32-S3-Touch-LCD-2 (Arduino migration)

The firmware now includes Arduino-side hardware checks for camera, IMU, and display
initialization. The BLE status string will include error tags such as `NO_CAMERA`,
`NO_IMU`, or `NO_DISPLAY` if any device fails to initialize. Adjust camera pin
definitions in `boatlock/include/HardwareConfig.h` if your wiring differs.

## Running the Flutter App

The `boatlock_ui` directory contains a Flutter project.

### Requirements
- [Flutter](https://flutter.dev/) SDK version 3.8 or newer
- A connected device or emulator (Android, iOS, or desktop)

### Run
```bash
cd boatlock_ui
flutter pub get
flutter run
```
Use `flutter build <platform>` to create release builds.

## Required Tools and Dependencies

| Component        | Purpose                                |
|------------------|----------------------------------------|
| Arduino IDE      | Building and flashing the ESP32 firmware |
| Flutter SDK      | Running the cross‑platform UI           |
| Git              | Cloning and updating this repository    |

Ensure these tools are installed and available in your `PATH` before attempting to build or run the project.

## Quick Start: ReadyToSky NEO‑M8N GPS Module

The firmware is preconfigured for the ReadyToSky GPS+compass board based on the
NEO‑M8N receiver. Connect the module to the ESP32‑S3 board as follows:

- **GPS TX** → **GPIO17**
- **GPS RX** → **GPIO18**
- **SDA** → **GPIO8**
- **SCL** → **GPIO9**
- **VCC** → 5 V (or 3.3 V if your module supports it)
- **GND** → **GND**

After wiring, build and flash the firmware from the Arduino IDE, then open the
Arduino serial monitor to verify that GPS data is being received.

## Compass Calibration

Run the BLE command `CALIB_COMPASS` or press the BOOT button while the device is
running to start calibration. Slowly rotate the device in all directions for
about 10 seconds. The calculated offsets and scale factors are saved to EEPROM
and automatically reloaded on startup.

See [CHANGELOG.md](CHANGELOG.md) for recent changes and firmware versions.

## HC 160A S2 Motor Controller

The HC 160A S2 driver requires two direction pins. Define them in
`boatlock/src/main.cpp`:

```cpp
#define MOTOR_DIR_PIN1 6   // IN1 on the driver
#define MOTOR_DIR_PIN2 10  // IN2 on the driver
```

During setup the firmware calls:

```cpp
motor.setDirPins(MOTOR_DIR_PIN1, MOTOR_DIR_PIN2);
```

Pin 1 should be HIGH and Pin 2 LOW for forward rotation. The logic is reversed
for reverse rotation. Adjust the GPIO numbers if you wire the controller to
different pins on the ESP32 board.

## Manual Thruster Control

The mobile app can directly control the rudder and motor speed.
Tap **Ручной режим** on the main screen to override automatic modes.
Hold the left or right rotate buttons to steer and move the speed
slider to drive the motor forward or reverse. See [docs/MANUAL_CONTROL.md](docs/MANUAL_CONTROL.md)
for a short how-to.

## License

This project is licensed under the terms of the [MIT License](LICENSE).
