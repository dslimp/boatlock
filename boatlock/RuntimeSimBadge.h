#pragma once

#include <stdio.h>

#include <string>

class RuntimeSimBadge {
public:
  const char* update(bool simRunning,
                     const std::string& scenarioId,
                     int lastPass,
                     unsigned long nowMs,
                     unsigned long bannerDurationMs) {
    if (simRunning) {
      runLatched_ = true;
      badgeStartedMs_ = 0;
      badgeDurationMs_ = 0;
      badge_[0] = '\0';
      return nullptr;
    }

    if (runLatched_) {
      runLatched_ = false;
      std::string shortId = scenarioId;
      const size_t underscore = shortId.find('_');
      if (underscore != std::string::npos) {
        shortId = shortId.substr(0, underscore);
      }
      if (shortId.empty()) {
        shortId = "SIM";
      }
      const char* verdict = (lastPass == 1) ? "PASS" : ((lastPass == 0) ? "FAIL" : "ABRT");
      snprintf(badge_, sizeof(badge_), "SIM %s %s", shortId.c_str(), verdict);
      badgeStartedMs_ = nowMs;
      badgeDurationMs_ = bannerDurationMs;
    }

    return current(nowMs);
  }

  const char* current(unsigned long nowMs) {
    const bool active = (badge_[0] != '\0') && (badgeDurationMs_ > 0) &&
                        ((nowMs - badgeStartedMs_) < badgeDurationMs_);
    if (!active) {
      badge_[0] = '\0';
      return nullptr;
    }
    return badge_;
  }

private:
  bool runLatched_ = false;
  unsigned long badgeStartedMs_ = 0;
  unsigned long badgeDurationMs_ = 0;
  char badge_[24] = {0};
};
