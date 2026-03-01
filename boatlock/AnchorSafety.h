#pragma once
#include <Arduino.h>

class AnchorSafety {
public:
  enum Action {
    NONE = 0,
    EXIT_GPS_WEAK,
    FAILSAFE_LINK_LOST,
    FAILSAFE_HEARTBEAT_TIMEOUT,
  };

  void reset() {
    gpsWeakSinceMs_ = 0;
  }

  void notifyControlActivity(unsigned long nowMs) {
    lastControlActivityMs_ = nowMs;
    controlActivitySeen_ = true;
  }

  Action update(unsigned long nowMs,
                bool anchorActive,
                bool gpsQualityOk,
                bool bleConnected,
                unsigned long heartbeatTimeoutMs,
                unsigned long gpsWeakGraceMs) {
    if (!anchorActive) {
      reset();
      return NONE;
    }

    if (!gpsQualityOk) {
      if (gpsWeakSinceMs_ == 0) {
        gpsWeakSinceMs_ = nowMs;
      } else if (nowMs - gpsWeakSinceMs_ >= gpsWeakGraceMs) {
        return EXIT_GPS_WEAK;
      }
    } else {
      gpsWeakSinceMs_ = 0;
    }

    if (!bleConnected) {
      return FAILSAFE_LINK_LOST;
    }

    if (controlActivitySeen_ &&
        heartbeatTimeoutMs > 0 &&
        nowMs - lastControlActivityMs_ > heartbeatTimeoutMs) {
      return FAILSAFE_HEARTBEAT_TIMEOUT;
    }

    return NONE;
  }

private:
  unsigned long gpsWeakSinceMs_ = 0;
  unsigned long lastControlActivityMs_ = 0;
  bool controlActivitySeen_ = false;
};
