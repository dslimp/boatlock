#pragma once

constexpr unsigned long kRuntimeCompassNoEventAge = 0xFFFFFFFFUL;

enum class RuntimeCompassLossReason {
  NONE,
  FIRST_EVENT_TIMEOUT,
  EVENT_STALE,
};

struct RuntimeCompassHealthInput {
  bool compassReady = false;
  unsigned long nowMs = 0;
  unsigned long readySinceMs = 0;
  unsigned long lastHeadingEventAgeMs = kRuntimeCompassNoEventAge;
  unsigned long firstEventTimeoutMs = 0;
  unsigned long staleEventMs = 0;
};

inline bool elapsedAtLeast(unsigned long nowMs, unsigned long sinceMs, unsigned long intervalMs) {
  return nowMs - sinceMs >= intervalMs;
}

inline RuntimeCompassLossReason runtimeCompassLossReason(const RuntimeCompassHealthInput& input) {
  if (!input.compassReady) {
    return RuntimeCompassLossReason::NONE;
  }

  if (input.lastHeadingEventAgeMs == kRuntimeCompassNoEventAge) {
    if (input.firstEventTimeoutMs == 0) {
      return RuntimeCompassLossReason::NONE;
    }
    return elapsedAtLeast(input.nowMs, input.readySinceMs, input.firstEventTimeoutMs)
               ? RuntimeCompassLossReason::FIRST_EVENT_TIMEOUT
               : RuntimeCompassLossReason::NONE;
  }

  if (input.staleEventMs > 0 && input.lastHeadingEventAgeMs >= input.staleEventMs) {
    return RuntimeCompassLossReason::EVENT_STALE;
  }
  return RuntimeCompassLossReason::NONE;
}

inline const char* runtimeCompassLossReasonString(RuntimeCompassLossReason reason) {
  switch (reason) {
    case RuntimeCompassLossReason::FIRST_EVENT_TIMEOUT:
      return "FIRST_EVENT_TIMEOUT";
    case RuntimeCompassLossReason::EVENT_STALE:
      return "EVENT_STALE";
    case RuntimeCompassLossReason::NONE:
    default:
      return "NONE";
  }
}
