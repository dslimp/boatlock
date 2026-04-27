#pragma once

#include <Arduino.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

enum class ManualControlSource : uint8_t {
  NONE = 0,
  BLE_PHONE = 1,
  BLE_REMOTE = 2,
};

class ManualControl {
public:
  static constexpr unsigned long kMinTtlMs = 100;
  static constexpr unsigned long kMaxTtlMs = 1000;
  static constexpr unsigned long kDefaultTtlMs = 1000;

  bool apply(ManualControlSource source,
             float angleDeg,
             int throttlePct,
             unsigned long ttlMs,
             unsigned long nowMs) {
    if (!validSource(source) || !validAngleDeg(angleDeg) || !validThrottlePct(throttlePct) ||
        !validTtlMs(ttlMs)) {
      return false;
    }
    if (active_ && source_ != source) {
      if ((unsigned long)(nowMs - updatedMs_) < ttlMs_) {
        return false;
      }
      stop();
    }
    source_ = source;
    angleDeg_ = angleDeg;
    throttlePct_ = throttlePct;
    ttlMs_ = ttlMs;
    updatedMs_ = nowMs;
    active_ = true;
    return true;
  }

  bool update(unsigned long nowMs) {
    if (!active_) {
      return false;
    }
    if ((unsigned long)(nowMs - updatedMs_) < ttlMs_) {
      return false;
    }
    stop();
    return true;
  }

  void stop() {
    active_ = false;
    source_ = ManualControlSource::NONE;
    angleDeg_ = 0.0f;
    throttlePct_ = 0;
    ttlMs_ = 0;
    updatedMs_ = 0;
  }

  bool active() const { return active_; }
  ManualControlSource source() const { return source_; }
  float angleDeg() const { return active_ ? angleDeg_ : 0.0f; }
  int throttlePct() const { return active_ ? throttlePct_ : 0; }
  unsigned long ttlMs() const { return ttlMs_; }

  int motorPwm() const {
    if (!active_) {
      return 0;
    }
    return constrain((throttlePct_ * 255) / 100, -255, 255);
  }

  static bool validAngleDeg(float angleDeg) {
    return isfinite(angleDeg) && angleDeg >= -90.0f && angleDeg <= 90.0f;
  }

  static bool validThrottlePct(int throttlePct) {
    return throttlePct >= -100 && throttlePct <= 100;
  }

  static bool validTtlMs(unsigned long ttlMs) {
    return ttlMs >= kMinTtlMs && ttlMs <= kMaxTtlMs;
  }

private:
  static bool validSource(ManualControlSource source) {
    return source == ManualControlSource::BLE_PHONE || source == ManualControlSource::BLE_REMOTE;
  }

  bool active_ = false;
  ManualControlSource source_ = ManualControlSource::NONE;
  float angleDeg_ = 0.0f;
  int throttlePct_ = 0;
  unsigned long ttlMs_ = 0;
  unsigned long updatedMs_ = 0;
};
