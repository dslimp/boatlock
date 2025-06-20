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
