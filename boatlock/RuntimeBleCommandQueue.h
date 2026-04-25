#pragma once

#include <cstddef>
#include <cstring>
#include <string>

inline bool runtimeBleCommandQueueByteAllowed(unsigned char ch) {
  return ch >= 0x20 && ch <= 0x7E;
}

inline const char* runtimeBleCommandQueueRejectReason(size_t destLen, const std::string& command) {
  if (destLen == 0) {
    return "bad_slot";
  }
  if (command.size() >= destLen) {
    return "too_long";
  }
  for (unsigned char ch : command) {
    if (!runtimeBleCommandQueueByteAllowed(ch)) {
      return "bad_bytes";
    }
  }
  return "";
}

inline bool runtimeBleCopyCommandForQueue(char* dest, size_t destLen, const std::string& command) {
  if (!dest || destLen == 0) {
    return false;
  }
  std::memset(dest, 0, destLen);
  if (runtimeBleCommandQueueRejectReason(destLen, command)[0] != '\0') {
    return false;
  }
  if (!command.empty()) {
    std::memcpy(dest, command.data(), command.size());
  }
  return true;
}
