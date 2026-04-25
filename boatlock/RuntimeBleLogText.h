#pragma once

#include <stddef.h>
#include <string.h>

#include <string>

inline size_t runtimeBleCStringLength(const char* value, size_t maxLen) {
  if (!value) {
    return 0;
  }
  size_t len = 0;
  while (len < maxLen && value[len] != '\0') {
    ++len;
  }
  return len;
}

inline bool runtimeBleLogLineBreak(char ch) {
  return ch == '\r' || ch == '\n';
}

inline size_t runtimeBleLogTrimLength(const char* value, size_t len) {
  while (len > 0 && runtimeBleLogLineBreak(value[len - 1])) {
    --len;
  }
  return len;
}

inline char runtimeBleLogSafeChar(char ch) {
  const unsigned char value = (unsigned char)ch;
  if (value < 0x20 || value == 0x7F) {
    return ' ';
  }
  return ch;
}

inline std::string runtimeBleLogValue(const char* value, size_t maxLen) {
  if (!value) {
    return std::string();
  }
  const size_t len =
      runtimeBleLogTrimLength(value, runtimeBleCStringLength(value, maxLen));
  std::string out;
  out.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    out.push_back(runtimeBleLogSafeChar(value[i]));
  }
  return out;
}

inline size_t runtimeBlePrepareLogPayload(char* out, size_t outSize, const char* value) {
  if (!out || outSize == 0) {
    return 0;
  }
  memset(out, 0, outSize);
  if (!value) {
    return 0;
  }
  const size_t len =
      runtimeBleLogTrimLength(value, runtimeBleCStringLength(value, outSize - 1));
  for (size_t i = 0; i < len; ++i) {
    out[i] = runtimeBleLogSafeChar(value[i]);
  }
  return len;
}
