#pragma once

#include <stddef.h>

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
