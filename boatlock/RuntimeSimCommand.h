#pragma once

#include <stdint.h>
#include <stdlib.h>

#include <string>

enum class RuntimeSimCommandType : uint8_t {
  NONE = 0,
  LIST = 1,
  STATUS = 2,
  RUN = 3,
  ABORT = 4,
  REPORT = 5,
  UNKNOWN = 6,
};

struct RuntimeSimCommand {
  RuntimeSimCommandType type = RuntimeSimCommandType::NONE;
  bool valid = true;
  std::string scenarioId;
  int speedup = 0;
};

inline std::string trimRuntimeSimAscii(const std::string& input) {
  size_t begin = 0;
  while (begin < input.size() &&
         (input[begin] == ' ' || input[begin] == '\t' || input[begin] == '\r' || input[begin] == '\n')) {
    ++begin;
  }
  size_t end = input.size();
  while (end > begin &&
         (input[end - 1] == ' ' || input[end - 1] == '\t' || input[end - 1] == '\r' ||
          input[end - 1] == '\n')) {
    --end;
  }
  return input.substr(begin, end - begin);
}

inline bool parseRuntimeSimRunPayload(const std::string& command, std::string* scenarioId, int* speedup) {
  if (!scenarioId || !speedup) {
    return false;
  }

  *scenarioId = "";
  *speedup = 0;
  const size_t colon = command.find(':');
  if (colon == std::string::npos || colon + 1 >= command.size()) {
    return false;
  }

  const std::string payload = trimRuntimeSimAscii(command.substr(colon + 1));
  if (payload.empty()) {
    return false;
  }

  const size_t comma = payload.find(',');
  if (comma == std::string::npos) {
    *scenarioId = trimRuntimeSimAscii(payload);
    return !scenarioId->empty();
  }

  *scenarioId = trimRuntimeSimAscii(payload.substr(0, comma));
  const std::string speedupToken = trimRuntimeSimAscii(payload.substr(comma + 1));
  char* end = nullptr;
  const long parsedSpeedup = strtol(speedupToken.c_str(), &end, 10);
  if (speedupToken.empty() || !end || *end != '\0' || (parsedSpeedup != 0 && parsedSpeedup != 1)) {
    return false;
  }
  *speedup = (int)parsedSpeedup;
  return !scenarioId->empty();
}

inline RuntimeSimCommand parseRuntimeSimCommand(const std::string& command) {
  RuntimeSimCommand parsed;
  if (command.rfind("SIM_", 0) != 0) {
    return parsed;
  }

  if (command == "SIM_LIST") {
    parsed.type = RuntimeSimCommandType::LIST;
    return parsed;
  }

  if (command == "SIM_STATUS") {
    parsed.type = RuntimeSimCommandType::STATUS;
    return parsed;
  }

  if (command == "SIM_RUN" || command.rfind("SIM_RUN:", 0) == 0) {
    parsed.type = RuntimeSimCommandType::RUN;
    parsed.valid = parseRuntimeSimRunPayload(command, &parsed.scenarioId, &parsed.speedup);
    return parsed;
  }

  if (command == "SIM_ABORT") {
    parsed.type = RuntimeSimCommandType::ABORT;
    return parsed;
  }

  if (command == "SIM_REPORT") {
    parsed.type = RuntimeSimCommandType::REPORT;
    return parsed;
  }

  parsed.type = RuntimeSimCommandType::UNKNOWN;
  return parsed;
}
