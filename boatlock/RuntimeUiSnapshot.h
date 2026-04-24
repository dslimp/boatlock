#pragma once

#include <algorithm>
#include <cmath>

#include "HilSimRunner.h"

struct RuntimeUiSnapshot {
  bool gpsFix = false;
  int satellites = 0;
  bool gpsFromPhone = false;
  float gpsHdop = NAN;
  float speedKmh = 0.0f;
  float heading = 0.0f;
  bool headingValid = false;
  bool headingFromPhone = false;
  int compassQuality = 0;
  float bearing = 0.0f;
  float distanceM = 0.0f;
  float diffDeg = 0.0f;
  int motorPwmPercent = 0;
};

inline float normalizeUiDiffDeg(float targetDeg, float currentDeg) {
  float diff = std::fmod(targetDeg - currentDeg, 360.0f);
  if (diff > 180.0f) {
    diff -= 360.0f;
  } else if (diff < -180.0f) {
    diff += 360.0f;
  }
  return diff;
}

inline int clampUiPercent(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 100) {
    return 100;
  }
  return value;
}

inline RuntimeUiSnapshot buildRuntimeUiSnapshot(bool gpsFix,
                                                int satellites,
                                                bool gpsFromPhone,
                                                float gpsHdop,
                                                float speedKmh,
                                                float heading,
                                                bool headingValid,
                                                bool headingFromPhone,
                                                int compassQuality,
                                                float bearing,
                                                float distanceM,
                                                float diffDeg,
                                                int motorPwmPercent) {
  RuntimeUiSnapshot snapshot;
  snapshot.gpsFix = gpsFix;
  snapshot.satellites = satellites;
  snapshot.gpsFromPhone = gpsFromPhone;
  snapshot.gpsHdop = gpsHdop;
  snapshot.speedKmh = speedKmh;
  snapshot.heading = heading;
  snapshot.headingValid = headingValid;
  snapshot.headingFromPhone = headingFromPhone;
  snapshot.compassQuality = compassQuality;
  snapshot.bearing = bearing;
  snapshot.distanceM = distanceM;
  snapshot.diffDeg = diffDeg;
  snapshot.motorPwmPercent = motorPwmPercent;
  return snapshot;
}

inline bool applyHilSimUiSnapshot(RuntimeUiSnapshot* snapshot,
                                  const hilsim::HilScenarioRunner::LiveTelemetry& live,
                                  float fallbackHeading) {
  if (!snapshot || !live.valid) {
    return false;
  }

  snapshot->gpsFix = live.gnss.valid;
  snapshot->satellites = live.gnss.sats;
  snapshot->gpsFromPhone = false;
  snapshot->gpsHdop = live.gnss.hdop;
  snapshot->speedKmh = live.speedMps * 3.6f;
  snapshot->headingValid =
      live.heading.valid && std::isfinite(live.heading.headingDeg) && !std::isnan(live.heading.headingDeg);
  snapshot->heading = snapshot->headingValid ? live.heading.headingDeg : fallbackHeading;
  snapshot->headingFromPhone = false;
  snapshot->compassQuality = snapshot->headingValid ? 3 : 0;
  snapshot->bearing = live.bearingDeg;
  snapshot->distanceM = live.errTrueM;
  snapshot->diffDeg =
      snapshot->headingValid ? normalizeUiDiffDeg(snapshot->bearing, snapshot->heading) : 0.0f;
  snapshot->motorPwmPercent = clampUiPercent((int)std::lround(live.command.thrust * 100.0f));
  return true;
}
