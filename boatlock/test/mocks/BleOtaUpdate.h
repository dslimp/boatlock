#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

class BleOtaUpdate {
public:
  bool begin(size_t imageSize, const char* sha256Hex) {
    beginCalls += 1;
    lastImageSize = imageSize;
    std::strncpy(lastSha256Hex, sha256Hex ? sha256Hex : "", sizeof(lastSha256Hex) - 1);
    lastSha256Hex[sizeof(lastSha256Hex) - 1] = '\0';
    active_ = beginResult;
    return beginResult;
  }

  bool writeChunk(const uint8_t*, size_t) { return active_; }

  bool finish() {
    finishCalls += 1;
    active_ = false;
    return finishResult;
  }

  void abort(const char* reason = "abort") {
    abortCalls += 1;
    std::strncpy(lastAbortReason, reason ? reason : "", sizeof(lastAbortReason) - 1);
    lastAbortReason[sizeof(lastAbortReason) - 1] = '\0';
    active_ = false;
  }

  bool active() const { return active_; }

  void setActive(bool value) { active_ = value; }

  void reset() {
    beginCalls = 0;
    finishCalls = 0;
    abortCalls = 0;
    lastImageSize = 0;
    lastSha256Hex[0] = '\0';
    lastAbortReason[0] = '\0';
    active_ = false;
    beginResult = true;
    finishResult = true;
  }

  int beginCalls = 0;
  int finishCalls = 0;
  int abortCalls = 0;
  size_t lastImageSize = 0;
  char lastSha256Hex[65] = {0};
  char lastAbortReason[32] = {0};
  bool beginResult = true;
  bool finishResult = true;

private:
  bool active_ = false;
};
