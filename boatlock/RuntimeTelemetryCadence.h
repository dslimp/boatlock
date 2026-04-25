#pragma once

class RuntimeTelemetryCadence {
public:
  static constexpr unsigned long kMinIntervalMs = 1;

  bool shouldRefreshUi(unsigned long nowMs, unsigned long intervalMs) {
    return shouldRun(nowMs, intervalMs, lastUiRefreshMs_);
  }

  bool shouldNotifyBle(unsigned long nowMs, unsigned long intervalMs) {
    return shouldRun(nowMs, intervalMs, lastBleNotifyMs_);
  }

  bool shouldPollCompass(unsigned long nowMs, unsigned long intervalMs) {
    return shouldRun(nowMs, intervalMs, lastCompassPollMs_);
  }

  unsigned long lastUiRefreshMs() const { return lastUiRefreshMs_; }
  unsigned long lastBleNotifyMs() const { return lastBleNotifyMs_; }
  unsigned long lastCompassPollMs() const { return lastCompassPollMs_; }

private:
  static bool shouldRun(unsigned long nowMs, unsigned long intervalMs, unsigned long& lastMs) {
    const unsigned long effectiveInterval =
        intervalMs < kMinIntervalMs ? kMinIntervalMs : intervalMs;
    if (nowMs - lastMs < effectiveInterval) {
      return false;
    }
    lastMs = nowMs;
    return true;
  }

  unsigned long lastUiRefreshMs_ = 0;
  unsigned long lastBleNotifyMs_ = 0;
  unsigned long lastCompassPollMs_ = 0;
};
