#pragma once

#include <math.h>
#include <stdint.h>

enum class CoreMode : uint8_t {
  IDLE = 0,
  SAFE_HOLD = 1,
  ANCHOR = 2,
  MANUAL = 3,
  SIM = 4,
};

inline CoreMode resolveCoreMode(bool simRunning,
                                bool manualMode,
                                bool anchorEnabled,
                                bool safeHoldActive) {
  if (simRunning) {
    return CoreMode::SIM;
  }
  if (manualMode) {
    return CoreMode::MANUAL;
  }
  if (anchorEnabled) {
    return CoreMode::ANCHOR;
  }
  if (safeHoldActive) {
    return CoreMode::SAFE_HOLD;
  }
  return CoreMode::IDLE;
}

inline const char* coreModeLabel(CoreMode mode) {
  switch (mode) {
    case CoreMode::SIM:
      return "SIM";
    case CoreMode::MANUAL:
      return "MANUAL";
    case CoreMode::ANCHOR:
      return "ANCHOR";
    case CoreMode::SAFE_HOLD:
      return "HOLD";
    case CoreMode::IDLE:
    default:
      return "IDLE";
  }
}

inline bool coreModeUsesAnchorControl(CoreMode mode) {
  return mode == CoreMode::ANCHOR;
}

struct RuntimeControlInput {
  unsigned long nowMs = 0;
  CoreMode mode = CoreMode::IDLE;
  int manualDir = -1;
  int manualSpeed = 0;
  bool controlGpsAvailable = false;
  bool hasHeading = false;
  bool hasBearing = false;
  float bearingDeg = 0.0f;
  float headingDeg = 0.0f;
  float diffDeg = 0.0f;
  bool compassReady = false;
  float rvAccuracyDeg = NAN;
  int rvQuality = 0;
  float distanceM = 0.0f;
};
