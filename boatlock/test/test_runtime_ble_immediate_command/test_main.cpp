#include "RuntimeBleImmediateCommand.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_immediate_command_parser_accepts_exact_commands() {
  TEST_ASSERT_EQUAL(static_cast<int>(RuntimeBleImmediateCommand::STREAM_START),
                    static_cast<int>(runtimeBleParseImmediateCommand("STREAM_START")));
  TEST_ASSERT_EQUAL(static_cast<int>(RuntimeBleImmediateCommand::STREAM_STOP),
                    static_cast<int>(runtimeBleParseImmediateCommand("STREAM_STOP")));
  TEST_ASSERT_EQUAL(static_cast<int>(RuntimeBleImmediateCommand::SNAPSHOT),
                    static_cast<int>(runtimeBleParseImmediateCommand("SNAPSHOT")));
}

void test_immediate_command_parser_rejects_prefix_suffix_and_control_bytes() {
  TEST_ASSERT_EQUAL(static_cast<int>(RuntimeBleImmediateCommand::NONE),
                    static_cast<int>(runtimeBleParseImmediateCommand("STREAM_START:1")));
  TEST_ASSERT_EQUAL(static_cast<int>(RuntimeBleImmediateCommand::NONE),
                    static_cast<int>(runtimeBleParseImmediateCommand("STREAM_STOP\n")));
  TEST_ASSERT_EQUAL(static_cast<int>(RuntimeBleImmediateCommand::NONE),
                    static_cast<int>(runtimeBleParseImmediateCommand("SNAPSHOT ")));
}

void test_immediate_command_parser_rejects_ordinary_control_commands() {
  TEST_ASSERT_EQUAL(static_cast<int>(RuntimeBleImmediateCommand::NONE),
                    static_cast<int>(runtimeBleParseImmediateCommand("STOP")));
  TEST_ASSERT_EQUAL(static_cast<int>(RuntimeBleImmediateCommand::NONE),
                    static_cast<int>(runtimeBleParseImmediateCommand("HEARTBEAT")));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_immediate_command_parser_accepts_exact_commands);
  RUN_TEST(test_immediate_command_parser_rejects_prefix_suffix_and_control_bytes);
  RUN_TEST(test_immediate_command_parser_rejects_ordinary_control_commands);
  return UNITY_END();
}
