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
    CONTAINMENT_BREACH,
    COMM_TIMEOUT,
    CONTROL_LOOP_TIMEOUT,
    SENSOR_TIMEOUT,
    INTERNAL_ERROR_NAN,
    COMMAND_OUT_OF_RANGE,
  };

  struct Config {
    unsigned long commTimeoutMs = 7000;
    unsigned long controlLoopTimeoutMs = 600;
    unsigned long sensorTimeoutMs = 3000;
    unsigned long gpsWeakGraceMs = 5000;
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
    bool containmentBreached = false;
    bool controlActivitySeen = false;
    unsigned long controlLoopDtMs = 0;
    int commandThrustPct = 0;
  };

  struct Decision {
    SafeAction action = SafeAction::NONE;
    Reason reason = Reason::NONE;
  };

  void reset() {
    gpsWeakSinceMs_ = 0;
    sensorBadSinceMs_ = 0;
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

    if (!in.gpsQualityOk) {
      if (gpsWeakSinceMs_ == 0) {
        gpsWeakSinceMs_ = in.nowMs;
      } else if (in.nowMs - gpsWeakSinceMs_ >= cfg.gpsWeakGraceMs) {
        Decision d;
        d.action = SafeAction::EXIT_ANCHOR;
        d.reason = Reason::GPS_WEAK;
        return d;
      }
    } else {
      gpsWeakSinceMs_ = 0;
    }

    if (in.containmentBreached) {
      Decision d;
      d.action = SafeAction::EXIT_ANCHOR;
      d.reason = Reason::CONTAINMENT_BREACH;
      return d;
    }

    if (!in.linkOk) {
      Decision d;
      d.action = cfg.failsafeAction;
      d.reason = Reason::COMM_TIMEOUT;
      return d;
    }

    if ((controlActivitySeen_ || in.controlActivitySeen) &&
        cfg.commTimeoutMs > 0 &&
        in.nowMs - lastControlActivityMs_ > cfg.commTimeoutMs) {
      Decision d;
      d.action = cfg.failsafeAction;
      d.reason = Reason::COMM_TIMEOUT;
      return d;
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
      } else if (cfg.sensorTimeoutMs > 0 &&
                 in.nowMs - sensorBadSinceMs_ >= cfg.sensorTimeoutMs) {
        Decision d;
        d.action = cfg.failsafeAction;
        d.reason = Reason::SENSOR_TIMEOUT;
        return d;
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
      case Reason::CONTAINMENT_BREACH:
        return "CONTAINMENT_BREACH";
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
  unsigned long lastControlActivityMs_ = 0;
  bool controlActivitySeen_ = false;
};
