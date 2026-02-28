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

## Building the Firmware

The `boatlock` directory is an Arduino sketch. Open `boatlock/boatlock.ino`
in the Arduino IDE or build it with `arduino-cli`.

> **Troubleshooting duplicate symbol errors**
>
> The Arduino build system compiles **all** `.ino`, `.cpp`, and `.c` files inside
> the sketch folder. If you accidentally copy the sketch to another file (for
> example `gp.cpp` containing `setup()`/`loop()` or duplicate globals), the linker
> will fail with “multiple definition” errors. Remove any extra files that define
> `setup()`, `loop()`, or the same global variables so that `boatlock.ino` remains
> the only entry point.

### Requirements
- Arduino IDE 2.x (or `arduino-cli`)
- An ESP32‑S3 development board
- Installed libraries:
  - Arduino_GFX
  - NimBLE-Arduino
  - AccelStepper
  - TinyGPSPlus
  - Adafruit Unified Sensor
  - Adafruit HMC5883 Unified

### Build and Upload (Arduino CLI)
```bash
arduino-cli lib install "Arduino_GFX" "NimBLE-Arduino" "AccelStepper" "TinyGPSPlus" \
  "Adafruit Unified Sensor" "Adafruit HMC5883 Unified"
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli compile --fqbn esp32:esp32:esp32s3 boatlock
arduino-cli upload --fqbn esp32:esp32:esp32s3 --port /dev/ttyUSB0 boatlock
```

### Firmware Unit Tests
`boatlock/test` uses Unity tests under PlatformIO native environment.

```bash
cd boatlock
platformio test -e native
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
| Arduino IDE/CLI  | Building and flashing the ESP32 firmware |
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

After wiring, build and flash the firmware from the Arduino IDE or with
`arduino-cli` (see the build section above). Open the serial monitor to verify
that GPS data is being received.

## Compass Calibration

Run the BLE command `CALIB_COMPASS` or press the BOOT button while the device is
running to start calibration. Slowly rotate the device in all directions for
about 10 seconds. The calculated offsets and scale factors are saved to EEPROM
and automatically reloaded on startup.

See [CHANGELOG.md](CHANGELOG.md) for recent changes and firmware versions.

## HC 160A S2 Motor Controller

The HC 160A S2 driver requires two direction pins. Define them in
`boatlock/boatlock.ino`:

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
