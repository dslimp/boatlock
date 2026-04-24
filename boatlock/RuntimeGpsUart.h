#pragma once

#include <stddef.h>

struct RuntimeGpsUartConfig {
  unsigned long noDataWarnMs = 6000;
  unsigned long staleRestartMs = 6000;
  unsigned long restartCooldownMs = 4000;
};

struct RuntimeGpsUartActions {
  bool firstDataSeen = false;
  bool warnNoData = false;
  bool warnStale = false;
  bool restartSerial = false;
  unsigned long staleAgeMs = 0;
};

class RuntimeGpsUart {
public:
  void reset(unsigned long bootMs) {
    noDataBaselineMs_ = bootMs;
    seen_ = false;
    noDataWarned_ = false;
    staleLogged_ = false;
    lastByteMs_ = 0;
    lastRestartMs_ = 0;
    restartSeen_ = false;
  }

  RuntimeGpsUartActions update(size_t bytesRead,
                               unsigned long nowMs,
                               const RuntimeGpsUartConfig& config) {
    RuntimeGpsUartActions actions;

    if (bytesRead > 0) {
      actions.firstDataSeen = !seen_;
      seen_ = true;
      lastByteMs_ = nowMs;
      staleLogged_ = false;
    }

    if (!seen_ && !noDataWarned_ &&
        elapsedGreaterThan(nowMs, noDataBaselineMs_, config.noDataWarnMs)) {
      noDataWarned_ = true;
      actions.warnNoData = true;
    }

    if (seen_ &&
        elapsedAtLeast(nowMs, lastByteMs_, config.staleRestartMs)) {
      actions.staleAgeMs = nowMs - lastByteMs_;
      if (!staleLogged_) {
        staleLogged_ = true;
        actions.warnStale = true;
      }
      if (!restartSeen_ || elapsedAtLeast(nowMs, lastRestartMs_, config.restartCooldownMs)) {
        lastRestartMs_ = nowMs;
        restartSeen_ = true;
        noDataBaselineMs_ = nowMs;
        seen_ = false;
        noDataWarned_ = false;
        actions.restartSerial = true;
      }
    }

    return actions;
  }

private:
  unsigned long noDataBaselineMs_ = 0;
  bool seen_ = false;
  bool noDataWarned_ = false;
  bool staleLogged_ = false;
  bool restartSeen_ = false;
  unsigned long lastByteMs_ = 0;
  unsigned long lastRestartMs_ = 0;

  static bool elapsedAtLeast(unsigned long nowMs, unsigned long sinceMs, unsigned long intervalMs) {
    return nowMs - sinceMs >= intervalMs;
  }

  static bool elapsedGreaterThan(unsigned long nowMs, unsigned long sinceMs, unsigned long intervalMs) {
    return nowMs - sinceMs > intervalMs;
  }
};
