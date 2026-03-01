# Boatlock

This repository contains firmware for an ESP32-S3 based lock and a Flutter application used to control it.

## Device Functions

Boatlock combines the ESP32-S3 firmware with a companion mobile app to automate
anchoring and thruster control. Major capabilities include:

- saving the current location as an anchor point with heading
- automatically holding position near the saved anchor
- manual rudder and motor control from the app
- onboard BNO08x heading diagnostics and persistent heading offset
- on-device deterministic HIL simulation scenarios (`SIM_*`, `S0..S19`) for regression checks without external sensors

The app and firmware communicate through a simple text protocol; see
[docs/BLE_PROTOCOL.md](docs/BLE_PROTOCOL.md) for the full command list.

Current firmware release: `0.2.0`

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
3. Anchor thrust uses human-friendly control: hold radius (`DistTh`) + deadband, PWM ramp limiting, and anti-oscillation filtering to avoid constant motor twitching.

### Manual Override
1. Enable **Ручной режим** in the app.
2. Use left/right steering buttons and the speed slider for direct control.

## Building the Firmware

The `boatlock` directory is built with PlatformIO only.

### Requirements
- PlatformIO CLI
- ESP32‑S3 development board

### Build and Upload (PlatformIO)
```bash
cd boatlock
pio run -e esp32s3
pio run -e esp32s3 -t upload
pio device monitor -e esp32s3
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
| PlatformIO CLI   | Building and flashing the ESP32 firmware |
| Flutter SDK      | Running the cross‑platform UI           |
| Git              | Cloning and updating this repository    |

Ensure these tools are installed and available in your `PATH` before attempting to build or run the project.

## Quick Start: ReadyToSky NEO‑M8N GPS Module

The firmware is preconfigured for the ReadyToSky GPS+compass board based on the
NEO‑M8N receiver. Connect the module to the ESP32‑S3 board as follows:

- **GPS TX** → **GPIO17**
- **GPS RX** → **GPIO18**
- **SDA** → **GPIO47**
- **SCL** → **GPIO48**
- **VCC** → 5 V (or 3.3 V if your module supports it)
- **GND** → **GND**

After wiring, build and flash the firmware with PlatformIO
(see the build section above). Open the serial monitor to verify
that GPS data is being received.

## Compass (BNO08x)

The firmware uses onboard BNO08x dynamic calibration and does not expose a
manual BLE calibration command. If mounting angle needs adjustment, use the
compass offset controls in app settings (`SET_COMPASS_OFFSET` /
`RESET_COMPASS_OFFSET`).

See [CHANGELOG.md](CHANGELOG.md) for recent changes and firmware versions.

## HC 160A S2 Motor Controller

The HC 160A S2 driver requires two direction pins. They are configured in
`boatlock/main.cpp`:

```cpp
constexpr int kMotorDirPin1 = 6;   // IN1 on the driver
constexpr int kMotorDirPin2 = 10;  // IN2 on the driver
```

During setup the firmware calls:

```cpp
motor.setDirPins(kMotorDirPin1, kMotorDirPin2);
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

## Emergency STOP Button (Hardware)

Firmware supports a dedicated hardware STOP input:

- **GPIO15** (configured as `INPUT_PULLUP`)
- connect a **momentary button between GPIO15 and GND**
- pressing the button triggers the same action as BLE `STOP`:
  - disables Anchor mode
  - exits manual mode
  - cancels stepper movement
  - stops thruster PWM

## License

This project is licensed under the terms of the [MIT License](LICENSE).
