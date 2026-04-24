#pragma once

#include <Arduino.h>
#include <cmath>
#include <functional>
#include <string>

#include "AnchorControl.h"
#include "AnchorProfiles.h"
#include "BLEBoatLock.h"
#include "BNO08xCompass.h"
#include "BleSecurity.h"
#include "DriftMonitor.h"
#include "HilSimRunner.h"
#include "MotorControl.h"
#include "ParamHelpers.h"
#include "RuntimeGnss.h"
#include "RuntimeMotion.h"
#include "RuntimeStatus.h"
#include "Settings.h"

struct RuntimeBleParamContext {
  BLEBoatLock& ble;
  Settings& settings;
  RuntimeGnss& gnss;
  RuntimeMotion& motion;
  AnchorControl& anchor;
  MotorControl& motor;
  DriftMonitor& driftMonitor;
  BNO08xCompass& compass;
  hilsim::HilSimManager& hilSim;
  BleSecurity& security;
  BLEBoatLock::CommandHandler commandHandler;
  std::function<bool()> compassReady;
  std::function<float()> currentHeadingValue;
  std::function<const char*()> currentModeLabel;
  std::function<int()> gnssQualityLevel;
  std::function<std::string()> buildStatusReasons;
  std::function<const char*()> fixTypeSourceLabel;
};

inline const char* runtimeAnchorProfileName(float rawValue) {
  int raw = (int)rawValue;
  if (raw < 0) {
    raw = 0;
  } else if (raw > 2) {
    raw = 2;
  }
  return anchorProfileName((AnchorProfileId)raw);
}

