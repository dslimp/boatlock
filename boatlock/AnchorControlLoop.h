#pragma once

#include <algorithm>
#include <math.h>
#include <stdint.h>
#include "ControlInterfaces.h"

namespace simctl {

inline float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

inline float wrap360f(float deg) {
  while (deg < 0.0f) deg += 360.0f;
  while (deg >= 360.0f) deg -= 360.0f;
  return deg;
}

inline float wrap180f(float deg) {
  while (deg > 180.0f) deg -= 360.0f;
  while (deg < -180.0f) deg += 360.0f;
  return deg;
}

inline float bearingDeg(float fromX, float fromY, float toX, float toY) {
  const float dx = toX - fromX;
  const float dy = toY - fromY;
  return wrap360f(atan2f(dx, dy) * 57.2957795f);
}

enum class GnssReason : uint8_t {
  NONE = 0,
  NO_FIX,
  DATA_STALE,
  SATS_TOO_LOW,
  HDOP_TOO_HIGH,
  POSITION_JUMP,
  SPEED_INVALID,
};

enum class StopReason : uint8_t {
  NONE = 0,
  GPS_DATA_STALE,
  CONTROL_LOOP_TIMEOUT,
  SENSOR_TIMEOUT,
  INTERNAL_ERROR_NAN,
  COMMAND_OUT_OF_RANGE,
  MAX_CONTINUOUS_THRUST,
};

inline const char* gnssReasonStr(GnssReason r) {
  switch (r) {
    case GnssReason::NO_FIX:
      return "GPS_NO_FIX";
    case GnssReason::DATA_STALE:
      return "GPS_DATA_STALE";
    case GnssReason::SATS_TOO_LOW:
      return "GPS_SATS_TOO_LOW";
    case GnssReason::HDOP_TOO_HIGH:
      return "GPS_HDOP_TOO_HIGH";
    case GnssReason::POSITION_JUMP:
      return "GPS_POSITION_JUMP";
    case GnssReason::SPEED_INVALID:
      return "GPS_SPEED_INVALID";
    case GnssReason::NONE:
    default:
      return "NONE";
  }
}

inline const char* stopReasonStr(StopReason r) {
  switch (r) {
    case StopReason::GPS_DATA_STALE:
      return "GPS_DATA_STALE";
    case StopReason::CONTROL_LOOP_TIMEOUT:
      return "CONTROL_LOOP_TIMEOUT";
    case StopReason::SENSOR_TIMEOUT:
      return "SENSOR_TIMEOUT";
    case StopReason::INTERNAL_ERROR_NAN:
      return "INTERNAL_ERROR_NAN";
    case StopReason::COMMAND_OUT_OF_RANGE:
      return "COMMAND_OUT_OF_RANGE";
    case StopReason::MAX_CONTINUOUS_THRUST:
      return "MAX_CONTINUOUS_THRUST";
    case StopReason::NONE:
    default:
      return "NONE";
  }
}

struct ControllerConfig {
  // GNSS gate.
  int minSats = 10;
  float maxHdop = 1.8f;
  unsigned long maxGpsAgeMs = 1500;
  float maxPositionJumpM = 8.0f;
  unsigned long minGoodTimeToEnterMs = 2000;
  unsigned long badTimeToExitMs = 1500;
  float speedSanityMaxMps = 8.0f;

  // Anchor control.
  float deadbandM = 1.5f;
  float holdRadiusM = 2.5f;
  float reacquireRadiusM = 3.5f;
  float maxThrust = 0.60f;
  float minThrust = 0.08f;
  float thrustRampPerS = 0.35f;
  float maxTurnRateDegS = 120.0f;

  // Safety.
  unsigned long controlLoopTimeoutMs = 200;
  unsigned long sensorTimeoutMs = 1500;
  unsigned long maxContinuousThrustTimeMs = 60000;
  bool autoResume = false;
};

class IControlEventSink {
public:
  virtual ~IControlEventSink() {}
  virtual void onControlEvent(unsigned long atMs,
                              const char* code,
                              const char* details) = 0;
};

struct ControllerTelemetry {
  bool gnssGood = false;
  GnssReason gnssReason = GnssReason::NONE;
  bool anchorActive = false;
  bool safeStop = false;
  StopReason stopReason = StopReason::NONE;
  bool rampViolation = false;
  bool commandOutOfRange = false;
  bool nanDetected = false;
  ActuatorCmd command;
};

class AnchorControlLoop {
public:
  AnchorControlLoop(IGnssSource* gnss,
                    IHeadingSource* heading,
                    IActuator* actuator,
                    IClock* clock)
      : gnss_(gnss), heading_(heading), actuator_(actuator), clock_(clock) {}

