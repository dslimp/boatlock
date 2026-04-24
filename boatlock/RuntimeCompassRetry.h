#pragma once

class RuntimeCompassRetry {
public:
  bool shouldRetry(bool compassReady, unsigned long nowMs, unsigned long intervalMs) {
    if (compassReady) {
      return false;
    }
    if (nowMs - lastRetryMs_ < intervalMs) {
      return false;
    }
    lastRetryMs_ = nowMs;
    return true;
  }

private:
  unsigned long lastRetryMs_ = 0;
};
