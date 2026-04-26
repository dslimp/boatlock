#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

struct RuntimeBleOtaBeginCommand {
  size_t imageSize = 0;
  char sha256Hex[65] = {0};
};

inline bool runtimeBleOtaIsHex(char c) {
  return (c >= '0' && c <= '9') ||
         (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

inline bool runtimeBleOtaIsSha256Hex(const char* value) {
  if (!value || std::strlen(value) != 64) {
    return false;
  }
  for (size_t i = 0; i < 64; ++i) {
    if (!runtimeBleOtaIsHex(value[i])) {
      return false;
    }
  }
  return true;
}

inline bool runtimeBleParseOtaBeginCommand(const std::string& command,
                                           RuntimeBleOtaBeginCommand* out) {
  if (command.rfind("OTA_BEGIN:", 0) != 0) {
    return false;
  }
  const char* payload = command.c_str() + 10;
  const char* comma = std::strchr(payload, ',');
  if (!comma || comma == payload) {
    return false;
  }

  uint64_t size = 0;
  for (const char* p = payload; p < comma; ++p) {
    if (*p < '0' || *p > '9') {
      return false;
    }
    size = (size * 10ULL) + static_cast<uint64_t>(*p - '0');
    if (size > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
      return false;
    }
  }
  if (size == 0) {
    return false;
  }

  const char* sha = comma + 1;
  if (!runtimeBleOtaIsSha256Hex(sha)) {
    return false;
  }

  if (out) {
    out->imageSize = static_cast<size_t>(size);
    std::memcpy(out->sha256Hex, sha, 64);
    out->sha256Hex[64] = '\0';
  }
  return true;
}