  void setConfig(const ControllerConfig& cfg) { cfg_ = cfg; }
  const ControllerConfig& config() const { return cfg_; }
  void setEventSink(IControlEventSink* sink) { sink_ = sink; }

  void reset() {
    anchorRequested_ = true;
    anchorActive_ = false;
    safeStop_ = false;
    stopReason_ = StopReason::NONE;
    goodGnssSinceMs_ = 0;
    badGnssSinceMs_ = 0;
    sensorBadSinceMs_ = 0;
    lastTickMs_ = 0;
    lastThrust_ = 0.0f;
    lastThrustMs_ = 0;
    thrustBias_ = 0.0f;
    maxThrustSinceMs_ = 0;
    maxThrustEventSent_ = false;
    prevFixValid_ = false;
    prevFixX_ = 0.0f;
    prevFixY_ = 0.0f;
    prevFixMs_ = 0;
    filtFixValid_ = false;
    filtFixX_ = 0.0f;
    filtFixY_ = 0.0f;
    lastTelemetry_ = {};
    applyStop();
  }

  void requestAnchor(bool enabled) {
    anchorRequested_ = enabled;
    if (!enabled) {
      anchorActive_ = false;
      safeStop_ = false;
      stopReason_ = StopReason::NONE;
      applyStop();
    }
  }

  bool isAnchorActive() const { return anchorActive_; }
  bool isSafeStopped() const { return safeStop_; }
  StopReason stopReason() const { return stopReason_; }
  const ControllerTelemetry& telemetry() const { return lastTelemetry_; }

