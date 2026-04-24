#pragma once

#include <stdint.h>

enum class BleAdvertisingWatchdogAction : uint8_t {
  NONE = 0,
  MARK_CONNECTED = 1,
  RESTART_ADVERTISING = 2,
};

inline BleAdvertisingWatchdogAction bleAdvertisingWatchdogAction(bool statusConnected,
                                                                 bool serverHasClients,
                                                                 bool advertisingActive) {
  if (serverHasClients) {
    return statusConnected ? BleAdvertisingWatchdogAction::NONE
                           : BleAdvertisingWatchdogAction::MARK_CONNECTED;
  }
  if (statusConnected || !advertisingActive) {
    return BleAdvertisingWatchdogAction::RESTART_ADVERTISING;
  }
  return BleAdvertisingWatchdogAction::NONE;
}
