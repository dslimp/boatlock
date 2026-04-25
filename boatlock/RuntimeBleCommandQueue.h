#pragma once

#include <cstddef>
#include <cstring>
#include <string>

inline bool runtimeBleCopyCommandForQueue(char* dest, size_t destLen, const std::string& command) {
  if (!dest || destLen == 0) {
    return false;
  }
  std::memset(dest, 0, destLen);
  if (command.size() >= destLen) {
    return false;
  }
  if (!command.empty()) {
    std::memcpy(dest, command.data(), command.size());
  }
  return true;
}
