#pragma once

#include <cstring>
#include <string>

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

inline bool runtimeStatusReasonPresent(const char* reason) {
  return reason && reason[0] != '\0' && strcmp(reason, "NONE") != 0;
}

inline bool runtimeStatusReasonIsInformational(const char* reason) {
  return reason && strcmp(reason, "NUDGE_OK") == 0;
}

inline void appendRuntimeStatusReason(std::string* out, const char* reason) {
  if (!out || !runtimeStatusReasonPresent(reason)) {
    return;
  }
  if (!out->empty()) {
    *out += ",";
  }
  *out += reason;
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
  if (runtimeStatusReasonPresent(input.failsafeReason)) {
    return true;
  }
  return runtimeStatusReasonPresent(input.safetyReason) &&
         !runtimeStatusReasonIsInformational(input.safetyReason);
}

inline const char* buildRuntimeStatusSummary(const RuntimeStatusInput& input,
                                             const std::string& reasons) {
  if (input.holdMode || input.driftFail || runtimeStatusReasonPresent(input.failsafeReason)) {
    return "ALERT";
  }
  if (runtimeStatusInputHasWarning(input, reasons)) {
    return "WARN";
  }
  return "OK";
}
