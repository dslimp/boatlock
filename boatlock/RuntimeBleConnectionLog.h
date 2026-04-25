#pragma once

#include "RuntimeLogText.h"
#include <stddef.h>
#include <stdio.h>

inline bool runtimeBleLogAddressCharAllowed(char ch) {
  return (ch >= '0' && ch <= '9') ||
         (ch >= 'a' && ch <= 'f') ||
         (ch >= 'A' && ch <= 'F') ||
         ch == ':';
}

inline bool runtimeBleLogAddressLooksSafe(const char* address) {
  if (!address || !address[0]) {
    return false;
  }
  size_t len = 0;
  while (address[len] != '\0') {
    if (len >= 31 || !runtimeBleLogAddressCharAllowed(address[len])) {
      return false;
    }
    ++len;
  }
  return len > 0;
}

inline const char* runtimeBleLogAddress(const char* address) {
  return runtimeBleLogAddressLooksSafe(address) ? address : "unknown";
}

inline unsigned int runtimeBleReasonCodeHex(int reason) {
  return reason < 0 ? 0U : (unsigned int)reason;
}

inline size_t runtimeBleFormatConnectLog(char* out,
                                         size_t outSize,
                                         const char* address,
                                         unsigned int mtu,
                                         unsigned int interval,
                                         unsigned int timeout) {
  if (!out || outSize == 0) {
    return 0;
  }
  const int written = snprintf(out,
                               outSize,
                               "[BLE] connect addr=%s mtu=%u int=%u timeout=%u\n",
                               runtimeBleLogAddress(address),
                               mtu,
                               interval,
                               timeout);
  return runtimeLogFormattedLength(written, outSize);
}

inline size_t runtimeBleFormatDisconnectLog(char* out,
                                            size_t outSize,
                                            const char* address,
                                            int reason,
                                            unsigned int connectedClients) {
  if (!out || outSize == 0) {
    return 0;
  }
  const int written = snprintf(out,
                               outSize,
                               "[BLE] disconnect addr=%s reason=%d hex=0x%03X clients=%u\n",
                               runtimeBleLogAddress(address),
                               reason,
                               runtimeBleReasonCodeHex(reason),
                               connectedClients);
  return runtimeLogFormattedLength(written, outSize);
}
