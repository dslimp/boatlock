#pragma once

class RuntimeTelemetryCadence {
public:
  bool shouldRefreshUi(unsigned long nowMs, unsigned long intervalMs) {
    if (nowMs - lastUiRefreshMs_ < intervalMs) {
      return false;
    }
    lastUiRefreshMs_ = nowMs;
    return true;
  }

  bool shouldNotifyBle(unsigned long nowMs, unsigned long intervalMs) {
    if (nowMs - lastBleNotifyMs_ < intervalMs) {
      return false;
    }
    lastBleNotifyMs_ = nowMs;
    return true;
  }

  unsigned long lastUiRefreshMs() const { return lastUiRefreshMs_; }
  unsigned long lastBleNotifyMs() const { return lastBleNotifyMs_; }

private:
  unsigned long lastUiRefreshMs_ = 0;
  unsigned long lastBleNotifyMs_ = 0;
};
