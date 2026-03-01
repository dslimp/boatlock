#pragma once
#include "AnchorControl.h"
#include "BNO08xCompass.h"
#include "Logger.h"
#include "MotorControl.h"
#include "Settings.h"
#include "StepperControl.h"
#include <string>

extern AnchorControl anchor;
extern StepperControl stepperControl;
extern MotorControl motor;
extern Settings settings;
extern BNO08xCompass compass;
extern bool compassReady;
extern bool manualMode;
extern int manualDir;
extern int manualSpeed;
void setPhoneGpsFix(float lat, float lon, float speedKmh, int satellites);
void captureStepperBowZero();

inline void handleBleCommand(const std::string &cmd) {
  if (cmd.rfind("SET_ANCHOR:", 0) == 0) {
    float lat = 0, lon = 0;
    sscanf(cmd.c_str() + 11, "%f,%f", &lat, &lon);
    anchor.saveAnchor(lat, lon, compassReady ? compass.getAzimuth() : 0.0f);
    logMessage("[BLE] Anchor set via BLE: %.6f, %.6f\n", lat, lon);
  } else if (cmd.rfind("SET_HOLD_HEADING:", 0) == 0) {
    int val = atoi(cmd.c_str() + 17);
    settings.set("HoldHeading", val);
    settings.save();
    logMessage("[BLE] HoldHeading set to %d\n", val);
  } else if (cmd.rfind("SET_COMPASS_OFFSET:", 0) == 0) {
    float off = atof(cmd.c_str() + 19);
    compass.setHeadingOffsetDeg(off);
    settings.set("MagOffX", compass.getHeadingOffsetDeg());
    settings.save();
  } else if (cmd == "RESET_COMPASS_OFFSET") {
    compass.setHeadingOffsetDeg(0.0f);
    settings.set("MagOffX", 0.0f);
    settings.save();
  } else if (cmd.rfind("SET_PHONE_GPS:", 0) == 0) {
    float lat = 0.0f, lon = 0.0f, speedKmh = 0.0f;
    int satellites = 0;
    int parsed = sscanf(cmd.c_str() + 14, "%f,%f,%f,%d", &lat, &lon, &speedKmh, &satellites);
    if (parsed >= 2) {
      if (parsed < 3) {
        speedKmh = 0.0f;
      }
      if (parsed < 4) {
        satellites = 0;
      }
      if (lat >= -90.0f && lat <= 90.0f && lon >= -180.0f && lon <= 180.0f) {
        setPhoneGpsFix(lat, lon, speedKmh, satellites);
      } else {
        logMessage("[BLE] Invalid phone GPS range: %.6f, %.6f\n", lat, lon);
      }
    } else {
      logMessage("[BLE] Invalid phone GPS payload: %s\n", cmd.c_str());
    }
  } else if (cmd.rfind("SET_STEP_SPR:", 0) == 0) {
    settings.set("StepSpr", 4096);
    settings.save();
    stepperControl.loadFromSettings();
  } else if (cmd.rfind("SET_STEP_MAXSPD:", 0) == 0) {
    float v = atof(cmd.c_str() + 16);
    settings.set("StepMaxSpd", v);
    settings.save();
    stepperControl.loadFromSettings();
  } else if (cmd.rfind("SET_STEP_ACCEL:", 0) == 0) {
    float v = atof(cmd.c_str() + 15);
    settings.set("StepAccel", v);
    settings.save();
    stepperControl.loadFromSettings();
  } else if (cmd == "SET_STEPPER_BOW") {
    captureStepperBowZero();
  } else if (cmd.rfind("MANUAL:", 0) == 0) {
    bool newMode = atoi(cmd.c_str() + 7) != 0;
    if (newMode) {
      stepperControl.cancelMove();
    } else {
      stepperControl.stopManual();
      motor.stop();
      manualSpeed = 0;
      manualDir = -1;
    }
    manualMode = newMode;
  } else if (cmd.rfind("MANUAL_DIR:", 0) == 0) {
    manualDir = atoi(cmd.c_str() + 11);
  } else if (cmd.rfind("MANUAL_SPEED:", 0) == 0) {
    manualSpeed = atoi(cmd.c_str() + 13);
  } else {
    logMessage("[BLE] Unhandled command: %s\n", cmd.c_str());
  }
}
