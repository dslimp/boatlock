#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace hilsim {

struct ScenarioExpect {
  enum class Type : uint8_t {
    HOLD = 0,
    FAILSAFE = 1,
  };

  Type type = Type::HOLD;
  float p95ErrorMaxM = -1.0f;
  float maxErrorMaxM = -1.0f;
  float timeInDeadbandMinPct = -1.0f;
  float timeSaturatedMaxPct = -1.0f;
  float dirChangesPerMinMax = -1.0f;
  float maxBadGnssInAnchorMaxS = -1.0f;
  std::vector<std::string> requiredEvents;
};

struct SimMetrics {
  float p95ErrorM = 0.0f;
  float maxErrorM = 0.0f;
  float timeInDeadbandPct = 0.0f;
  float timeSaturatedPct = 0.0f;
  float dirChangesPerMin = 0.0f;
  uint32_t rampViolations = 0;
  float maxBadGnssInAnchorS = 0.0f;
  bool nanDetected = false;
  bool outOfRangeCommand = false;
  bool hardFailure = false;
  std::string hardFailureReason;
};

struct SimExpectationEval {
  bool pass = true;
  std::string reason = "PASS";
};

inline bool simExpectAllowsInternalNan(const ScenarioExpect& ex) {
  if (ex.type != ScenarioExpect::Type::FAILSAFE) {
    return false;
  }
  for (const std::string& required : ex.requiredEvents) {
    if (required == "INTERNAL_ERROR_NAN") {
      return true;
    }
  }
  return false;
}

inline SimExpectationEval evaluateSimMetrics(const ScenarioExpect& ex,
                                             const SimMetrics& metrics) {
  SimExpectationEval eval;
  if (metrics.nanDetected && !simExpectAllowsInternalNan(ex)) {
    eval.pass = false;
    eval.reason = "NAN_DETECTED";
    return eval;
  }
  if (metrics.outOfRangeCommand) {
    eval.pass = false;
    eval.reason = "COMMAND_OUT_OF_RANGE";
    return eval;
  }
  if (ex.p95ErrorMaxM > 0.0f && metrics.p95ErrorM > ex.p95ErrorMaxM) {
    eval.pass = false;
    eval.reason = "P95_ERROR";
    return eval;
  }
  if (ex.maxErrorMaxM > 0.0f && metrics.maxErrorM > ex.maxErrorMaxM) {
    eval.pass = false;
    eval.reason = "MAX_ERROR";
    return eval;
  }
  if (ex.timeInDeadbandMinPct >= 0.0f &&
      metrics.timeInDeadbandPct < ex.timeInDeadbandMinPct) {
    eval.pass = false;
    eval.reason = "DEADBAND_PCT";
    return eval;
  }
  if (ex.timeSaturatedMaxPct >= 0.0f &&
      metrics.timeSaturatedPct > ex.timeSaturatedMaxPct) {
    eval.pass = false;
    eval.reason = "SATURATED_PCT";
    return eval;
  }
  if (ex.dirChangesPerMinMax >= 0.0f &&
      metrics.dirChangesPerMin > ex.dirChangesPerMinMax) {
    eval.pass = false;
    eval.reason = "DIR_CHANGES";
    return eval;
  }
  if (ex.maxBadGnssInAnchorMaxS >= 0.0f &&
      metrics.maxBadGnssInAnchorS > ex.maxBadGnssInAnchorMaxS) {
    eval.pass = false;
    eval.reason = "BAD_GNSS_TIME";
    return eval;
  }
  return eval;
}

} // namespace hilsim
