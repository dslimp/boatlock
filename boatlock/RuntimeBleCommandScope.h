#pragma once

#include <cstdint>
#include <string>

enum class RuntimeBleCommandScope : uint8_t {
  UNKNOWN,
  RELEASE,
  SERVICE,
  DEV_HIL,
};

inline bool runtimeBleCommandHasPrefix(const std::string& command,
                                       const char* prefix) {
  return command.rfind(prefix, 0) == 0;
}

inline RuntimeBleCommandScope runtimeBleClassifyCommand(
    const std::string& command) {
  if (command.empty()) {
    return RuntimeBleCommandScope::UNKNOWN;
  }

  if (command == "STREAM_START" || command == "STREAM_STOP" ||
      command == "SNAPSHOT" || command == "ANCHOR_ON" ||
      command == "ANCHOR_OFF" || command == "STOP" ||
      command == "HEARTBEAT" || command == "MANUAL_OFF" ||
      command == "PAIR_CLEAR" || command == "AUTH_HELLO" ||
      runtimeBleCommandHasPrefix(command, "SET_ANCHOR:") ||
      runtimeBleCommandHasPrefix(command, "MANUAL_SET:") ||
      runtimeBleCommandHasPrefix(command, "NUDGE_DIR:") ||
      runtimeBleCommandHasPrefix(command, "NUDGE_BRG:") ||
      runtimeBleCommandHasPrefix(command, "SET_HOLD_HEADING:") ||
      runtimeBleCommandHasPrefix(command, "PAIR_SET:") ||
      runtimeBleCommandHasPrefix(command, "AUTH_PROVE:")) {
    return RuntimeBleCommandScope::RELEASE;
  }

  if (command == "RESET_COMPASS_OFFSET" || command == "SET_STEPPER_BOW" ||
      command == "COMPASS_CAL_START" || command == "COMPASS_DCD_SAVE" ||
      command == "COMPASS_DCD_AUTOSAVE_ON" ||
      command == "COMPASS_DCD_AUTOSAVE_OFF" ||
      command == "COMPASS_TARE_Z" || command == "COMPASS_TARE_SAVE" ||
      command == "COMPASS_TARE_CLEAR" || command == "OTA_FINISH" ||
      command == "OTA_ABORT" ||
      runtimeBleCommandHasPrefix(command, "SET_ANCHOR_PROFILE:") ||
      runtimeBleCommandHasPrefix(command, "SET_COMPASS_OFFSET:") ||
      runtimeBleCommandHasPrefix(command, "SET_STEP_MAXSPD:") ||
      runtimeBleCommandHasPrefix(command, "SET_STEP_ACCEL:") ||
      runtimeBleCommandHasPrefix(command, "OTA_BEGIN:")) {
    return RuntimeBleCommandScope::SERVICE;
  }

  if (command == "SIM_LIST" || command == "SIM_STATUS" || command == "SIM_REPORT" ||
      command == "SIM_ABORT" ||
      runtimeBleCommandHasPrefix(command, "SIM_RUN:") ||
      runtimeBleCommandHasPrefix(command, "SET_PHONE_GPS:")) {
    return RuntimeBleCommandScope::DEV_HIL;
  }

  return RuntimeBleCommandScope::UNKNOWN;
}
