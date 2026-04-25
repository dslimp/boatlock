#pragma once

#include <string>

inline bool runtimeBleShouldLogCommand(const std::string& command) {
  return command != "HEARTBEAT";
}
