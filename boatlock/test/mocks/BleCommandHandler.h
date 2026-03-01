#pragma once
#include "AnchorControl.h"
#include "BNO08xCompass.h"
#include "Logger.h"
#include "MotorControl.h"
#include "AnchorProfiles.h"
#include "AnchorReasons.h"
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
bool canEnableAnchorNow();
bool hasAnchorPoint();
void stopAllMotionNow();
void noteControlActivityNow();
bool nudgeAnchorCardinal(const char* dir, float meters);
bool nudgeAnchorBearing(float bearingDeg, float meters);
const char* currentGnssFailReason();
AnchorDeniedReason currentGnssDeniedReason();
void setLastAnchorDeniedReason(AnchorDeniedReason reason);
void setLastFailsafeReason(FailsafeReason reason);
bool preprocessSecureCommand(const std::string& incoming, std::string* effective);

inline void handleBleCommand(const std::string &cmd) {
  std::string effectiveCmd;
  if (!preprocessSecureCommand(cmd, &effectiveCmd)) {
    return;
  }
  const std::string& command = effectiveCmd;

  noteControlActivityNow();

  if (command == "HEARTBEAT") {
    return;
  }

  if (command == "STOP") {
    setLastFailsafeReason(FailsafeReason::STOP_CMD);
    stopAllMotionNow();
    logMessage("[BLE] STOP accepted\n");
    return;
  }

  if (command == "ANCHOR_OFF") {
    setLastFailsafeReason(FailsafeReason::NONE);
    setLastAnchorDeniedReason(AnchorDeniedReason::NONE);
    settings.set("AnchorEnabled", 0);
    settings.save();
    stepperControl.cancelMove();
    motor.stop();
    logMessage("[BLE] ANCHOR_OFF accepted\n");
    return;
  }

  if (command == "ANCHOR_ON") {
    if (!hasAnchorPoint()) {
      setLastAnchorDeniedReason(AnchorDeniedReason::NO_ANCHOR_POINT);
      logMessage("[BLE] ANCHOR_ON rejected: anchor point is not set\n");
    } else if (!canEnableAnchorNow()) {
      setLastAnchorDeniedReason(currentGnssDeniedReason());
      logMessage("[EVENT] ANCHOR_DENIED reason=%s\n", currentGnssFailReason());
      logMessage("[BLE] ANCHOR_ON rejected: GNSS quality is insufficient\n");
    } else {
      setLastAnchorDeniedReason(AnchorDeniedReason::NONE);
      setLastFailsafeReason(FailsafeReason::NONE);
      manualMode = false;
      manualDir = -1;
      manualSpeed = 0;
      settings.set("AnchorEnabled", 1);
      settings.save();
      logMessage("[BLE] ANCHOR_ON accepted\n");
    }
    return;
  }

  if (command.rfind("SET_ANCHOR:", 0) == 0) {
    float lat = 0, lon = 0;
    sscanf(command.c_str() + 11, "%f,%f", &lat, &lon);
    if (lat < -90.0f || lat > 90.0f || lon < -180.0f || lon > 180.0f) {
      logMessage("[BLE] Invalid anchor range: %.6f, %.6f\n", lat, lon);
      return;
    }
    anchor.saveAnchor(lat, lon, compassReady ? compass.getAzimuth() : 0.0f, false);
    logMessage("[BLE] Anchor point saved via BLE: %.6f, %.6f\n", lat, lon);
  } else if (command.rfind("NUDGE_DIR:", 0) == 0) {
    char dir[12] = {0};
    float meters = 0.0f;
    if (sscanf(command.c_str() + 10, "%11[^,],%f", dir, &meters) == 2) {
      if (nudgeAnchorCardinal(dir, meters)) {
        logMessage("[BLE] NUDGE_DIR accepted: %s, %.2f\n", dir, meters);
      } else {
        logMessage("[BLE] NUDGE_DIR rejected: %s, %.2f\n", dir, meters);
      }
    } else {
      logMessage("[BLE] Invalid NUDGE_DIR payload: %s\n", command.c_str());
    }
  } else if (command.rfind("NUDGE_BRG:", 0) == 0) {
    float bearingDeg = 0.0f;
    float meters = 0.0f;
    if (sscanf(command.c_str() + 10, "%f,%f", &bearingDeg, &meters) == 2) {
      if (nudgeAnchorBearing(bearingDeg, meters)) {
        logMessage("[BLE] NUDGE_BRG accepted: %.1f, %.2f\n", bearingDeg, meters);
      } else {
        logMessage("[BLE] NUDGE_BRG rejected: %.1f, %.2f\n", bearingDeg, meters);
      }
    } else {
      logMessage("[BLE] Invalid NUDGE_BRG payload: %s\n", command.c_str());
    }
  } else if (command.rfind("SET_HOLD_HEADING:", 0) == 0) {
    int val = atoi(command.c_str() + 17);
    settings.set("HoldHeading", val);
    settings.save();
    logMessage("[BLE] HoldHeading set to %d\n", val);
  } else if (command.rfind("SET_ANCHOR_PROFILE:", 0) == 0) {
    AnchorProfileId profile = AnchorProfileId::NORMAL;
    const char* payload = command.c_str() + 19;
    if (!parseAnchorProfile(payload, &profile)) {
      logMessage("[BLE] SET_ANCHOR_PROFILE rejected: invalid payload '%s'\n", payload);
      return;
    }
    if (!applyAnchorProfile(settings, profile)) {
      logMessage("[BLE] SET_ANCHOR_PROFILE rejected: apply failed\n");
      return;
    }
    logMessage("[BLE] SET_ANCHOR_PROFILE accepted: %s\n", anchorProfileName(profile));
  } else if (command.rfind("SET_COMPASS_OFFSET:", 0) == 0) {
    float off = atof(command.c_str() + 19);
    compass.setHeadingOffsetDeg(off);
    settings.set("MagOffX", compass.getHeadingOffsetDeg());
    settings.save();
  } else if (command == "RESET_COMPASS_OFFSET") {
    compass.setHeadingOffsetDeg(0.0f);
    settings.set("MagOffX", 0.0f);
    settings.save();
  } else if (command.rfind("SET_PHONE_GPS:", 0) == 0) {
    float lat = 0.0f, lon = 0.0f, speedKmh = 0.0f;
    int satellites = 0;
    int parsed = sscanf(command.c_str() + 14, "%f,%f,%f,%d", &lat, &lon, &speedKmh, &satellites);
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
      logMessage("[BLE] Invalid phone GPS payload: %s\n", command.c_str());
    }
  } else if (command.rfind("SET_STEP_SPR:", 0) == 0) {
    settings.set("StepSpr", 4096);
    settings.save();
    stepperControl.loadFromSettings();
  } else if (command.rfind("SET_STEP_MAXSPD:", 0) == 0) {
    float v = atof(command.c_str() + 16);
    settings.set("StepMaxSpd", v);
    settings.save();
    stepperControl.loadFromSettings();
  } else if (command.rfind("SET_STEP_ACCEL:", 0) == 0) {
    float v = atof(command.c_str() + 15);
    settings.set("StepAccel", v);
    settings.save();
    stepperControl.loadFromSettings();
  } else if (command == "SET_STEPPER_BOW") {
    captureStepperBowZero();
  } else if (command.rfind("MANUAL:", 0) == 0) {
    bool newMode = atoi(command.c_str() + 7) != 0;
    if (newMode) {
      stepperControl.cancelMove();
    } else {
      stepperControl.stopManual();
      motor.stop();
      manualSpeed = 0;
      manualDir = -1;
    }
    manualMode = newMode;
  } else if (command.rfind("MANUAL_DIR:", 0) == 0) {
    manualDir = atoi(command.c_str() + 11);
  } else if (command.rfind("MANUAL_SPEED:", 0) == 0) {
    manualSpeed = atoi(command.c_str() + 13);
  } else {
    logMessage("[BLE] Unhandled command: %s\n", command.c_str());
  }
}
