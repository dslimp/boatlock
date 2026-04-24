#pragma once

#include <Arduino.h>
#include <stdint.h>

#include <string>

struct RuntimeCompassRecoveryOutcome {
  bool attempted = false;
  bool ready = false;
  bool shouldReloadCalibration = false;
  bool shouldLogInventory = false;
  int initAttempts = 0;
};

template <typename RestartBusFn, typename InitFn>
inline RuntimeCompassRecoveryOutcome attemptRuntimeCompassRecovery(bool shouldRetry,
                                                                   RestartBusFn restartBus,
                                                                   InitFn initSensor) {
  RuntimeCompassRecoveryOutcome out;
  if (!shouldRetry) {
    return out;
  }

  out.attempted = true;
  restartBus();
  out.initAttempts = 1;
  out.ready = initSensor();
  if (!out.ready) {
    restartBus();
    ++out.initAttempts;
    out.ready = initSensor();
  }
  out.shouldReloadCalibration = out.ready;
  out.shouldLogInventory = !out.ready;
  return out;
}

inline std::string buildRuntimeCompassRecoveryStatusLine(bool ready,
                                                         const char* sourceName,
                                                         uint8_t address) {
  char buffer[96];
  if (ready) {
    snprintf(buffer,
             sizeof(buffer),
             "[COMPASS] retry ready=%d source=%s addr=0x%02X",
             ready ? 1 : 0,
             sourceName ? sourceName : "UNKNOWN",
             address);
  } else {
    snprintf(buffer,
             sizeof(buffer),
             "[COMPASS] retry ready=%d source=%s",
             ready ? 1 : 0,
             sourceName ? sourceName : "UNKNOWN");
  }
  return std::string(buffer);
}

inline std::string buildRuntimeCompassOffsetLine(float headingOffsetDeg) {
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "[COMPASS] heading offset=%.1f", headingOffsetDeg);
  return std::string(buffer);
}
