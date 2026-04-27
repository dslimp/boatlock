#include "RuntimeBleCommandScope.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void assertScope(RuntimeBleCommandScope expected, const char* command) {
  TEST_ASSERT_EQUAL((int)expected, (int)runtimeBleClassifyCommand(command));
}

void assertAllowed(bool expected,
                   RuntimeBleCommandProfile profile,
                   const char* command) {
  TEST_ASSERT_EQUAL(expected, runtimeBleCommandAllowedInProfile(command, profile));
}

void test_command_scope_classifies_release_exact_commands() {
  const char* commands[] = {
      "STREAM_START", "STREAM_STOP", "SNAPSHOT", "ANCHOR_ON", "ANCHOR_OFF",
      "STOP", "HEARTBEAT", "MANUAL_OFF", "PAIR_CLEAR", "AUTH_HELLO",
      "SIM_LIST", "SIM_STATUS", "SIM_REPORT", "SIM_ABORT",
  };
  for (const char* command : commands) {
    assertScope(RuntimeBleCommandScope::RELEASE, command);
  }
}

void test_command_scope_classifies_release_prefix_commands_without_payload_validation() {
  const char* commands[] = {
      "SET_ANCHOR:55.1,37.2",
      "MANUAL_TARGET:1.5,5000,bad",
      "NUDGE_DIR:LEFT",
      "NUDGE_BRG:90",
      "SET_HOLD_HEADING:1",
      "PAIR_SET:not-hex",
      "AUTH_PROVE:short",
      "SIM_RUN:S0,1",
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
      "SEC_CMD:1:2:MANUAL_TARGET:0,0,300",
      "STREAM_START:1",
      "SIM_RUNNER:S1",
      "OTA_BEGIN_NOW",
      "SET_ANCHOR",
      "MANUAL_TARGET",
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

void test_command_profile_names_are_stable_log_tokens() {
  TEST_ASSERT_EQUAL_STRING(
      "release",
      runtimeBleCommandProfileName(RuntimeBleCommandProfile::RELEASE));
  TEST_ASSERT_EQUAL_STRING(
      "service",
      runtimeBleCommandProfileName(RuntimeBleCommandProfile::SERVICE));
  TEST_ASSERT_EQUAL_STRING(
      "acceptance",
      runtimeBleCommandProfileName(RuntimeBleCommandProfile::ACCEPTANCE));
  TEST_ASSERT_EQUAL_STRING(
      "dev_hil",
      runtimeBleCommandScopeName(RuntimeBleCommandScope::DEV_HIL));
}

void test_default_active_profile_is_release_compatible() {
  TEST_ASSERT_EQUAL((int)RuntimeBleCommandProfile::RELEASE,
                    (int)runtimeBleActiveCommandProfile());
}

void test_release_profile_accepts_sim_but_rejects_service_and_external_sensor_injection() {
  assertAllowed(true, RuntimeBleCommandProfile::RELEASE, "ANCHOR_ON");
  assertAllowed(true, RuntimeBleCommandProfile::RELEASE, "SIM_RUN:S0,1");
  assertAllowed(true, RuntimeBleCommandProfile::RELEASE, "SIM_STATUS");
  assertAllowed(false, RuntimeBleCommandProfile::RELEASE, "OTA_BEGIN:bad");
  assertAllowed(false, RuntimeBleCommandProfile::RELEASE, "SET_ANCHOR_PROFILE:quiet");
  assertAllowed(false, RuntimeBleCommandProfile::RELEASE, "SET_PHONE_GPS:59,30");
  assertAllowed(false, RuntimeBleCommandProfile::RELEASE, "SET_ROUTE:old");
}

void test_service_profile_accepts_release_service_sim_and_rejects_external_sensor_injection() {
  assertAllowed(true, RuntimeBleCommandProfile::SERVICE, "ANCHOR_ON");
  assertAllowed(true, RuntimeBleCommandProfile::SERVICE, "SIM_STATUS");
  assertAllowed(true, RuntimeBleCommandProfile::SERVICE, "OTA_BEGIN:bad");
  assertAllowed(true, RuntimeBleCommandProfile::SERVICE, "SET_ANCHOR_PROFILE:quiet");
  assertAllowed(false, RuntimeBleCommandProfile::SERVICE, "SET_PHONE_GPS:59,30");
  assertAllowed(false, RuntimeBleCommandProfile::SERVICE, "SET_ROUTE:old");
}

void test_acceptance_profile_accepts_all_known_command_scopes_and_rejects_unknown() {
  assertAllowed(true, RuntimeBleCommandProfile::ACCEPTANCE, "ANCHOR_ON");
  assertAllowed(true, RuntimeBleCommandProfile::ACCEPTANCE, "OTA_BEGIN:bad");
  assertAllowed(true, RuntimeBleCommandProfile::ACCEPTANCE, "SET_ANCHOR_PROFILE:quiet");
  assertAllowed(true, RuntimeBleCommandProfile::ACCEPTANCE, "SET_PHONE_GPS:59,30");
  assertAllowed(true, RuntimeBleCommandProfile::ACCEPTANCE, "SIM_STATUS");
  assertAllowed(false, RuntimeBleCommandProfile::ACCEPTANCE, "SET_ROUTE:old");
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
  RUN_TEST(test_command_profile_names_are_stable_log_tokens);
  RUN_TEST(test_default_active_profile_is_release_compatible);
  RUN_TEST(test_release_profile_accepts_sim_but_rejects_service_and_external_sensor_injection);
  RUN_TEST(test_service_profile_accepts_release_service_sim_and_rejects_external_sensor_injection);
  RUN_TEST(test_acceptance_profile_accepts_all_known_command_scopes_and_rejects_unknown);
  return UNITY_END();
}
