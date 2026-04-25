#pragma once

#include <string>

enum class RuntimeBleImmediateCommand {
  NONE,
  STREAM_START,
  STREAM_STOP,
  SNAPSHOT,
};

inline RuntimeBleImmediateCommand runtimeBleParseImmediateCommand(const std::string& command) {
  if (command == "STREAM_START") {
    return RuntimeBleImmediateCommand::STREAM_START;
  }
  if (command == "STREAM_STOP") {
    return RuntimeBleImmediateCommand::STREAM_STOP;
  }
  if (command == "SNAPSHOT") {
    return RuntimeBleImmediateCommand::SNAPSHOT;
  }
  return RuntimeBleImmediateCommand::NONE;
}
