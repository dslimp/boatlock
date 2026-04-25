#pragma once

namespace hilsim {

inline bool simTimeWindowContains(unsigned long nowMs,
                                  unsigned long atMs,
                                  unsigned long durationMs) {
  if (durationMs == 0) {
    return false;
  }
  return (unsigned long)(nowMs - atMs) < durationMs;
}

}  // namespace hilsim
