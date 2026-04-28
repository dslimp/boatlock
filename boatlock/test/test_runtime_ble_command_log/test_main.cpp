#include "RuntimeBleCommandLog.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_runtime_ble_command_log_skips_heartbeat() {
  TEST_ASSERT_FALSE(runtimeBleShouldLogCommand("HEARTBEAT"));
}

void test_runtime_ble_command_log_keeps_operator_commands() {
  TEST_ASSERT_TRUE(runtimeBleShouldLogCommand("STREAM_START"));
  TEST_ASSERT_TRUE(runtimeBleShouldLogCommand("STOP"));
}

void test_runtime_ble_command_log_skips_high_rate_manual_refreshes() {
  TEST_ASSERT_FALSE(runtimeBleShouldLogCommand("MANUAL_TARGET:0,35,1000"));
}

void test_runtime_ble_command_log_sanitizes_control_bytes() {
  TEST_ASSERT_EQUAL_STRING("STOP [EVENT] FAKE X",
                           runtimeBleLogCommandText("STOP\n[EVENT]\rFAKE\tX").c_str());
}

void test_runtime_ble_command_log_bounds_long_commands() {
  const std::string logged = runtimeBleLogCommandText("MANUAL_TARGET:12345678901234567890", 16);
  TEST_ASSERT_EQUAL_STRING("MANUAL_TARGET...", logged.c_str());
  TEST_ASSERT_EQUAL(16U, logged.size());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_runtime_ble_command_log_skips_heartbeat);
  RUN_TEST(test_runtime_ble_command_log_keeps_operator_commands);
  RUN_TEST(test_runtime_ble_command_log_skips_high_rate_manual_refreshes);
  RUN_TEST(test_runtime_ble_command_log_sanitizes_control_bytes);
  RUN_TEST(test_runtime_ble_command_log_bounds_long_commands);
  return UNITY_END();
}
