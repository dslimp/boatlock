#pragma once
#include <Arduino.h>

class AnchorDiagnostics {
public:
  struct Events {
    bool driftAlert = false;
    bool controlSaturated = false;
    bool sensorTimeout = false;
  };

  static constexpr unsigned long SENSOR_TIMEOUT_MS = 3000;
  static constexpr unsigned long SATURATION_MIN_MS = 2500;
  static constexpr unsigned long SATURATION_COOLDOWN_MS = 10000;

  Events update(unsigned long nowMs,
                bool anchorActive,
                bool gpsFix,
                bool compassReady,
                bool driftAlertActive,
                int motorPwmPercent,
                int motorMaxPercent) {
    Events ev;

    if (!anchorActive) {
      reset();
      return ev;
    }

    if (driftAlertActive && !driftAlertLatched_) {
      ev.driftAlert = true;
    }
    driftAlertLatched_ = driftAlertActive;

    const bool sensorsOk = gpsFix && compassReady;
    if (!sensorsOk) {
      if (sensorBadSinceMs_ == 0) {
        sensorBadSinceMs_ = nowMs;
      } else if (!sensorTimeoutLatched_ && nowMs - sensorBadSinceMs_ >= SENSOR_TIMEOUT_MS) {
        ev.sensorTimeout = true;
        sensorTimeoutLatched_ = true;
      }
    } else {
      sensorBadSinceMs_ = 0;
      sensorTimeoutLatched_ = false;
    }

    const bool saturated = motorPwmPercent >= motorMaxPercent;
    if (saturated) {
      if (satSinceMs_ == 0) {
        satSinceMs_ = nowMs;
      } else if (nowMs - satSinceMs_ >= SATURATION_MIN_MS &&
                 (lastSatEventMs_ == 0 ||
                  nowMs - lastSatEventMs_ >= SATURATION_COOLDOWN_MS)) {
        ev.controlSaturated = true;
        lastSatEventMs_ = nowMs;
      }
    } else {
      satSinceMs_ = 0;
    }

    return ev;
  }

  void reset() {
    driftAlertLatched_ = false;
    sensorBadSinceMs_ = 0;
    sensorTimeoutLatched_ = false;
    satSinceMs_ = 0;
    lastSatEventMs_ = 0;
  }

private:
  bool driftAlertLatched_ = false;
  unsigned long sensorBadSinceMs_ = 0;
  bool sensorTimeoutLatched_ = false;
  unsigned long satSinceMs_ = 0;
  unsigned long lastSatEventMs_ = 0;
};
