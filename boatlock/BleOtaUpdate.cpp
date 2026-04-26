#include "BleOtaUpdate.h"
#include "Logger.h"
#include "RuntimeBleOtaCommand.h"
#include <ESP.h>
#include <Update.h>
#include <cstring>

namespace {
constexpr unsigned long kRestartDelayMs = 900;
constexpr unsigned long kProgressLogGapMs = 2500;

char lowerHex(char c) {
  if (c >= 'A' && c <= 'F') {
    return static_cast<char>(c - 'A' + 'a');
  }
  return c;
}
} // namespace

bool BleOtaUpdate::begin(size_t imageSize, const char* sha256Hex) {
  if (active_) {
    logMessage("[OTA] begin rejected reason=already_active\n");
    setError("already_active");
    return false;
  }
  if (restartPending_) {
    logMessage("[OTA] begin rejected reason=restart_pending\n");
    setError("restart_pending");
    return false;
  }
  if (imageSize == 0 || !normalizeSha256(sha256Hex, expectedSha256_, sizeof(expectedSha256_))) {
    logMessage("[OTA] begin rejected reason=bad_payload\n");
    setError("bad_payload");
    return false;
  }

  if (!Update.begin(imageSize, U_FLASH)) {
    setError(Update.errorString());
    logMessage("[OTA] begin rejected reason=%s\n", lastError_);
    resetState();
    return false;
  }

  mbedtls_sha256_init(&shaCtx_);
  if (mbedtls_sha256_starts_ret(&shaCtx_, 0) != 0) {
    Update.abort();
    resetState();
    setError("sha_start_failed");
    logMessage("[OTA] begin rejected reason=sha_start_failed\n");
    return false;
  }

  active_ = true;
  shaStarted_ = true;
  expectedSize_ = imageSize;
  receivedSize_ = 0;
  lastProgressLogMs_ = millis();
  setError("");
  logMessage("[OTA] begin ok size=%lu sha256=%s\n",
             static_cast<unsigned long>(expectedSize_),
             expectedSha256_);
  return true;
}

bool BleOtaUpdate::writeChunk(const uint8_t* data, size_t len) {
  if (!active_) {
    return false;
  }
  if (!data || len == 0) {
    return true;
  }
  if (receivedSize_ + len > expectedSize_) {
    Update.abort();
    resetState();
    setError("too_much_data");
    logMessage("[OTA] chunk rejected reason=too_much_data\n");
    return false;
  }

  const size_t written = Update.write(const_cast<uint8_t*>(data), len);
  if (written != len) {
    setError(Update.errorString());
    Update.abort();
    resetState();
    logMessage("[OTA] chunk rejected reason=%s\n", lastError_);
    return false;
  }

  if (mbedtls_sha256_update_ret(&shaCtx_, data, len) != 0) {
    Update.abort();
    resetState();
    setError("sha_update_failed");
    logMessage("[OTA] chunk rejected reason=sha_update_failed\n");
    return false;
  }

  receivedSize_ += len;
  const unsigned long now = millis();
  if (now - lastProgressLogMs_ >= kProgressLogGapMs || receivedSize_ == expectedSize_) {
    lastProgressLogMs_ = now;
    logMessage("[OTA] progress bytes=%lu total=%lu\n",
               static_cast<unsigned long>(receivedSize_),
               static_cast<unsigned long>(expectedSize_));
  }
  return true;
}

bool BleOtaUpdate::finish() {
  if (!active_) {
    logMessage("[OTA] finish rejected reason=not_active\n");
    setError("not_active");
    return false;
  }
  if (receivedSize_ != expectedSize_) {
    Update.abort();
    resetState();
    setError("size_mismatch");
    logMessage("[OTA] finish rejected reason=size_mismatch received=%lu expected=%lu\n",
               static_cast<unsigned long>(receivedSize_),
               static_cast<unsigned long>(expectedSize_));
    return false;
  }

  uint8_t digest[32] = {0};
  if (mbedtls_sha256_finish_ret(&shaCtx_, digest) != 0) {
    Update.abort();
    resetState();
    setError("sha_finish_failed");
    logMessage("[OTA] finish rejected reason=sha_finish_failed\n");
    return false;
  }
  mbedtls_sha256_free(&shaCtx_);
  shaStarted_ = false;

  if (!expectedShaMatches(digest)) {
    char actual[65] = {0};
    formatSha256(digest, actual, sizeof(actual));
    Update.abort();
    resetState();
    setError("sha_mismatch");
    logMessage("[OTA] finish rejected reason=sha_mismatch actual=%s\n", actual);
    return false;
  }

  if (!Update.end(true)) {
    setError(Update.errorString());
    resetState();
    logMessage("[OTA] finish rejected reason=%s\n", lastError_);
    return false;
  }

  active_ = false;
  restartPending_ = true;
  restartAtMs_ = millis() + kRestartDelayMs;
  logMessage("[OTA] finish ok size=%lu reboot_ms=%lu\n",
             static_cast<unsigned long>(expectedSize_),
             static_cast<unsigned long>(kRestartDelayMs));
  return true;
}

void BleOtaUpdate::abort(const char* reason) {
  if (active_) {
    Update.abort();
    logMessage("[OTA] abort reason=%s received=%lu expected=%lu\n",
               reason ? reason : "abort",
               static_cast<unsigned long>(receivedSize_),
               static_cast<unsigned long>(expectedSize_));
  }
  resetState();
  setError(reason ? reason : "abort");
}

void BleOtaUpdate::loop() {
  if (!restartPending_) {
    return;
  }
  const unsigned long now = millis();
  if (static_cast<long>(now - restartAtMs_) < 0) {
    return;
  }
  logMessage("[OTA] restarting\n");
  delay(50);
  ESP.restart();
}

void BleOtaUpdate::resetState() {
  if (shaStarted_) {
    mbedtls_sha256_free(&shaCtx_);
  }
  active_ = false;
  shaStarted_ = false;
  expectedSize_ = 0;
  receivedSize_ = 0;
  expectedSha256_[0] = '\0';
  lastProgressLogMs_ = 0;
}

void BleOtaUpdate::setError(const char* reason) {
  if (!reason) {
    reason = "";
  }
  std::strncpy(lastError_, reason, sizeof(lastError_) - 1);
  lastError_[sizeof(lastError_) - 1] = '\0';
}

bool BleOtaUpdate::expectedShaMatches(const uint8_t digest[32]) const {
  char actual[65] = {0};
  formatSha256(digest, actual, sizeof(actual));
  return std::strcmp(actual, expectedSha256_) == 0;
}

void BleOtaUpdate::formatSha256(const uint8_t digest[32], char* out, size_t outSize) {
  if (!out || outSize < 65) {
    return;
  }
  static constexpr char kHex[] = "0123456789abcdef";
  for (size_t i = 0; i < 32; ++i) {
    out[i * 2] = kHex[(digest[i] >> 4) & 0x0F];
    out[i * 2 + 1] = kHex[digest[i] & 0x0F];
  }
  out[64] = '\0';
}

bool BleOtaUpdate::normalizeSha256(const char* in, char* out, size_t outSize) {
  if (!in || !out || outSize < 65 || !runtimeBleOtaIsSha256Hex(in)) {
    return false;
  }
  for (size_t i = 0; i < 64; ++i) {
    out[i] = lowerHex(in[i]);
  }
  out[64] = '\0';
  return true;
}
