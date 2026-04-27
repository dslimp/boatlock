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
                                                    float manualAngleDeg,
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
  state.hasHeading = headingAvailable && isfinite(headingDeg);
  state.headingDeg = state.hasHeading ? headingDeg : 0.0f;
  state.hasBearing = state.autoControlActive && bearingAvailable && isfinite(bearingDeg);
  state.diffDeg =
      (state.hasHeading && state.hasBearing) ? RuntimeGnss::normalizeDiffDeg(bearingDeg, state.headingDeg) : 0.0f;
  const bool distanceValid = isfinite(distanceM) && distanceM >= 0.0f;
  const bool controlGpsUsable = controlGpsAvailable && (!state.autoControlActive || distanceValid);
  const bool rvAccuracyValid = isfinite(rvAccuracyDeg) && rvAccuracyDeg >= 0.0f;
  const int safeRvQuality = (rvQuality >= 0 && rvQuality <= 3) ? rvQuality : 0;

  state.input.nowMs = nowMs;
  state.input.mode = mode;
  state.input.manualAngleDeg = isfinite(manualAngleDeg) ? manualAngleDeg : 0.0f;
  state.input.manualSpeed = manualSpeed;
  state.input.controlGpsAvailable = controlGpsUsable;
  state.input.hasHeading = state.hasHeading;
  state.input.hasBearing = state.hasBearing;
  state.input.bearingDeg = state.hasBearing ? bearingDeg : 0.0f;
  state.input.headingDeg = state.headingDeg;
  state.input.diffDeg = state.diffDeg;
  state.input.compassReady = compassReady;
  state.input.rvAccuracyDeg = rvAccuracyValid ? rvAccuracyDeg : NAN;
  state.input.rvQuality = safeRvQuality;
  state.input.distanceM = distanceValid ? distanceM : 0.0f;
  return state;
}
