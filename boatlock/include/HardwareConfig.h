#pragma once

// Hardware feature flags (enable by default for Arduino IDE builds).
// Override by editing this file or defining the macros before including it.

#ifndef BOATLOCK_ENABLE_CAMERA
#define BOATLOCK_ENABLE_CAMERA 1
#endif

#ifndef BOATLOCK_ENABLE_IMU
#define BOATLOCK_ENABLE_IMU 1
#endif

// Camera pin map for ESP32-S3-Touch-LCD-2 (OV5640).
// Override any pin with build flags if your wiring differs.
#ifndef BOATLOCK_CAM_PWDN_PIN
#define BOATLOCK_CAM_PWDN_PIN 17
#endif
#ifndef BOATLOCK_CAM_RESET_PIN
#define BOATLOCK_CAM_RESET_PIN -1
#endif
#ifndef BOATLOCK_CAM_XCLK_PIN
#define BOATLOCK_CAM_XCLK_PIN 8
#endif
#ifndef BOATLOCK_CAM_SIOD_PIN
#define BOATLOCK_CAM_SIOD_PIN 21
#endif
#ifndef BOATLOCK_CAM_SIOC_PIN
#define BOATLOCK_CAM_SIOC_PIN 16
#endif
#ifndef BOATLOCK_CAM_Y9_PIN
#define BOATLOCK_CAM_Y9_PIN 2
#endif
#ifndef BOATLOCK_CAM_Y8_PIN
#define BOATLOCK_CAM_Y8_PIN 7
#endif
#ifndef BOATLOCK_CAM_Y7_PIN
#define BOATLOCK_CAM_Y7_PIN 10
#endif
#ifndef BOATLOCK_CAM_Y6_PIN
#define BOATLOCK_CAM_Y6_PIN 14
#endif
#ifndef BOATLOCK_CAM_Y5_PIN
#define BOATLOCK_CAM_Y5_PIN 11
#endif
#ifndef BOATLOCK_CAM_Y4_PIN
#define BOATLOCK_CAM_Y4_PIN 15
#endif
#ifndef BOATLOCK_CAM_Y3_PIN
#define BOATLOCK_CAM_Y3_PIN 13
#endif
#ifndef BOATLOCK_CAM_Y2_PIN
#define BOATLOCK_CAM_Y2_PIN 12
#endif
#ifndef BOATLOCK_CAM_VSYNC_PIN
#define BOATLOCK_CAM_VSYNC_PIN 6
#endif
#ifndef BOATLOCK_CAM_HREF_PIN
#define BOATLOCK_CAM_HREF_PIN 4
#endif
#ifndef BOATLOCK_CAM_PCLK_PIN
#define BOATLOCK_CAM_PCLK_PIN 9
#endif

// QMI8658 IMU address probe list.
#ifndef BOATLOCK_IMU_ADDR_PRIMARY
#define BOATLOCK_IMU_ADDR_PRIMARY 0x6B
#endif
#ifndef BOATLOCK_IMU_ADDR_SECONDARY
#define BOATLOCK_IMU_ADDR_SECONDARY 0x6A
#endif
