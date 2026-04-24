#include "RuntimeSimCommand.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_non_sim_command_returns_none() {
  const RuntimeSimCommand command = parseRuntimeSimCommand("ANCHOR_ON");

  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::NONE, (int)command.type);
}

void test_simple_sim_commands_parse_without_payload() {
  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::LIST, (int)parseRuntimeSimCommand("SIM_LIST").type);
  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::STATUS, (int)parseRuntimeSimCommand("SIM_STATUS").type);
  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::ABORT, (int)parseRuntimeSimCommand("SIM_ABORT").type);
  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::REPORT, (int)parseRuntimeSimCommand("SIM_REPORT").type);
}

void test_run_command_parses_csv_payload() {
  const RuntimeSimCommand command = parseRuntimeSimCommand("SIM_RUN:S1_current_0p4_good,1");

  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::RUN, (int)command.type);
  TEST_ASSERT_TRUE(command.valid);
  TEST_ASSERT_EQUAL_STRING("S1_current_0p4_good", command.scenarioId.c_str());
  TEST_ASSERT_EQUAL(1, command.speedup);
}

void test_run_command_defaults_to_fast_mode() {
  const RuntimeSimCommand command = parseRuntimeSimCommand("SIM_RUN:S2_current_0p8_hard");

  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::RUN, (int)command.type);
  TEST_ASSERT_TRUE(command.valid);
  TEST_ASSERT_EQUAL_STRING("S2_current_0p8_hard", command.scenarioId.c_str());
  TEST_ASSERT_EQUAL(0, command.speedup);
}

void test_run_command_rejects_empty_payload() {
  const RuntimeSimCommand command = parseRuntimeSimCommand("SIM_RUN:");

  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::RUN, (int)command.type);
  TEST_ASSERT_FALSE(command.valid);
  TEST_ASSERT_TRUE(command.scenarioId.empty());
}

void test_run_command_rejects_unsupported_payload_forms() {
  const RuntimeSimCommand objectPayload =
      parseRuntimeSimCommand("SIM_RUN {id=S2_current_0p8_hard,speedup=1}");
  const RuntimeSimCommand spacedPayload = parseRuntimeSimCommand("SIM_RUN S2_current_0p8_hard,1");

  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::UNKNOWN, (int)objectPayload.type);
  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::UNKNOWN, (int)spacedPayload.type);
}

void test_run_command_rejects_invalid_speedup() {
  const RuntimeSimCommand badText = parseRuntimeSimCommand("SIM_RUN:S1,fast");
  const RuntimeSimCommand outOfRange = parseRuntimeSimCommand("SIM_RUN:S1,2");
  const RuntimeSimCommand emptySpeed = parseRuntimeSimCommand("SIM_RUN:S1,");

  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::RUN, (int)badText.type);
  TEST_ASSERT_FALSE(badText.valid);
  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::RUN, (int)outOfRange.type);
  TEST_ASSERT_FALSE(outOfRange.valid);
  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::RUN, (int)emptySpeed.type);
  TEST_ASSERT_FALSE(emptySpeed.valid);
}

void test_unknown_sim_command_is_classified() {
  const RuntimeSimCommand command = parseRuntimeSimCommand("SIM_FOO");

  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::UNKNOWN, (int)command.type);
}

void test_command_prefixes_are_not_accepted_as_known_commands() {
  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::UNKNOWN, (int)parseRuntimeSimCommand("SIM_RUNNER:S1").type);
  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::UNKNOWN, (int)parseRuntimeSimCommand("SIM_REPORT_NOW").type);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_non_sim_command_returns_none);
  RUN_TEST(test_simple_sim_commands_parse_without_payload);
  RUN_TEST(test_run_command_parses_csv_payload);
  RUN_TEST(test_run_command_defaults_to_fast_mode);
  RUN_TEST(test_run_command_rejects_empty_payload);
  RUN_TEST(test_run_command_rejects_unsupported_payload_forms);
  RUN_TEST(test_run_command_rejects_invalid_speedup);
  RUN_TEST(test_unknown_sim_command_is_classified);
  RUN_TEST(test_command_prefixes_are_not_accepted_as_known_commands);
  return UNITY_END();
}
