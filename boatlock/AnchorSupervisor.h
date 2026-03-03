#pragma once
#include <Arduino.h>

class AnchorSupervisor {
public:
  enum class SafeAction : uint8_t {
    NONE = 0,
    STOP = 1,
    MANUAL = 2,
    EXIT_ANCHOR = 3,
  };

  enum class Reason : uint8_t {
    NONE = 0,
    GPS_WEAK,
    COMM_TIMEOUT,
    CONTROL_LOOP_TIMEOUT,
    SENSOR_TIMEOUT,
    INTERNAL_ERROR_NAN,
    COMMAND_OUT_OF_RANGE,
  };

  struct Config {
    unsigned long commTimeoutMs = 7000;
    unsigned long linkLossGraceMs = 0;
    unsigned long controlLoopTimeoutMs = 600;
    unsigned long sensorTimeoutMs = 3000;
    unsigned long gpsWeakGraceMs = 5000;
    unsigned long nonExtremeExtraMs = 0;
    float softeningDistanceM = 12.0f;
    int softeningMaxThrustPct = 45;
    SafeAction failsafeAction = SafeAction::STOP;
    SafeAction nanGuardAction = SafeAction::STOP;
    int maxCommandThrustPct = 100;
  };

  struct Input {
    unsigned long nowMs = 0;
    bool anchorActive = false;
    bool gpsQualityOk = true;
    bool linkOk = true;
    bool sensorsOk = true;
    bool hasNaN = false;
    bool controlActivitySeen = false;
    unsigned long controlLoopDtMs = 0;
    int commandThrustPct = 0;
    float distanceM = -1.0f;
    bool driftAlertActive = false;
    bool driftFailActive = false;
  };

  struct Decision {
    SafeAction action = SafeAction::NONE;
    Reason reason = Reason::NONE;
    bool softened = false;
    unsigned long observedMs = 0;
    unsigned long thresholdMs = 0;
  };

  void reset() {
    gpsWeakSinceMs_ = 0;
    sensorBadSinceMs_ = 0;
    linkBadSinceMs_ = 0;
    lastControlActivityMs_ = 0;
    controlActivitySeen_ = false;
  }

  void noteControlActivity(unsigned long nowMs) {
    lastControlActivityMs_ = nowMs;
    controlActivitySeen_ = true;
  }

