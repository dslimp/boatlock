#pragma once

#include <stddef.h>

inline size_t runtimeBleFixedNotifyPayloadLength(size_t len, size_t expectedLen) {
  if (len != expectedLen) {
    return 0;
  }
  return len;
}
