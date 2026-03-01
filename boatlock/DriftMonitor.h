#pragma once
#include <Arduino.h>
#include <math.h>

class DriftMonitor {
public:
  static constexpr float SPEED_ALPHA = 0.30f;

  void reset() {
    speedMps_ = 0.0f;
    prevDistM_ = 0.0f;
    prevUpdateMs_ = 0;
    hasPrev_ = false;
    alert_ = false;
    fail_ = false;
  }

  void update(unsigned long nowMs,
              bool anchorActive,
              bool gpsFix,
              float distanceM,
              float alertThresholdM,
              float failThresholdM) {
    if (!anchorActive || !gpsFix || !isfinite(distanceM) || distanceM < 0.0f) {
      reset();
      return;
    }

    const float alertM = max(0.5f, alertThresholdM);
    const float failM = max(alertM + 0.5f, failThresholdM);

    if (hasPrev_) {
      const unsigned long dtMs = nowMs - prevUpdateMs_;
      if (dtMs >= 200 && dtMs <= 5000) {
        const float dt = dtMs / 1000.0f;
        const float instant = fabsf(distanceM - prevDistM_) / dt;
        if (!isfinite(speedMps_)) {
          speedMps_ = instant;
        } else {
          speedMps_ = speedMps_ + (instant - speedMps_) * SPEED_ALPHA;
        }
      }
    }

    prevDistM_ = distanceM;
    prevUpdateMs_ = nowMs;
    hasPrev_ = true;

    alert_ = distanceM >= alertM;
    fail_ = distanceM >= failM;
  }

  float driftSpeedMps() const { return isfinite(speedMps_) ? speedMps_ : 0.0f; }
  bool alertActive() const { return alert_; }
  bool failActive() const { return fail_; }

private:
  float speedMps_ = 0.0f;
  float prevDistM_ = 0.0f;
  unsigned long prevUpdateMs_ = 0;
  bool hasPrev_ = false;
  bool alert_ = false;
  bool fail_ = false;
};