inline void registerRuntimeBleParams(const RuntimeBleParamContext& context) {
  BLEBoatLock* ble = &context.ble;
  Settings* settings = &context.settings;
  RuntimeGnss* gnss = &context.gnss;
  RuntimeMotion* motion = &context.motion;
  AnchorControl* anchor = &context.anchor;
  MotorControl* motor = &context.motor;
  DriftMonitor* driftMonitor = &context.driftMonitor;
  BNO08xCompass* compass = &context.compass;
  hilsim::HilSimManager* hilSim = &context.hilSim;
  BleSecurity* security = &context.security;
  const BLEBoatLock::CommandHandler commandHandler = context.commandHandler;
  const auto compassReady = context.compassReady;
  const auto currentHeadingValue = context.currentHeadingValue;
  const auto currentModeLabel = context.currentModeLabel;
  const auto gnssQualityLevel = context.gnssQualityLevel;
  const auto buildStatusReasons = context.buildStatusReasons;
  const auto fixTypeSourceLabel = context.fixTypeSourceLabel;

  ble->setCommandHandler(commandHandler);

  ble->registerParam("lat", makeFloatParam([gnss]() {
    return gnss->fix() ? gnss->lastLat() : 0.0f;
  }, "%.6f"));

  ble->registerParam("lon", makeFloatParam([gnss]() {
    return gnss->fix() ? gnss->lastLon() : 0.0f;
  }, "%.6f"));

  ble->registerParam("heading", makeFloatParam([currentHeadingValue]() {
    return currentHeadingValue();
  }, "%.1f"));

  ble->registerParam("headingRaw", makeFloatParam([compass, compassReady]() {
    return compassReady() ? compass->getRawAzimuth() : 0.0f;
  }, "%.1f"));

  ble->registerParam("compassOffset", makeFloatParam([compass, settings, compassReady]() {
    return compassReady() ? compass->getHeadingOffsetDeg() : settings->get("MagOffX");
  }, "%.1f"));

  ble->registerParam("gpsHdgCorr", makeFloatParam([gnss]() {
    return gnss->headingCorrectionDeg(millis());
  }, "%.1f"));

  ble->registerParam("compassQ", makeFloatParam([compass, compassReady]() {
    return compassReady() ? (float)compass->getHeadingQuality() : 0.0f;
  }, "%.0f"));

  ble->registerParam("rvAcc", makeFloatParam([compass, compassReady]() {
    return compassReady() ? compass->getRvAccuracyDeg() : 0.0f;
  }, "%.2f"));

  ble->registerParam("magNorm", makeFloatParam([compass, compassReady]() {
    return compassReady() ? compass->getMagNormUT() : 0.0f;
  }, "%.2f"));

  ble->registerParam("gyroNorm", makeFloatParam([compass, compassReady]() {
    return compassReady() ? compass->getGyroNormDps() : 0.0f;
  }, "%.2f"));

  ble->registerParam("magQ", makeFloatParam([compass, compassReady]() {
    return compassReady() ? (float)compass->getMagQuality() : 0.0f;
  }, "%.0f"));

  ble->registerParam("gyroQ", makeFloatParam([compass, compassReady]() {
    return compassReady() ? (float)compass->getGyroQuality() : 0.0f;
  }, "%.0f"));

  ble->registerParam("pitch", makeFloatParam([compass, compassReady]() {
    return compassReady() ? compass->getPitchDeg() : 0.0f;
  }, "%.2f"));

  ble->registerParam("roll", makeFloatParam([compass, compassReady]() {
    return compassReady() ? compass->getRollDeg() : 0.0f;
  }, "%.2f"));

  ble->registerParam("anchorLat", makeFloatParam([anchor]() {
    return std::isnan(anchor->anchorLat) ? 0.0f : anchor->anchorLat;
  }, "%.6f"));

  ble->registerParam("anchorLon", makeFloatParam([anchor]() {
    return std::isnan(anchor->anchorLon) ? 0.0f : anchor->anchorLon;
  }, "%.6f"));

  ble->registerParam("anchorHead", makeFloatParam([anchor]() {
    return anchor->anchorHeading;
  }, "%.1f"));

  ble->registerParam("holdHeading", makeFloatParam([settings]() {
    return settings->get("HoldHeading");
  }, "%.0f"));

  ble->registerParam("stepSpr", makeFloatParam([settings]() {
    return settings->get("StepSpr");
  }, "%.0f"));

  ble->registerParam("stepMaxSpd", makeFloatParam([settings]() {
    return settings->get("StepMaxSpd");
  }, "%.0f"));

  ble->registerParam("stepAccel", makeFloatParam([settings]() {
    return settings->get("StepAccel");
  }, "%.0f"));

  ble->registerParam("distance", makeFloatParam([gnss]() {
    return gnss->distanceM();
  }, "%.2f"));

  ble->registerParam("driftSpeed", makeFloatParam([driftMonitor]() {
    return driftMonitor->driftSpeedMps();
  }, "%.2f"));

  ble->registerParam("driftAlert", makeFloatParam([motion]() {
    return motion->driftAlertActive() ? 1.0f : 0.0f;
  }, "%.0f"));

  ble->registerParam("driftFail", makeFloatParam([motion]() {
    return motion->driftFailActive() ? 1.0f : 0.0f;
  }, "%.0f"));

  ble->registerParam("gnssQ", makeFloatParam([gnssQualityLevel]() {
    return (float)gnssQualityLevel();
  }, "%.0f"));

  ble->registerParam("motorPwm", makeFloatParam([motor]() {
    return (float)motor->pwmPercent();
  }, "%.0f"));

  ble->registerParam("mode", [currentModeLabel]() {
    return std::string(currentModeLabel());
  });

  ble->registerParam("simState", [hilSim]() {
    return std::string(hilSim->stateLabel());
  });

  ble->registerParam("simScenario", [hilSim]() {
    return hilSim->currentScenarioId();
  });

  ble->registerParam("simProgress", makeFloatParam([hilSim]() {
    return hilSim->progressPct();
  }, "%.1f"));

  ble->registerParam("simPass", makeFloatParam([hilSim]() {
    return (float)hilSim->lastPass();
  }, "%.0f"));

  ble->registerParam("cfgVer", makeFloatParam([]() {
    return (float)Settings::VERSION;
  }, "%.0f"));

  ble->registerParam("anchorProfile", makeFloatParam([settings]() {
    return settings->get("AnchorProf");
  }, "%.0f"));

  ble->registerParam("anchorProfileName", [settings]() {
    return std::string(runtimeAnchorProfileName(settings->get("AnchorProf")));
  });

  ble->registerParam("fixType", []() {
    return std::string("UNKNOWN");
  });

  ble->registerParam("fixTypeSource", [fixTypeSourceLabel]() {
    return std::string(fixTypeSourceLabel());
  });

  ble->registerParam("anchorDeniedReason", [motion]() {
    return std::string(motion->currentAnchorDeniedReason());
  });

  ble->registerParam("failsafeReason", [motion]() {
    return std::string(motion->currentFailsafeReason());
  });

  ble->registerParam("statusReasons", [buildStatusReasons]() {
    return buildStatusReasons();
  });

  ble->registerParam("secPaired", makeFloatParam([security]() {
    return security->isPaired() ? 1.0f : 0.0f;
  }, "%.0f"));

  ble->registerParam("secAuth", makeFloatParam([security]() {
    return security->sessionActive() ? 1.0f : 0.0f;
  }, "%.0f"));

  ble->registerParam("secPairWin", makeFloatParam([security]() {
    return security->pairingWindowOpen(millis()) ? 1.0f : 0.0f;
  }, "%.0f"));

  ble->registerParam("secNonce", [security]() {
    return security->sessionNonceHex();
  });

  ble->registerParam("secReject", [security]() {
    return std::string(security->lastRejectString());
  });
}

inline void notifyRuntimeBleStatus(BLEBoatLock& ble, const RuntimeStatusInput& input) {
  const std::string reasons = buildRuntimeStatusReasons(input);
  ble.setStatus(buildRuntimeStatusSummary(input, reasons));
  ble.notifyAll();
}
