#pragma once

#include "AnchorReasons.h"

inline AnchorDeniedReason resolveRuntimeAnchorEnableDeniedReason(bool anchorPointReady,
                                                                 bool headingReady,
                                                                 AnchorDeniedReason gnssDeniedReason) {
  if (!anchorPointReady) {
    return AnchorDeniedReason::NO_ANCHOR_POINT;
  }
  if (!headingReady) {
    return AnchorDeniedReason::NO_HEADING;
  }
  return gnssDeniedReason;
}
