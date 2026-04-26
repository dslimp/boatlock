#pragma once

#include <cstring>
#include <string>

static constexpr size_t kRuntimeStatusReasonMaxLen = 32;

struct RuntimeStatusInput {
  bool gpsUnavailable = false;
  bool gnssWeak = false;
  const char* gnssWeakReason = nullptr;
  bool headingAvailable = true;
  bool driftFail = false;
  bool driftAlert = false;
  bool anchorActive = false;
  const char* anchorDeniedReason = nullptr;
  const char* failsafeReason = nullptr;
  const char* safetyReason = nullptr;
  bool holdMode = false;
};

struct RuntimeStatusSnapshot {
  std::string summary;
  std::string reasons;
};

inline bool runtimeStatusReasonPresent(const char* reason) {
  return reason && reason[0] != '\0' && strcmp(reason, "NONE") != 0;
}

inline bool runtimeStatusReasonIsInformational(const char* reason) {
  return reason && strcmp(reason, "NUDGE_OK") == 0;
}

inline bool runtimeStatusInputHasAlert(const RuntimeStatusInput& input) {
  return input.holdMode || input.driftFail || runtimeStatusReasonPresent(input.failsafeReason);
}

inline bool runtimeStatusReasonCharAllowed(char ch) {
  return (ch >= 'A' && ch <= 'Z') ||
         (ch >= 'a' && ch <= 'z') ||
         (ch >= '0' && ch <= '9') ||
         ch == '_';
}

inline void appendRuntimeStatusReason(std::string* out, const char* reason) {
  if (!out || !runtimeStatusReasonPresent(reason)) {
    return;
  }
  std::string token;
  for (size_t i = 0; reason[i] != '\0' && token.size() < kRuntimeStatusReasonMaxLen; ++i) {
    token += runtimeStatusReasonCharAllowed(reason[i]) ? reason[i] : '_';
  }
  if (token.empty()) {
    return;
  }
  if (!out->empty()) {
    *out += ",";
  }
  *out += token;
}

inline std::string buildRuntimeStatusReasons(const RuntimeStatusInput& input) {
  std::string reasons;
  if (input.gpsUnavailable) {
    appendRuntimeStatusReason(&reasons, "NO_GPS");
  } else if (input.gnssWeak) {
    appendRuntimeStatusReason(&reasons, input.gnssWeakReason);
  }
  if (!input.headingAvailable) {
    appendRuntimeStatusReason(&reasons, "NO_COMPASS");
  }
  if (input.driftFail) {
    appendRuntimeStatusReason(&reasons, "DRIFT_FAIL");
  } else if (input.driftAlert) {
    appendRuntimeStatusReason(&reasons, "DRIFT_ALERT");
  }
  if (!input.anchorActive) {
    appendRuntimeStatusReason(&reasons, input.anchorDeniedReason);
  }
  appendRuntimeStatusReason(&reasons, input.failsafeReason);
  appendRuntimeStatusReason(&reasons, input.safetyReason);
  return reasons;
}

inline bool runtimeStatusInputHasWarning(const RuntimeStatusInput& input,
                                         const std::string& reasons) {
  if (reasons.empty()) {
    return false;
  }
  if (input.gpsUnavailable || input.gnssWeak || !input.headingAvailable ||
      input.driftAlert || input.driftFail) {
    return true;
  }
  if (!input.anchorActive && runtimeStatusReasonPresent(input.anchorDeniedReason)) {
    return true;
  }
  return runtimeStatusReasonPresent(input.safetyReason) &&
         !runtimeStatusReasonIsInformational(input.safetyReason);
}

inline const char* buildRuntimeStatusSummary(const RuntimeStatusInput& input,
                                             const std::string& reasons) {
  if (runtimeStatusInputHasAlert(input)) {
    return "ALERT";
  }
  if (runtimeStatusInputHasWarning(input, reasons)) {
    return "WARN";
  }
  return "OK";
}

inline RuntimeStatusSnapshot buildRuntimeStatusSnapshot(const RuntimeStatusInput& input) {
  RuntimeStatusSnapshot snapshot;
  snapshot.reasons = buildRuntimeStatusReasons(input);
  snapshot.summary = buildRuntimeStatusSummary(input, snapshot.reasons);
  return snapshot;
}
