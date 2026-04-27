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
  std::function<RuntimeStatusSnapshot()> buildStatusSnapshot;
  std::function<const char*()> fixTypeSourceLabel;
};

inline void setRuntimeBleAnchorTelemetry(RuntimeBleLiveTelemetry* telemetry,
                                         const AnchorControl& anchor) {
  if (!telemetry || !AnchorControl::validAnchorPoint(anchor.anchorLat, anchor.anchorLon)) {
    if (telemetry) {
      telemetry->anchorLat = 0.0;
      telemetry->anchorLon = 0.0;
      telemetry->anchorHeadingDeg = 0.0f;
      telemetry->anchorBearingDeg = 0.0f;
    }
    return;
  }

  telemetry->anchorLat = anchor.anchorLat;
  telemetry->anchorLon = anchor.anchorLon;
  telemetry->anchorHeadingDeg = isfinite(anchor.anchorHeading)
                                    ? AnchorControl::normalizeHeading(anchor.anchorHeading)
                                    : 0.0f;
}

inline bool setRuntimeBleAnchorRangeTelemetry(RuntimeBleLiveTelemetry* telemetry,
                                              const AnchorControl& anchor,
                                              const RuntimeGnss& gnss) {
  if (!telemetry ||
      !AnchorControl::validAnchorPoint(anchor.anchorLat, anchor.anchorLon) ||
      !gnss.fix() ||
      !RuntimeGnss::validPosition(gnss.lastLat(), gnss.lastLon())) {
    if (telemetry) {
      telemetry->distanceM = 0.0f;
      telemetry->anchorBearingDeg = 0.0f;
    }
    return false;
  }

  static constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
  static constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
  static constexpr double kEarthRadiusM = 6378137.0;

  const double lat1 = gnss.lastLat() * kDegToRad;
  const double lat2 = anchor.anchorLat * kDegToRad;
  const double dlon = (anchor.anchorLon - gnss.lastLon()) * kDegToRad;
  const double dlat = lat2 - lat1;
  const double sinHalfLat = sin(dlat / 2.0);
  const double sinHalfLon = sin(dlon / 2.0);
  const double haversine =
      sinHalfLat * sinHalfLat + cos(lat1) * cos(lat2) * sinHalfLon * sinHalfLon;
  const double distanceM =
      kEarthRadiusM * 2.0 *
      atan2(sqrt(haversine), sqrt(fmax(0.0, 1.0 - haversine)));
  const double y = sin(dlon) * cos(lat2);
  const double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlon);
  const double bearing = atan2(y, x) * kRadToDeg;
  if (!isfinite(distanceM) || !isfinite(bearing)) {
    telemetry->distanceM = 0.0f;
    telemetry->anchorBearingDeg = 0.0f;
    return false;
  }
  telemetry->distanceM = (float)distanceM;
  telemetry->anchorBearingDeg = AnchorControl::normalizeHeading((float)bearing);
  return true;
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

inline bool projectRuntimeBleSimMeters(double baseLat,
                                       double baseLon,
                                       float eastM,
                                       float northM,
                                       double* lat,
                                       double* lon) {
  if (!lat || !lon || !isfinite(baseLat) || !isfinite(baseLon) ||
      !isfinite(eastM) || !isfinite(northM)) {
    return false;
  }

  static constexpr double kMetersPerDegLat = 111320.0;
  static constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
  const double cosLat = cos(baseLat * kDegToRad);
  if (!isfinite(cosLat) || fabs(cosLat) < 0.01) {
    return false;
  }
  *lat = baseLat + ((double)northM / kMetersPerDegLat);
  *lon = baseLon + ((double)eastM / (kMetersPerDegLat * cosLat));
  return isfinite(*lat) && isfinite(*lon);
}

