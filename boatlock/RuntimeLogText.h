#pragma once

#include <stddef.h>
#include <string.h>

inline size_t runtimeLogFormattedLength(int formatResult, size_t bufferSize) {
  if (formatResult <= 0 || bufferSize == 0) {
    return 0;
  }
  const size_t maxLen = bufferSize - 1;
  const size_t wanted = (size_t)formatResult;
  return wanted < maxLen ? wanted : maxLen;
}

inline bool runtimeLogShouldForwardToBle(const char* value) {
  return value && strncmp(value, "[BLE]", 5) != 0;
}