  void step(float anchorX, float anchorY) {
    ControllerTelemetry t;
    const unsigned long now = clock_->now_ms();
    const unsigned long dtMs =
        (lastTickMs_ == 0 || now < lastTickMs_) ? 0 : (now - lastTickMs_);
    lastTickMs_ = now;

    if (cfg_.controlLoopTimeoutMs > 0 && dtMs > cfg_.controlLoopTimeoutMs) {
      triggerSafeStop(now, StopReason::CONTROL_LOOP_TIMEOUT, "CONTROL_LOOP_TIMEOUT");
    }

    GnssFix fix = gnss_->getFix();
    HeadingSample hdg = heading_->getHeading();

    GnssReason gnssReason = evaluateGnss(now, fix);
    const bool gnssGood = (gnssReason == GnssReason::NONE);

    const bool headingFinite =
        hdg.valid && isfinite(hdg.headingDeg) && !isnan(hdg.headingDeg) &&
        hdg.ageMs <= cfg_.sensorTimeoutMs;
    if (!headingFinite) {
      if (sensorBadSinceMs_ == 0) {
        sensorBadSinceMs_ = now;
      }
      if (cfg_.sensorTimeoutMs == 0 ||
          now - sensorBadSinceMs_ >= cfg_.sensorTimeoutMs) {
        triggerSafeStop(now, StopReason::SENSOR_TIMEOUT, "SENSOR_TIMEOUT");
      }
    } else {
      sensorBadSinceMs_ = 0;
    }

    if (hdg.valid && (!isfinite(hdg.headingDeg) || isnan(hdg.headingDeg))) {
      triggerSafeStop(now, StopReason::INTERNAL_ERROR_NAN, "INTERNAL_ERROR_NAN");
      t.nanDetected = true;
    }
    if (fix.valid &&
        ((!isfinite(fix.xM) || !isfinite(fix.yM)) || isnan(fix.xM) || isnan(fix.yM))) {
      triggerSafeStop(now, StopReason::INTERNAL_ERROR_NAN, "INTERNAL_ERROR_NAN");
      t.nanDetected = true;
    }

    if (!safeStop_ && anchorRequested_) {
      if (gnssGood) {
        badGnssSinceMs_ = 0;
        if (goodGnssSinceMs_ == 0) {
          goodGnssSinceMs_ = now;
        }
        if (!anchorActive_ && now - goodGnssSinceMs_ >= cfg_.minGoodTimeToEnterMs) {
          anchorActive_ = true;
          emitEvent(now, "ANCHOR_ACTIVE", "GNSS_QUALITY_OK");
        }
      } else {
        goodGnssSinceMs_ = 0;
        if (anchorActive_) {
          if (badGnssSinceMs_ == 0) {
            badGnssSinceMs_ = now;
          } else if (now - badGnssSinceMs_ >= cfg_.badTimeToExitMs) {
            anchorActive_ = false;
            triggerSafeStop(now, StopReason::GPS_DATA_STALE, gnssReasonStr(gnssReason));
          }
        }
      }
    }

    ActuatorCmd cmd;
    cmd.stop = true;
    cmd.thrust = 0.0f;
    cmd.steerDeg = 0.0f;
    t.rampViolation = false;
    t.commandOutOfRange = false;

    if (anchorActive_ && !safeStop_ && gnssGood && headingFinite) {
      const bool newFixSample = (fix.ageMs == 0);
      if (!filtFixValid_) {
        filtFixX_ = fix.xM;
        filtFixY_ = fix.yM;
        filtFixValid_ = true;
      } else if (newFixSample) {
        const float a = 0.25f;
        filtFixX_ += (fix.xM - filtFixX_) * a;
        filtFixY_ += (fix.yM - filtFixY_) * a;
      }

      const float ctrlX = filtFixValid_ ? filtFixX_ : fix.xM;
      const float ctrlY = filtFixValid_ ? filtFixY_ : fix.yM;
      const float errM = hypotf(anchorX - ctrlX, anchorY - ctrlY);
      float targetThrust = 0.0f;

      float dtSecBias = 0.0f;
      if (lastThrustMs_ != 0 && now > lastThrustMs_) {
        dtSecBias = (now - lastThrustMs_) / 1000.0f;
      }
      dtSecBias = clampf(dtSecBias, 0.0f, 0.25f);
      const float errAbove = std::max(0.0f, errM - cfg_.deadbandM);
      if (errAbove > 0.0f) {
        thrustBias_ += errAbove * 0.22f * dtSecBias;
      } else {
        thrustBias_ -= 0.02f * dtSecBias;
      }
      thrustBias_ = clampf(thrustBias_, 0.0f, cfg_.maxThrust * 0.8f);

      if (errM > cfg_.deadbandM) {
        if (errM >= cfg_.reacquireRadiusM) {
          targetThrust = cfg_.maxThrust;
        } else {
          const float span =
              (cfg_.reacquireRadiusM - cfg_.deadbandM > 0.01f)
                  ? (cfg_.reacquireRadiusM - cfg_.deadbandM)
                  : 0.01f;
          const float r = clampf((errM - cfg_.deadbandM) / span, 0.0f, 1.0f);
          targetThrust = cfg_.minThrust + r * (cfg_.maxThrust - cfg_.minThrust);
        }
        targetThrust = std::max(targetThrust, thrustBias_);
      } else if (thrustBias_ > 0.02f) {
        // Keep a small feedforward thrust against steady current while close.
        targetThrust = thrustBias_ * 0.95f;
      }

      if (lastThrustMs_ == 0) {
        lastThrustMs_ = now;
      }
      float dtSec = (now - lastThrustMs_) / 1000.0f;
      if (!isfinite(dtSec) || dtSec < 0.0f) dtSec = 0.0f;
      if (dtSec > 1.0f) dtSec = 1.0f;
      lastThrustMs_ = now;

      const float maxDelta = cfg_.thrustRampPerS * dtSec;
      const float rawDelta = targetThrust - lastThrust_;
      if (fabsf(rawDelta) > maxDelta + 1e-6f) {
        t.rampViolation = true;
      }
      const float delta = clampf(rawDelta, -maxDelta, maxDelta);
      float thrust = clampf(lastThrust_ + delta, 0.0f, cfg_.maxThrust);
      if (thrust > 0.0f && thrust < cfg_.minThrust) {
        thrust = cfg_.minThrust;
      }
      if (!isfinite(thrust)) {
        thrust = 0.0f;
      }
      lastThrust_ = thrust;

      const float desiredBrg = bearingDeg(ctrlX, ctrlY, anchorX, anchorY);
      const float diff = wrap180f(desiredBrg - hdg.headingDeg);
      const float maxSteer = clampf(cfg_.maxTurnRateDegS, 1.0f, 180.0f);
      const float steer = clampf(diff, -maxSteer, maxSteer);

      cmd.stop = (thrust <= 0.0f);
      cmd.thrust = thrust;
      cmd.steerDeg = clampf(steer, -180.0f, 180.0f);
      if (!isfinite(cmd.thrust) || !isfinite(cmd.steerDeg) || cmd.thrust < 0.0f ||
          cmd.thrust > cfg_.maxThrust + 1e-4f) {
        t.commandOutOfRange = true;
        triggerSafeStop(now, StopReason::COMMAND_OUT_OF_RANGE, "COMMAND_OUT_OF_RANGE");
        cmd.stop = true;
        cmd.thrust = 0.0f;
        cmd.steerDeg = 0.0f;
      }
    } else {
      cmd.stop = true;
      cmd.thrust = 0.0f;
      cmd.steerDeg = 0.0f;
      lastThrust_ = 0.0f;
      lastThrustMs_ = now;
    }

    if (!safeStop_ && cmd.thrust >= cfg_.maxThrust - 0.01f) {
      if (maxThrustSinceMs_ == 0) {
        maxThrustSinceMs_ = now;
      } else if (!maxThrustEventSent_ &&
                 now - maxThrustSinceMs_ >= cfg_.maxContinuousThrustTimeMs) {
        maxThrustEventSent_ = true;
        emitEvent(now, "MAX_CONTINUOUS_THRUST", "LIMIT");
      }
      if (maxThrustEventSent_) {
        cmd.thrust = cfg_.maxThrust * 0.75f;
      }
    } else {
      maxThrustSinceMs_ = 0;
    }

    actuator_->apply(cmd);
    t.gnssGood = gnssGood;
    t.gnssReason = gnssReason;
    t.anchorActive = anchorActive_;
    t.safeStop = safeStop_;
    t.stopReason = stopReason_;
    t.command = cmd;
    lastTelemetry_ = t;
  }

private:
  GnssReason evaluateGnss(unsigned long now, const GnssFix& fix) {
    if (!fix.valid) {
      return GnssReason::NO_FIX;
    }
    if (fix.ageMs > cfg_.maxGpsAgeMs) {
      return GnssReason::DATA_STALE;
    }
    if (fix.sats < cfg_.minSats) {
      return GnssReason::SATS_TOO_LOW;
    }
    if (isfinite(fix.hdop) && fix.hdop > cfg_.maxHdop) {
      return GnssReason::HDOP_TOO_HIGH;
    }
    if (!isfinite(fix.xM) || !isfinite(fix.yM)) {
      return GnssReason::DATA_STALE;
    }

    const bool newFixSample = (fix.ageMs == 0);
    if (newFixSample && prevFixValid_ && now > prevFixMs_) {
      const float jump = hypotf(fix.xM - prevFixX_, fix.yM - prevFixY_);
      if (jump > cfg_.maxPositionJumpM) {
        // Advance reference even on rejected sample to avoid endless jump storms
        // after a long GNSS-quality outage.
        prevFixX_ = fix.xM;
        prevFixY_ = fix.yM;
        prevFixMs_ = now;
        emitEvent(now, "POSITION_JUMP_DETECTED", "GPS_POSITION_JUMP");
        return GnssReason::POSITION_JUMP;
      }
      const float dt = (now - prevFixMs_) / 1000.0f;
      // Speed sanity from position deltas is too noisy on short GNSS periods
      // (e.g. 5 Hz with ~1 m sigma). Validate speed only on >=1s baseline.
      if (dt >= 1.0f) {
        const float speed = jump / dt;
        if (speed > cfg_.speedSanityMaxMps) {
          return GnssReason::SPEED_INVALID;
        }
      }
    }

    if (newFixSample || !prevFixValid_) {
      prevFixValid_ = true;
      prevFixX_ = fix.xM;
      prevFixY_ = fix.yM;
      prevFixMs_ = now;
    }
    return GnssReason::NONE;
  }

