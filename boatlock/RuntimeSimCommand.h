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
  std::string payload;

  const size_t colon = command.find(':');
  if (colon != std::string::npos && colon + 1 < command.size()) {
    payload = command.substr(colon + 1);
  } else {
    const size_t space = command.find(' ');
    if (space != std::string::npos && space + 1 < command.size()) {
      payload = command.substr(space + 1);
    }
  }

  payload = trimRuntimeSimAscii(payload);
  if (payload.empty()) {
    return false;
  }

  if (payload.front() == '{') {
    const size_t idPos = payload.find("id");
    if (idPos != std::string::npos) {
      size_t sep = payload.find_first_of(":=", idPos);
      if (sep != std::string::npos) {
        size_t start = sep + 1;
        while (start < payload.size() &&
               (payload[start] == ' ' || payload[start] == '"' || payload[start] == '\'')) {
          ++start;
        }
        size_t end = start;
        while (end < payload.size() && payload[end] != ',' && payload[end] != '}' &&
               payload[end] != '"' && payload[end] != '\'') {
          ++end;
        }
        *scenarioId = trimRuntimeSimAscii(payload.substr(start, end - start));
      }
    }

    const size_t speedupPos = payload.find("speedup");
    if (speedupPos != std::string::npos) {
      size_t sep = payload.find_first_of(":=", speedupPos);
      if (sep != std::string::npos) {
        *speedup = atoi(payload.c_str() + sep + 1);
      }
    }
    return !scenarioId->empty();
  }

  const size_t comma = payload.find(',');
  if (comma == std::string::npos) {
    *scenarioId = trimRuntimeSimAscii(payload);
    return !scenarioId->empty();
  }

  *scenarioId = trimRuntimeSimAscii(payload.substr(0, comma));
  *speedup = atoi(payload.c_str() + comma + 1);
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

  if (command.rfind("SIM_RUN", 0) == 0) {
    parsed.type = RuntimeSimCommandType::RUN;
    parsed.valid = parseRuntimeSimRunPayload(command, &parsed.scenarioId, &parsed.speedup);
    return parsed;
  }

  if (command == "SIM_ABORT") {
    parsed.type = RuntimeSimCommandType::ABORT;
    return parsed;
  }

  if (command.rfind("SIM_REPORT", 0) == 0) {
    parsed.type = RuntimeSimCommandType::REPORT;
    return parsed;
  }

  parsed.type = RuntimeSimCommandType::UNKNOWN;
  return parsed;
}
