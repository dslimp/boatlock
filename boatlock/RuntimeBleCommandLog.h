#pragma once

#include <cstddef>
#include <string>

inline bool runtimeBleShouldLogCommand(const std::string& command) {
  return command != "HEARTBEAT" && command.rfind("MANUAL_TARGET:", 0) != 0;
}

inline std::string runtimeBleLogCommandText(const std::string& command, size_t maxLen = 96) {
  std::string output;
  output.reserve(command.size());
  for (unsigned char ch : command) {
    output.push_back(ch >= 0x20 && ch <= 0x7E ? static_cast<char>(ch) : ' ');
  }

  if (output.size() <= maxLen) {
    return output;
  }
  if (maxLen == 0) {
    return "";
  }
  if (maxLen <= 3) {
    return output.substr(0, maxLen);
  }
  output.resize(maxLen - 3);
  output += "...";
  return output;
}
