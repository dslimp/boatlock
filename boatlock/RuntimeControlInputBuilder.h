#pragma once

#include <math.h>

#include "RuntimeControl.h"
#include "RuntimeGnss.h"

struct RuntimeControlState {
  CoreMode mode = CoreMode::IDLE;
  bool autoControlActive = false;
  bool hasHeading = false;
  bool hasBearing = false;
  float headingDeg = 0.0f;
  float diffDeg = 0.0f;
  RuntimeControlInput input;
};

inline RuntimeControlState buildRuntimeControlState(unsigned long nowMs,
                                                    CoreMode mode,
                                                    int manualDir,
                                                    int manualSpeed,
                                                    bool controlGpsAvailable,
                                                    bool headingAvailable,
                                                    float headingDeg,
                                                    bool bearingAvailable,
                                                    float bearingDeg,
                                                    bool compassReady,
                                                    float rvAccuracyDeg,
                                                    int rvQuality,
                                                    float distanceM) {
  RuntimeControlState state;
  state.mode = mode;
  state.autoControlActive = coreModeUsesAnchorControl(mode);
  state.hasHeading = headingAvailable;
  state.headingDeg = headingDeg;
  state.hasBearing = state.autoControlActive && bearingAvailable;
  state.diffDeg =
      (state.hasHeading && state.hasBearing) ? RuntimeGnss::normalizeDiffDeg(bearingDeg, headingDeg) : 0.0f;

  state.input.nowMs = nowMs;
  state.input.mode = mode;
  state.input.manualDir = manualDir;
  state.input.manualSpeed = manualSpeed;
  state.input.controlGpsAvailable = controlGpsAvailable;
  state.input.hasHeading = state.hasHeading;
  state.input.hasBearing = state.hasBearing;
  state.input.bearingDeg = bearingDeg;
  state.input.headingDeg = headingDeg;
  state.input.diffDeg = state.diffDeg;
  state.input.compassReady = compassReady;
  state.input.rvAccuracyDeg = rvAccuracyDeg;
  state.input.rvQuality = rvQuality;
  state.input.distanceM = distanceM;
  return state;
}