inline bool applyRuntimeBleHilSimTelemetry(
    RuntimeBleLiveTelemetry* telemetry,
    const hilsim::HilScenarioRunner::LiveTelemetry& live) {
  if (!telemetry || !live.valid) {
    return false;
  }

  static constexpr double kSimBaseLat = 59.938630;
  static constexpr double kSimBaseLon = 30.314130;
  double boatLat = 0.0;
  double boatLon = 0.0;
  double anchorLat = 0.0;
  double anchorLon = 0.0;
  if (!projectRuntimeBleSimMeters(kSimBaseLat,
                                  kSimBaseLon,
                                  live.world.xM,
                                  live.world.yM,
                                  &boatLat,
                                  &boatLon) ||
      !projectRuntimeBleSimMeters(kSimBaseLat,
                                  kSimBaseLon,
                                  live.anchorXM,
                                  live.anchorYM,
                                  &anchorLat,
                                  &anchorLon)) {
    return false;
  }

  telemetry->lat = live.gnss.valid ? boatLat : 0.0;
  telemetry->lon = live.gnss.valid ? boatLon : 0.0;
  telemetry->anchorLat = anchorLat;
  telemetry->anchorLon = anchorLon;
  telemetry->distanceM = live.errTrueM;
  telemetry->anchorBearingDeg = isfinite(live.bearingDeg) ? live.bearingDeg : 0.0f;
  telemetry->headingDeg =
      (live.heading.valid && isfinite(live.heading.headingDeg)) ? live.heading.headingDeg : 0.0f;
  telemetry->headingRawDeg = telemetry->headingDeg;
  telemetry->compassQ = live.heading.valid ? 3 : 0;
  telemetry->magQ = live.heading.valid ? 3 : 0;
  telemetry->gyroQ = live.heading.valid ? 3 : 0;
  telemetry->rvAccDeg = live.heading.valid ? 2.0f : 0.0f;
  telemetry->gnssQ = live.gnss.valid ? 2 : 0;
  return true;
}

inline void registerRuntimeBleParams(const RuntimeBleParamContext& context) {
  BLEBoatLock* ble = &context.ble;
  Settings* settings = &context.settings;
  RuntimeGnss* gnss = &context.gnss;
  AnchorControl* anchor = &context.anchor;
  BNO08xCompass* compass = &context.compass;
  hilsim::HilSimManager* hilSim = &context.hilSim;
  BleSecurity* security = &context.security;
  const BLEBoatLock::CommandHandler commandHandler = context.commandHandler;
  const auto compassReady = context.compassReady;
  const auto currentHeadingValue = context.currentHeadingValue;
  const auto currentModeLabel = context.currentModeLabel;
  const auto gnssQualityLevel = context.gnssQualityLevel;
  const auto buildStatusSnapshot = context.buildStatusSnapshot;

  ble->setCommandHandler(commandHandler);
  ble->setTelemetryProvider([=]() {
    RuntimeBleLiveTelemetry telemetry;
    const bool compassReadyNow = compassReady();
    telemetry.lat = gnss->fix() ? gnss->lastLat() : 0.0;
    telemetry.lon = gnss->fix() ? gnss->lastLon() : 0.0;
    setRuntimeBleAnchorTelemetry(&telemetry, *anchor);
    setRuntimeBleAnchorRangeTelemetry(&telemetry, *anchor, *gnss);
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
    const RuntimeStatusSnapshot statusSnapshot = buildStatusSnapshot();
    telemetry.status = statusSnapshot.summary;
    telemetry.statusReasons = statusSnapshot.reasons;
    telemetry.secReject = std::string(security->lastRejectString());
    telemetry.gnssQ = runtimeBleTelemetryQuality(gnssQualityLevel(), 2);
    hilsim::HilScenarioRunner::LiveTelemetry simLive;
    if (hilSim->isRunning() && hilSim->liveTelemetry(&simLive)) {
      applyRuntimeBleHilSimTelemetry(&telemetry, simLive);
    }
    return telemetry;
  });
}

inline void notifyRuntimeBleStatus(BLEBoatLock& ble, const RuntimeStatusInput& input) {
  const std::string reasons = buildRuntimeStatusReasons(input);
  ble.setStatus(buildRuntimeStatusSummary(input, reasons));
  ble.notifyLive();
}
