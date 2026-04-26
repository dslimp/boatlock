#include "RuntimeBleCommandScope.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void assertScope(RuntimeBleCommandScope expected, const char* command) {
  TEST_ASSERT_EQUAL((int)expected, (int)runtimeBleClassifyCommand(command));
}

void test_command_scope_classifies_release_exact_commands() {
  const char* commands[] = {
      "STREAM_START", "STREAM_STOP", "SNAPSHOT", "ANCHOR_ON", "ANCHOR_OFF",
      "STOP", "HEARTBEAT", "MANUAL_OFF", "PAIR_CLEAR", "AUTH_HELLO",
  };
  for (const char* command : commands) {
    assertScope(RuntimeBleCommandScope::RELEASE, command);
  }
}

void test_command_scope_classifies_release_prefix_commands_without_payload_validation() {
  const char* commands[] = {
      "SET_ANCHOR:55.1,37.2",
      "MANUAL_SET:1.5,5000,bad",
      "NUDGE_DIR:LEFT",
      "NUDGE_BRG:90",
      "SET_HOLD_HEADING:1",
      "PAIR_SET:not-hex",
      "AUTH_PROVE:short",
  };
  for (const char* command : commands) {
    assertScope(RuntimeBleCommandScope::RELEASE, command);
  }
}

void test_command_scope_classifies_service_exact_commands() {
  const char* commands[] = {
      "RESET_COMPASS_OFFSET",
      "SET_STEPPER_BOW",
      "COMPASS_CAL_START",
      "COMPASS_DCD_SAVE",
      "COMPASS_DCD_AUTOSAVE_ON",
      "COMPASS_DCD_AUTOSAVE_OFF",
      "COMPASS_TARE_Z",
      "COMPASS_TARE_SAVE",
      "COMPASS_TARE_CLEAR",
      "OTA_FINISH",
      "OTA_ABORT",
  };
  for (const char* command : commands) {
    assertScope(RuntimeBleCommandScope::SERVICE, command);
  }
}

void test_command_scope_classifies_service_prefix_commands_without_payload_validation() {
  const char* commands[] = {
      "SET_ANCHOR_PROFILE:quiet",
      "SET_COMPASS_OFFSET:not-a-number",
      "SET_STEP_MAXSPD:fast",
      "SET_STEP_ACCEL:fast",
      "OTA_BEGIN:bad",
  };
  for (const char* command : commands) {
    assertScope(RuntimeBleCommandScope::SERVICE, command);
  }
}

void test_command_scope_classifies_dev_hil_commands() {
  const char* commands[] = {
      "SIM_LIST",
      "SIM_STATUS",
      "SIM_REPORT",
      "SIM_ABORT",
      "SIM_RUN:S0,1",
      "SET_PHONE_GPS:not,a,fix",
  };
  for (const char* command : commands) {
    assertScope(RuntimeBleCommandScope::DEV_HIL, command);
  }
}

void test_command_scope_classifies_unknown_commands() {
  const char* commands[] = {
      "",
      " STREAM_START",
      "STREAM_START ",
      "SEC_CMD:1:2:STOP",
      "SEC_CMD:1:2:MANUAL_SET:0,0,300",
      "STREAM_START:1",
      "SIM_RUNNER:S1",
      "OTA_BEGIN_NOW",
      "SET_ANCHOR",
      "MANUAL_SET",
      "SIM_RUN",
      "SET_PHONE_GPS",
      "SET_ROUTE",
      "SET_HEADING",
      "EMU_COMPASS",
      "CALIB_COMPASS",
      "EXPORT_LOG",
      "MANUAL_DIR",
      "MANUAL_SPEED",
  };
  for (const char* command : commands) {
    assertScope(RuntimeBleCommandScope::UNKNOWN, command);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_command_scope_classifies_release_exact_commands);
  RUN_TEST(
      test_command_scope_classifies_release_prefix_commands_without_payload_validation);
  RUN_TEST(test_command_scope_classifies_service_exact_commands);
  RUN_TEST(
      test_command_scope_classifies_service_prefix_commands_without_payload_validation);
  RUN_TEST(test_command_scope_classifies_dev_hil_commands);
  RUN_TEST(test_command_scope_classifies_unknown_commands);
  return UNITY_END();
}
