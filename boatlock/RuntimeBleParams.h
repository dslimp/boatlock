#pragma once

#include <Arduino.h>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>

#include "AnchorControl.h"
#include "BLEBoatLock.h"
#include "BNO08xCompass.h"
#include "BleSecurity.h"
#include "DriftMonitor.h"
#include "HilSimRunner.h"
#include "MotorControl.h"
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

inline void setRuntimeBleAnchorTelemetry(RuntimeBleLiveTelemetry* telemetry,
                                         const AnchorControl& anchor) {
  if (!telemetry || !AnchorControl::validAnchorPoint(anchor.anchorLat, anchor.anchorLon)) {
    if (telemetry) {
      telemetry->anchorLat = 0.0;
      telemetry->anchorLon = 0.0;
      telemetry->anchorHeadingDeg = 0.0f;
    }
    return;
  }

  telemetry->anchorLat = anchor.anchorLat;
  telemetry->anchorLon = anchor.anchorLon;
  telemetry->anchorHeadingDeg = isfinite(anchor.anchorHeading)
                                    ? AnchorControl::normalizeHeading(anchor.anchorHeading)
                                    : 0.0f;
}

inline uint16_t runtimeBleTelemetryU16(float value) {
  if (!isfinite(value) || value <= 0.0f) {
    return 0;
  }
  if (value >= (float)UINT16_MAX) {
    return UINT16_MAX;
  }
  return (uint16_t)value;
}

inline uint8_t runtimeBleTelemetryQuality(int value, uint8_t maxValue) {
  if (value < 0 || value > maxValue) {
    return 0;
  }
  return (uint8_t)value;
}

inline void registerRuntimeBleParams(const RuntimeBleParamContext& context) {
  BLEBoatLock* ble = &context.ble;
  Settings* settings = &context.settings;
  RuntimeGnss* gnss = &context.gnss;
  AnchorControl* anchor = &context.anchor;
  BNO08xCompass* compass = &context.compass;
  BleSecurity* security = &context.security;
  const BLEBoatLock::CommandHandler commandHandler = context.commandHandler;
  const auto compassReady = context.compassReady;
  const auto currentHeadingValue = context.currentHeadingValue;
  const auto currentModeLabel = context.currentModeLabel;
  const auto gnssQualityLevel = context.gnssQualityLevel;
  const auto buildStatusReasons = context.buildStatusReasons;

  ble->setCommandHandler(commandHandler);
  ble->setTelemetryProvider([=]() {
    RuntimeBleLiveTelemetry telemetry;
    const bool compassReadyNow = compassReady();
    telemetry.lat = gnss->fix() ? gnss->lastLat() : 0.0;
    telemetry.lon = gnss->fix() ? gnss->lastLon() : 0.0;
    setRuntimeBleAnchorTelemetry(&telemetry, *anchor);
    telemetry.distanceM = gnss->distanceM();
    telemetry.headingDeg = currentHeadingValue();
    telemetry.holdHeading = settings->get("HoldHeading") >= 0.5f;
    telemetry.stepSpr = runtimeBleTelemetryU16(settings->get("StepSpr"));
    telemetry.stepMaxSpd = runtimeBleTelemetryU16(settings->get("StepMaxSpd"));
    telemetry.stepAccel = runtimeBleTelemetryU16(settings->get("StepAccel"));
    telemetry.headingRawDeg = compassReadyNow ? compass->getRawAzimuth() : 0.0f;
    telemetry.compassOffsetDeg =
        compassReadyNow ? compass->getHeadingOffsetDeg() : settings->get("MagOffX");
    telemetry.compassQ = compassReadyNow
                              ? runtimeBleTelemetryQuality(compass->getHeadingQuality(), 3)
                              : 0;
    telemetry.magQ = compassReadyNow
                         ? runtimeBleTelemetryQuality(compass->getMagQuality(), 3)
                         : 0;
    telemetry.gyroQ = compassReadyNow
                          ? runtimeBleTelemetryQuality(compass->getGyroQuality(), 3)
                          : 0;
    telemetry.rvAccDeg = compassReadyNow ? compass->getRvAccuracyDeg() : 0.0f;
    telemetry.magNorm = compassReadyNow ? compass->getMagNormUT() : 0.0f;
    telemetry.gyroNorm = compassReadyNow ? compass->getGyroNormDps() : 0.0f;
    telemetry.pitchDeg = compassReadyNow ? compass->getPitchDeg() : 0.0f;
    telemetry.rollDeg = compassReadyNow ? compass->getRollDeg() : 0.0f;
    telemetry.secPaired = security->isPaired();
    telemetry.secAuth = security->sessionActive();
    telemetry.secPairWindowOpen = security->pairingWindowOpen(millis());
    telemetry.secNonce = security->sessionNonce();
    telemetry.mode = std::string(currentModeLabel());
    telemetry.status = ble->runtimeStatus();
    telemetry.statusReasons = buildStatusReasons();
    telemetry.secReject = std::string(security->lastRejectString());
    telemetry.gnssQ = runtimeBleTelemetryQuality(gnssQualityLevel(), 2);
    return telemetry;
  });
}

inline void notifyRuntimeBleStatus(BLEBoatLock& ble, const RuntimeStatusInput& input) {
  const std::string reasons = buildRuntimeStatusReasons(input);
  ble.setStatus(buildRuntimeStatusSummary(input, reasons));
  ble.notifyLive();
}
