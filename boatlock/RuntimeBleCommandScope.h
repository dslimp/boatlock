#pragma once

#include <cstdint>
#include <string>

enum class RuntimeBleCommandScope : uint8_t {
  UNKNOWN,
  RELEASE,
  DEV_HIL,
};

enum class RuntimeBleCommandProfile : uint8_t {
  RELEASE,
  ACCEPTANCE,
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
      command == "SIM_LIST" || command == "SIM_STATUS" ||
      command == "SIM_REPORT" || command == "SIM_ABORT" ||
      command == "RESET_COMPASS_OFFSET" || command == "SET_STEPPER_BOW" ||
      command == "COMPASS_CAL_START" || command == "COMPASS_DCD_SAVE" ||
      command == "COMPASS_DCD_AUTOSAVE_ON" ||
      command == "COMPASS_DCD_AUTOSAVE_OFF" ||
      command == "COMPASS_TARE_Z" || command == "COMPASS_TARE_SAVE" ||
      command == "COMPASS_TARE_CLEAR" || command == "OTA_FINISH" ||
      command == "OTA_ABORT" ||
      runtimeBleCommandHasPrefix(command, "SET_ANCHOR:") ||
      runtimeBleCommandHasPrefix(command, "SET_ANCHOR_PROFILE:") ||
      runtimeBleCommandHasPrefix(command, "SET_COMPASS_OFFSET:") ||
      runtimeBleCommandHasPrefix(command, "SET_STEP_MAXSPD:") ||
      runtimeBleCommandHasPrefix(command, "SET_STEP_ACCEL:") ||
      runtimeBleCommandHasPrefix(command, "MANUAL_TARGET:") ||
      runtimeBleCommandHasPrefix(command, "NUDGE_DIR:") ||
      runtimeBleCommandHasPrefix(command, "NUDGE_BRG:") ||
      runtimeBleCommandHasPrefix(command, "SET_HOLD_HEADING:") ||
      runtimeBleCommandHasPrefix(command, "PAIR_SET:") ||
      runtimeBleCommandHasPrefix(command, "AUTH_PROVE:") ||
      runtimeBleCommandHasPrefix(command, "SIM_RUN:") ||
      runtimeBleCommandHasPrefix(command, "OTA_BEGIN:")) {
    return RuntimeBleCommandScope::RELEASE;
  }

  if (runtimeBleCommandHasPrefix(command, "SET_PHONE_GPS:")) {
    return RuntimeBleCommandScope::DEV_HIL;
  }

  return RuntimeBleCommandScope::UNKNOWN;
}

inline const char* runtimeBleCommandScopeName(RuntimeBleCommandScope scope) {
  switch (scope) {
    case RuntimeBleCommandScope::RELEASE:
      return "release";
    case RuntimeBleCommandScope::DEV_HIL:
      return "dev_hil";
    case RuntimeBleCommandScope::UNKNOWN:
    default:
      return "unknown";
  }
}

inline const char* runtimeBleCommandProfileName(RuntimeBleCommandProfile profile) {
  switch (profile) {
    case RuntimeBleCommandProfile::RELEASE:
      return "release";
    case RuntimeBleCommandProfile::ACCEPTANCE:
      return "acceptance";
    default:
      return "unknown";
  }
}

inline bool runtimeBleCommandScopeAllowedInProfile(
    RuntimeBleCommandScope scope,
    RuntimeBleCommandProfile profile) {
  switch (scope) {
    case RuntimeBleCommandScope::RELEASE:
      return true;
    case RuntimeBleCommandScope::DEV_HIL:
      return profile == RuntimeBleCommandProfile::ACCEPTANCE;
    case RuntimeBleCommandScope::UNKNOWN:
    default:
      return false;
  }
}

inline bool runtimeBleCommandAllowedInProfile(
    const std::string& command,
    RuntimeBleCommandProfile profile) {
  return runtimeBleCommandScopeAllowedInProfile(runtimeBleClassifyCommand(command),
                                               profile);
}

inline RuntimeBleCommandProfile runtimeBleActiveCommandProfile() {
#if defined(BOATLOCK_COMMAND_PROFILE_ACCEPTANCE)
  return RuntimeBleCommandProfile::ACCEPTANCE;
#else
  return RuntimeBleCommandProfile::RELEASE;
#endif
}