  void emitEvent(unsigned long now, const char* code, const char* details) {
    if (sink_) {
      sink_->onControlEvent(now, code, details);
    }
  }

  void triggerSafeStop(unsigned long now, StopReason reason, const char* details) {
    if (safeStop_) {
      return;
    }
    safeStop_ = true;
    anchorActive_ = false;
    stopReason_ = reason;
    emitEvent(now, details, "FAILSAFE_TRIGGERED");
    emitEvent(now, "FAILSAFE_TRIGGERED", stopReasonStr(reason));
    applyStop();
  }

  void applyStop() {
    ActuatorCmd cmd;
    cmd.stop = true;
    cmd.thrust = 0.0f;
    cmd.steerDeg = 0.0f;
    actuator_->apply(cmd);
  }

private:
  IGnssSource* gnss_ = nullptr;
  IHeadingSource* heading_ = nullptr;
  IActuator* actuator_ = nullptr;
  IClock* clock_ = nullptr;
  IControlEventSink* sink_ = nullptr;
  ControllerConfig cfg_;

  bool anchorRequested_ = true;
  bool anchorActive_ = false;
  bool safeStop_ = false;
  StopReason stopReason_ = StopReason::NONE;

  unsigned long goodGnssSinceMs_ = 0;
  unsigned long badGnssSinceMs_ = 0;
  unsigned long sensorBadSinceMs_ = 0;
  unsigned long lastTickMs_ = 0;
  float lastThrust_ = 0.0f;
  unsigned long lastThrustMs_ = 0;
  float thrustBias_ = 0.0f;
  unsigned long maxThrustSinceMs_ = 0;
  bool maxThrustEventSent_ = false;

  bool prevFixValid_ = false;
  float prevFixX_ = 0.0f;
  float prevFixY_ = 0.0f;
  unsigned long prevFixMs_ = 0;
  bool filtFixValid_ = false;
  float filtFixX_ = 0.0f;
  float filtFixY_ = 0.0f;

  ControllerTelemetry lastTelemetry_;
};

} // namespace simctl
