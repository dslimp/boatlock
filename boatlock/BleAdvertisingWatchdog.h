#pragma once

#include <stdint.h>

enum class BleAdvertisingWatchdogAction : uint8_t {
  NONE = 0,
  MARK_CONNECTED = 1,
  RESTART_ADVERTISING = 2,
  START_CONNECTED_ADVERTISING = 3,
};

inline BleAdvertisingWatchdogAction bleAdvertisingWatchdogAction(bool statusConnected,
                                                                 bool serverHasClients,
                                                                 bool advertisingActive) {
  if (serverHasClients) {
    if (!statusConnected) {
      return BleAdvertisingWatchdogAction::MARK_CONNECTED;
    }
    return advertisingActive ? BleAdvertisingWatchdogAction::NONE
                             : BleAdvertisingWatchdogAction::START_CONNECTED_ADVERTISING;
  }
  if (statusConnected || !advertisingActive) {
    return BleAdvertisingWatchdogAction::RESTART_ADVERTISING;
  }
  return BleAdvertisingWatchdogAction::NONE;
}
