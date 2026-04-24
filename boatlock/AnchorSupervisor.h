#pragma once
#include <Arduino.h>

class AnchorSupervisor {
public:
  static constexpr unsigned long kMinCommTimeoutMs = 3000;
  static constexpr unsigned long kMinControlLoopTimeoutMs = 100;
  static constexpr unsigned long kMinSensorTimeoutMs = 300;
  static constexpr unsigned long kMinGpsWeakGraceMs = 500;

  enum class SafeAction : uint8_t {
    NONE = 0,
    STOP = 1,
    EXIT_ANCHOR = 2,
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
    gpsWeakActive_ = false;
    gpsWeakSinceMs_ = 0;
    sensorBadActive_ = false;
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

    if (in.controlActivitySeen) {
      noteControlActivity(in.nowMs);
    }

    if (in.containmentBreached) {
      return decision(SafeAction::EXIT_ANCHOR, Reason::CONTAINMENT_BREACH);
    }

    if (in.hasNaN) {
      return decision(SafeAction::STOP, Reason::INTERNAL_ERROR_NAN);
    }

    const int maxCommandThrustPct = effectiveMaxCommandThrustPct(cfg);
    if (in.commandThrustPct > maxCommandThrustPct ||
        in.commandThrustPct < -maxCommandThrustPct) {
      return decision(SafeAction::STOP, Reason::COMMAND_OUT_OF_RANGE);
    }

    if (!in.linkOk) {
      return decision(SafeAction::STOP, Reason::COMM_TIMEOUT);
    }

    if ((controlActivitySeen_ || in.controlActivitySeen) &&
        elapsedMs(in.nowMs, lastControlActivityMs_) >= effectiveCommTimeoutMs(cfg)) {
      return decision(SafeAction::STOP, Reason::COMM_TIMEOUT);
    }

    if (in.controlLoopDtMs > effectiveControlLoopTimeoutMs(cfg)) {
      return decision(SafeAction::STOP, Reason::CONTROL_LOOP_TIMEOUT);
    }

    if (!in.gpsQualityOk) {
      if (!gpsWeakActive_) {
        gpsWeakActive_ = true;
        gpsWeakSinceMs_ = in.nowMs;
      } else if (elapsedMs(in.nowMs, gpsWeakSinceMs_) >= effectiveGpsWeakGraceMs(cfg)) {
        return decision(SafeAction::EXIT_ANCHOR, Reason::GPS_WEAK);
      }
    } else {
      gpsWeakActive_ = false;
      gpsWeakSinceMs_ = 0;
    }

    if (!in.sensorsOk) {
      if (!sensorBadActive_) {
        sensorBadActive_ = true;
        sensorBadSinceMs_ = in.nowMs;
      } else if (elapsedMs(in.nowMs, sensorBadSinceMs_) >= effectiveSensorTimeoutMs(cfg)) {
        return decision(SafeAction::STOP, Reason::SENSOR_TIMEOUT);
      }
    } else {
      sensorBadActive_ = false;
      sensorBadSinceMs_ = 0;
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
  static unsigned long elapsedMs(unsigned long nowMs, unsigned long thenMs) {
    return nowMs - thenMs;
  }

  static unsigned long floorTimeoutMs(unsigned long value, unsigned long floorMs) {
    return value < floorMs ? floorMs : value;
  }

  static unsigned long effectiveCommTimeoutMs(const Config& cfg) {
    return floorTimeoutMs(cfg.commTimeoutMs, kMinCommTimeoutMs);
  }

  static unsigned long effectiveControlLoopTimeoutMs(const Config& cfg) {
    return floorTimeoutMs(cfg.controlLoopTimeoutMs, kMinControlLoopTimeoutMs);
  }

  static unsigned long effectiveSensorTimeoutMs(const Config& cfg) {
    return floorTimeoutMs(cfg.sensorTimeoutMs, kMinSensorTimeoutMs);
  }

  static unsigned long effectiveGpsWeakGraceMs(const Config& cfg) {
    return floorTimeoutMs(cfg.gpsWeakGraceMs, kMinGpsWeakGraceMs);
  }

  static int effectiveMaxCommandThrustPct(const Config& cfg) {
    if (cfg.maxCommandThrustPct < 0) {
      return 0;
    }
    if (cfg.maxCommandThrustPct > 100) {
      return 100;
    }
    return cfg.maxCommandThrustPct;
  }

  static Decision decision(SafeAction action, Reason reason) {
    Decision d;
    d.action = action;
    d.reason = reason;
    return d;
  }

  bool gpsWeakActive_ = false;
  unsigned long gpsWeakSinceMs_ = 0;
  bool sensorBadActive_ = false;
  unsigned long sensorBadSinceMs_ = 0;
  unsigned long lastControlActivityMs_ = 0;
  bool controlActivitySeen_ = false;
};
