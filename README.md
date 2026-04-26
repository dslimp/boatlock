# Boatlock

This repository contains firmware for an ESP32-S3 based lock and a Flutter application used to control it.

## Device Functions

Boatlock combines the ESP32-S3 firmware with a companion mobile app to automate
anchoring and thruster control. Major capabilities include:

- saving the current location as an anchor point with heading
- automatically holding position near the saved anchor
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

## Quick Start: ReadyToSky NEO-M8N GPS Module

The firmware is preconfigured for a hardware GPS receiver on UART1. Connect the
GPS side of the module to the ESP32-S3 board as follows:

- **GPS TX** → **GPIO17**
- **GPS RX** → **GPIO18**
- **VCC** → 5 V (or 3.3 V if your module supports it)
- **GND** → **GND**

After wiring, build and flash the firmware with PlatformIO
(see the build section above). Open the serial monitor to verify
that GPS data is being received.

## Compass (BNO08x)

The firmware trusts onboard BNO08x UART-RVC heading frames only. The old I2C/SH2
path is not part of production firmware.

Current default wiring:

- **BNO08x SDA/TX** → **GPIO12**
- **BNO08x RST** → **GPIO13**
- **BNO08x P0/PS0** → **3V3**
- **BNO08x P1/PS1** → **GND**

Acceptance requires `[COMPASS] ready=1 source=BNO08x-RVC rx=12 baud=115200` and
fresh `[COMPASS] heading events ready` logs. See
[docs/COMPASS_BNO08X.md](docs/COMPASS_BNO08X.md).

If mounting angle needs adjustment, use the compass offset controls in app
settings (`SET_COMPASS_OFFSET` / `RESET_COMPASS_OFFSET`).

See [CHANGELOG.md](CHANGELOG.md) for recent changes and firmware versions.

## HC 160A S2 Motor Controller

The HC 160A S2 driver requires two direction pins. They are configured in
`boatlock/main.cpp`:

```cpp
constexpr int kMotorDirPin1 = 5;   // IN1 on the driver
constexpr int kMotorDirPin2 = 10;  // IN2 on the driver
```

During setup the firmware calls:

```cpp
motor.setDirPins(kMotorDirPin1, kMotorDirPin2);
```

Pin 1 should be HIGH and Pin 2 LOW for forward rotation. The logic is reversed
for reverse rotation. Adjust the GPIO numbers if you wire the controller to
different pins on the ESP32 board.

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
