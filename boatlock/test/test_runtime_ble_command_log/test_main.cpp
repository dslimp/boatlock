#include "RuntimeBleCommandLog.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_runtime_ble_command_log_skips_heartbeat() {
  TEST_ASSERT_FALSE(runtimeBleShouldLogCommand("HEARTBEAT"));
}

void test_runtime_ble_command_log_keeps_operator_commands() {
  TEST_ASSERT_TRUE(runtimeBleShouldLogCommand("STREAM_START"));
  TEST_ASSERT_TRUE(runtimeBleShouldLogCommand("MANUAL_SET:0.00,0,300"));
  TEST_ASSERT_TRUE(runtimeBleShouldLogCommand("STOP"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_runtime_ble_command_log_skips_heartbeat);
  RUN_TEST(test_runtime_ble_command_log_keeps_operator_commands);
  return UNITY_END();
}
