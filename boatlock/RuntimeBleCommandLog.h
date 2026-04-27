#pragma once

#include <cstddef>
#include <string>

inline bool runtimeBleShouldLogCommand(const std::string& command) {
  return command != "HEARTBEAT" && command.rfind("MANUAL_TARGET:", 0) != 0;
}

inline bool runtimeBleCommandStartsWith(const std::string& command, const char* prefix) {
  if (!prefix) {
    return false;
  }
  size_t i = 0;
  while (prefix[i] != '\0') {
    if (i >= command.size() || command[i] != prefix[i]) {
      return false;
    }
    ++i;
  }
  return true;
}

inline std::string runtimeBleSensitiveCommandLabel(const std::string& command) {
  if (runtimeBleCommandStartsWith(command, "PAIR_SET:")) {
    return "PAIR_SET:<redacted>";
  }
  if (runtimeBleCommandStartsWith(command, "AUTH_PROVE:")) {
    return "AUTH_PROVE:<redacted>";
  }
  if (runtimeBleCommandStartsWith(command, "SEC_CMD:")) {
    return "SEC_CMD:<redacted>";
  }
  return "";
}

inline std::string runtimeBleLogCommandText(const std::string& command, size_t maxLen = 96) {
  std::string output = runtimeBleSensitiveCommandLabel(command);
  if (output.empty()) {
    output.reserve(command.size());
    for (unsigned char ch : command) {
      output.push_back(ch >= 0x20 && ch <= 0x7E ? static_cast<char>(ch) : ' ');
    }
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
