#pragma once

#include <math.h>

inline float displayNormalize360Deg(float deg) {
  if (!isfinite(deg)) {
    return 0.0f;
  }
  float wrapped = fmodf(deg, 360.0f);
  if (wrapped < 0.0f) {
    wrapped += 360.0f;
  }
  return wrapped;
}

inline int displayNormalize360Int(float deg) {
  return static_cast<int>(roundf(displayNormalize360Deg(deg))) % 360;
}

inline float displayCompassCardScreenDeg(float worldDeg, float headingDeg) {
  return displayNormalize360Deg(worldDeg + headingDeg);
}

inline float displayAnchorArrowScreenDeg(float anchorBearingDeg, float headingDeg) {
  return displayNormalize360Deg(anchorBearingDeg - headingDeg);
}
