#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <mbedtls/sha256.h>

class BleOtaUpdate {
public:
  bool begin(size_t imageSize, const char* sha256Hex);
  bool writeChunk(const uint8_t* data, size_t len);
  bool finish();
  void abort(const char* reason = "abort");
  void loop();

  bool active() const { return active_; }
  bool restartPending() const { return restartPending_; }
  size_t expectedSize() const { return expectedSize_; }
  size_t receivedSize() const { return receivedSize_; }
  const char* lastError() const { return lastError_; }

private:
  void resetState();
  void setError(const char* reason);
  bool expectedShaMatches(const uint8_t digest[32]) const;
  static void formatSha256(const uint8_t digest[32], char* out, size_t outSize);
  static bool normalizeSha256(const char* in, char* out, size_t outSize);

  bool active_ = false;
  bool shaStarted_ = false;
  bool restartPending_ = false;
  unsigned long restartAtMs_ = 0;
  unsigned long lastProgressLogMs_ = 0;
  size_t expectedSize_ = 0;
  size_t receivedSize_ = 0;
  char expectedSha256_[65] = {0};
  char lastError_[40] = {0};
  mbedtls_sha256_context shaCtx_;
};
