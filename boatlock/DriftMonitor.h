#pragma once
#include <Arduino.h>
#include <math.h>

class DriftMonitor {
public:
  static constexpr float SPEED_ALPHA = 0.30f;
  static constexpr float DEFAULT_ALERT_M = 6.0f;
  static constexpr float DEFAULT_FAIL_M = 12.0f;
  static constexpr unsigned long SPEED_MIN_DT_MS = 200;
  static constexpr unsigned long SPEED_MAX_DT_MS = 5000;

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

    const float alertM = max(0.5f, finiteOr(alertThresholdM, DEFAULT_ALERT_M));
    const float failM = max(alertM + 0.5f, finiteOr(failThresholdM, DEFAULT_FAIL_M));

    if (hasPrev_) {
      const unsigned long dtMs = elapsedMs(nowMs, prevUpdateMs_);
      if (dtMs >= SPEED_MIN_DT_MS && dtMs <= SPEED_MAX_DT_MS) {
        const float dt = dtMs / 1000.0f;
        const float instant = fabsf(distanceM - prevDistM_) / dt;
        if (!isfinite(speedMps_)) {
          speedMps_ = instant;
        } else {
          speedMps_ = speedMps_ + (instant - speedMps_) * SPEED_ALPHA;
        }
      } else if (dtMs > SPEED_MAX_DT_MS) {
        speedMps_ = 0.0f;
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
  static unsigned long elapsedMs(unsigned long nowMs, unsigned long thenMs) {
    return nowMs - thenMs;
  }

  static float finiteOr(float value, float fallback) {
    return isfinite(value) ? value : fallback;
  }

  float speedMps_ = 0.0f;
  float prevDistM_ = 0.0f;
  unsigned long prevUpdateMs_ = 0;
  bool hasPrev_ = false;
  bool alert_ = false;
  bool fail_ = false;
};
