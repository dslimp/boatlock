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
  const RuntimeSimCommand command = parseRuntimeSimCommand("SIM_RUN:S1_current_0p4_good,3");

  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::RUN, (int)command.type);
  TEST_ASSERT_TRUE(command.valid);
  TEST_ASSERT_EQUAL_STRING("S1_current_0p4_good", command.scenarioId.c_str());
  TEST_ASSERT_EQUAL(3, command.speedup);
}

void test_run_command_parses_object_payload() {
  const RuntimeSimCommand command =
      parseRuntimeSimCommand("SIM_RUN {id=S2_current_0p8_hard,speedup=2}");

  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::RUN, (int)command.type);
  TEST_ASSERT_TRUE(command.valid);
  TEST_ASSERT_EQUAL_STRING("S2_current_0p8_hard", command.scenarioId.c_str());
  TEST_ASSERT_EQUAL(2, command.speedup);
}

void test_run_command_rejects_empty_payload() {
  const RuntimeSimCommand command = parseRuntimeSimCommand("SIM_RUN:");

  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::RUN, (int)command.type);
  TEST_ASSERT_FALSE(command.valid);
  TEST_ASSERT_TRUE(command.scenarioId.empty());
}

void test_unknown_sim_command_is_classified() {
  const RuntimeSimCommand command = parseRuntimeSimCommand("SIM_FOO");

  TEST_ASSERT_EQUAL((int)RuntimeSimCommandType::UNKNOWN, (int)command.type);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_non_sim_command_returns_none);
  RUN_TEST(test_simple_sim_commands_parse_without_payload);
  RUN_TEST(test_run_command_parses_csv_payload);
  RUN_TEST(test_run_command_parses_object_payload);
  RUN_TEST(test_run_command_rejects_empty_payload);
  RUN_TEST(test_unknown_sim_command_is_classified);
  return UNITY_END();
}
