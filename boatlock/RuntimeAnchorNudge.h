#pragma once

#include <math.h>
#include <strings.h>

#include "RuntimeGnss.h"

struct RuntimeAnchorNudgeTarget {
  float lat = 0.0f;
  float lon = 0.0f;
};

inline bool runtimeAnchorNudgeRangeValid(float meters) {
  return isfinite(meters) && meters >= 1.0f && meters <= 5.0f;
}

inline bool resolveRuntimeCardinalNudgeBearing(const char* dir, float headingDeg, float* bearingDeg) {
  if (!dir || !bearingDeg || !isfinite(headingDeg)) {
    return false;
  }

  if (strcasecmp(dir, "FWD") == 0 || strcasecmp(dir, "FORWARD") == 0) {
    *bearingDeg = headingDeg;
    return true;
  }
  if (strcasecmp(dir, "BACK") == 0) {
    *bearingDeg = headingDeg + 180.0f;
    return true;
  }
  if (strcasecmp(dir, "LEFT") == 0) {
    *bearingDeg = headingDeg - 90.0f;
    return true;
  }
  if (strcasecmp(dir, "RIGHT") == 0) {
    *bearingDeg = headingDeg + 90.0f;
    return true;
  }
  return false;
}

inline bool projectRuntimeAnchorNudge(float lat,
                                      float lon,
                                      float bearingDeg,
                                      float meters,
                                      RuntimeAnchorNudgeTarget* target) {
  if (!target || !isfinite(lat) || !isfinite(lon) || !isfinite(bearingDeg) ||
      !runtimeAnchorNudgeRangeValid(meters)) {
    return false;
  }

  const double lat1 = lat * (M_PI / 180.0);
  const double lon1 = lon * (M_PI / 180.0);
  const double bearingRad = RuntimeGnss::normalize360Deg(bearingDeg) * (M_PI / 180.0);
  const double distRatio = meters / 6378137.0;

  const double sinLat2 = sin(lat1) * cos(distRatio) + cos(lat1) * sin(distRatio) * cos(bearingRad);
  const double lat2 = asin(sinLat2);
  const double lon2 = lon1 + atan2(sin(bearingRad) * sin(distRatio) * cos(lat1),
                                   cos(distRatio) - sin(lat1) * sin(lat2));

  target->lat = (float)(lat2 * 180.0 / M_PI);
  target->lon = (float)(lon2 * 180.0 / M_PI);
  return true;
}
