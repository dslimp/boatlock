# Boatlock

This repository contains firmware for an ESP32-S3 based lock and a Flutter application used to control it.

## Building the Firmware

The `boatlock` directory is a [PlatformIO](https://platformio.org/) project.

### Requirements
- PlatformIO CLI (or the PlatformIO extension for VS Code)
- Python (installed automatically with PlatformIO)
- An ESP32‑S3 development board

### Build and Upload
```bash
cd boatlock
platformio run              # build the firmware
platformio run --target upload  # flash to the board
platformio device monitor   # optional: view serial output
```

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
| PlatformIO       | Building and flashing the ESP32 firmware |
| Python           | Required by PlatformIO                  |
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

After wiring, build and flash the firmware:

```bash
cd boatlock
platformio run
platformio run --target upload
```

Open the serial monitor (`platformio device monitor`) to verify that GPS data is
being received.

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
