#pragma once

#include <stddef.h>

inline size_t runtimeBleNotifyPayloadLength(size_t len, size_t maxLen) {
  if (len == 0 || len > maxLen) {
    return 0;
  }
  return len;
}