  Decision update(const Config& cfg, const Input& in) {
    if (!in.anchorActive) {
      reset();
      return {};
    }

    const bool softeningEligible =
        in.gpsQualityOk &&
        in.sensorsOk &&
        !in.hasNaN &&
        !in.driftFailActive &&
        !in.driftAlertActive &&
        isfinite(in.distanceM) &&
        in.distanceM >= 0.0f &&
        in.distanceM <= cfg.softeningDistanceM &&
        abs(in.commandThrustPct) <= cfg.softeningMaxThrustPct;
    const unsigned long extraMs = softeningEligible ? cfg.nonExtremeExtraMs : 0;

    if (!in.gpsQualityOk) {
      if (gpsWeakSinceMs_ == 0) {
        gpsWeakSinceMs_ = in.nowMs;
      } else if (in.nowMs - gpsWeakSinceMs_ >= cfg.gpsWeakGraceMs) {
        Decision d;
        d.action = SafeAction::EXIT_ANCHOR;
        d.reason = Reason::GPS_WEAK;
        d.observedMs = in.nowMs - gpsWeakSinceMs_;
        d.thresholdMs = cfg.gpsWeakGraceMs;
        return d;
      }
    } else {
      gpsWeakSinceMs_ = 0;
    }

    if (!in.linkOk) {
      const unsigned long linkLossThresholdMs = cfg.linkLossGraceMs + extraMs;
      if (linkLossThresholdMs == 0) {
        Decision d;
        d.action = cfg.failsafeAction;
        d.reason = Reason::COMM_TIMEOUT;
        d.softened = (extraMs > 0);
        d.observedMs = 0;
        d.thresholdMs = 0;
        return d;
      }
      if (linkBadSinceMs_ == 0) {
        linkBadSinceMs_ = in.nowMs;
      } else {
        const unsigned long linkBadAgeMs = in.nowMs - linkBadSinceMs_;
        if (linkLossThresholdMs == 0 || linkBadAgeMs >= linkLossThresholdMs) {
          Decision d;
          d.action = cfg.failsafeAction;
          d.reason = Reason::COMM_TIMEOUT;
          d.softened = (extraMs > 0);
          d.observedMs = linkBadAgeMs;
          d.thresholdMs = linkLossThresholdMs;
          return d;
        }
      }
    } else {
      linkBadSinceMs_ = 0;
    }

    if ((controlActivitySeen_ || in.controlActivitySeen) &&
        cfg.commTimeoutMs > 0) {
      const unsigned long commThresholdMs = cfg.commTimeoutMs + extraMs;
      const unsigned long commAgeMs = in.nowMs - lastControlActivityMs_;
      if (commAgeMs > commThresholdMs) {
        Decision d;
        d.action = cfg.failsafeAction;
        d.reason = Reason::COMM_TIMEOUT;
        d.softened = (extraMs > 0);
        d.observedMs = commAgeMs;
        d.thresholdMs = commThresholdMs;
        return d;
      }
    }

    if (cfg.controlLoopTimeoutMs > 0 &&
        in.controlLoopDtMs > cfg.controlLoopTimeoutMs) {
      Decision d;
      d.action = cfg.failsafeAction;
      d.reason = Reason::CONTROL_LOOP_TIMEOUT;
      return d;
    }

    if (!in.sensorsOk) {
      if (sensorBadSinceMs_ == 0) {
        sensorBadSinceMs_ = in.nowMs;
      } else if (cfg.sensorTimeoutMs > 0) {
        const unsigned long sensorThresholdMs = cfg.sensorTimeoutMs + extraMs;
        const unsigned long sensorBadAgeMs = in.nowMs - sensorBadSinceMs_;
        if (sensorBadAgeMs >= sensorThresholdMs) {
          Decision d;
          d.action = cfg.failsafeAction;
          d.reason = Reason::SENSOR_TIMEOUT;
          d.softened = (extraMs > 0);
          d.observedMs = sensorBadAgeMs;
          d.thresholdMs = sensorThresholdMs;
          return d;
        }
      }
    } else {
      sensorBadSinceMs_ = 0;
    }

    if (in.hasNaN) {
      Decision d;
      d.action = cfg.nanGuardAction;
      d.reason = Reason::INTERNAL_ERROR_NAN;
      return d;
    }

    if (in.commandThrustPct > cfg.maxCommandThrustPct ||
        in.commandThrustPct < -cfg.maxCommandThrustPct) {
      Decision d;
      d.action = cfg.failsafeAction;
      d.reason = Reason::COMMAND_OUT_OF_RANGE;
      return d;
    }

    return {};
  }

  static const char* reasonString(Reason reason) {
    switch (reason) {
      case Reason::GPS_WEAK:
        return "GPS_WEAK";
      case Reason::COMM_TIMEOUT:
        return "COMM_TIMEOUT";
      case Reason::CONTROL_LOOP_TIMEOUT:
        return "CONTROL_LOOP_TIMEOUT";
      case Reason::SENSOR_TIMEOUT:
        return "SENSOR_TIMEOUT";
      case Reason::INTERNAL_ERROR_NAN:
        return "INTERNAL_ERROR_NAN";
      case Reason::COMMAND_OUT_OF_RANGE:
        return "COMMAND_OUT_OF_RANGE";
      case Reason::NONE:
      default:
        return "NONE";
    }
  }

private:
  unsigned long gpsWeakSinceMs_ = 0;
  unsigned long sensorBadSinceMs_ = 0;
  unsigned long linkBadSinceMs_ = 0;
  unsigned long lastControlActivityMs_ = 0;
  bool controlActivitySeen_ = false;
};
