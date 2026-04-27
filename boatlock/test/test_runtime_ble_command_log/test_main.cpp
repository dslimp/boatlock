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
  TEST_ASSERT_FALSE(runtimeBleShouldLogCommand("MANUAL_SET:0,35,1000"));
}

void test_runtime_ble_command_log_redacts_sensitive_commands() {
  TEST_ASSERT_EQUAL_STRING("PAIR_SET:<redacted>",
                           runtimeBleLogCommandText("PAIR_SET:0123456789abcdef").c_str());
  TEST_ASSERT_EQUAL_STRING("AUTH_PROVE:<redacted>",
                           runtimeBleLogCommandText("AUTH_PROVE:deadbeef").c_str());
  TEST_ASSERT_EQUAL_STRING("SEC_CMD:<redacted>",
                           runtimeBleLogCommandText("SEC_CMD:00000001:deadbeef:STOP").c_str());
}

void test_runtime_ble_command_log_sanitizes_control_bytes() {
  TEST_ASSERT_EQUAL_STRING("STOP [EVENT] FAKE X",
                           runtimeBleLogCommandText("STOP\n[EVENT]\rFAKE\tX").c_str());
}

void test_runtime_ble_command_log_bounds_long_commands() {
  const std::string logged = runtimeBleLogCommandText("MANUAL_SET:12345678901234567890", 16);
  TEST_ASSERT_EQUAL_STRING("MANUAL_SET:12...", logged.c_str());
  TEST_ASSERT_EQUAL(16U, logged.size());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_runtime_ble_command_log_skips_heartbeat);
  RUN_TEST(test_runtime_ble_command_log_keeps_operator_commands);
  RUN_TEST(test_runtime_ble_command_log_skips_high_rate_manual_refreshes);
  RUN_TEST(test_runtime_ble_command_log_redacts_sensitive_commands);
  RUN_TEST(test_runtime_ble_command_log_sanitizes_control_bytes);
  RUN_TEST(test_runtime_ble_command_log_bounds_long_commands);
  return UNITY_END();
}
