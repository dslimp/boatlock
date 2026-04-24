#pragma once

#include <math.h>
#include <strings.h>

struct RuntimeAnchorNudgeTarget {
  float lat = 0.0f;
  float lon = 0.0f;
};

static constexpr float kRuntimeAnchorNudgeMeters = 1.5f;

inline float normalizeRuntimeAnchorNudgeBearing(float deg) {
  const float normalized = fmodf(deg, 360.0f);
  return normalized < 0.0f ? normalized + 360.0f : normalized;
}

inline float normalizeRuntimeAnchorNudgeLon(float lon) {
  const float normalized = fmodf(lon + 180.0f, 360.0f);
  const float wrapped = (normalized < 0.0f ? normalized + 360.0f : normalized) - 180.0f;
  return wrapped == -180.0f && lon > 0.0f ? 180.0f : wrapped;
}

inline bool runtimeAnchorNudgePointValid(float lat, float lon) {
  return isfinite(lat) &&
         isfinite(lon) &&
         lat >= -90.0f &&
         lat <= 90.0f &&
         lon >= -180.0f &&
         lon <= 180.0f &&
         !(lat == 0.0f && lon == 0.0f);
}

inline bool resolveRuntimeCardinalNudgeBearing(const char* dir, float headingDeg, float* bearingDeg) {
  if (!dir || !bearingDeg || !isfinite(headingDeg)) {
    return false;
  }

  if (strcasecmp(dir, "FWD") == 0 || strcasecmp(dir, "FORWARD") == 0) {
    *bearingDeg = normalizeRuntimeAnchorNudgeBearing(headingDeg);
    return true;
  }
  if (strcasecmp(dir, "BACK") == 0) {
    *bearingDeg = normalizeRuntimeAnchorNudgeBearing(headingDeg + 180.0f);
    return true;
  }
  if (strcasecmp(dir, "LEFT") == 0) {
    *bearingDeg = normalizeRuntimeAnchorNudgeBearing(headingDeg - 90.0f);
    return true;
  }
  if (strcasecmp(dir, "RIGHT") == 0) {
    *bearingDeg = normalizeRuntimeAnchorNudgeBearing(headingDeg + 90.0f);
    return true;
  }
  return false;
}

inline bool projectRuntimeAnchorNudge(float lat,
                                      float lon,
                                      float bearingDeg,
                                      RuntimeAnchorNudgeTarget* target) {
  if (!target || !runtimeAnchorNudgePointValid(lat, lon) || !isfinite(bearingDeg)) {
    return false;
  }

  const double lat1 = lat * (M_PI / 180.0);
  const double lon1 = lon * (M_PI / 180.0);
  const double bearingRad = normalizeRuntimeAnchorNudgeBearing(bearingDeg) * (M_PI / 180.0);
  const double distRatio = kRuntimeAnchorNudgeMeters / 6378137.0;

  const double sinLat2 = sin(lat1) * cos(distRatio) + cos(lat1) * sin(distRatio) * cos(bearingRad);
  const double lat2 = asin(sinLat2);
  const double lon2 = lon1 + atan2(sin(bearingRad) * sin(distRatio) * cos(lat1),
                                   cos(distRatio) - sin(lat1) * sin(lat2));

  target->lat = (float)(lat2 * 180.0 / M_PI);
  target->lon = normalizeRuntimeAnchorNudgeLon((float)(lon2 * 180.0 / M_PI));
  return runtimeAnchorNudgePointValid(target->lat, target->lon);
}
