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

inline std::string runtimeBleLogValue(const char* value, size_t maxLen) {
  return std::string(value ? value : "", runtimeBleCStringLength(value, maxLen));
}

inline size_t runtimeBlePrepareLogPayload(char* out, size_t outSize, const char* value) {
  if (!out || outSize == 0) {
    return 0;
  }
  memset(out, 0, outSize);
  if (!value) {
    return 0;
  }
  const size_t len = runtimeBleCStringLength(value, outSize - 1);
  memcpy(out, value, len);
  return len;
}
