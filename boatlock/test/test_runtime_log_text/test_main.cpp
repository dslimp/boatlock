#include "RuntimeLogText.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_runtime_log_formatted_length_handles_errors_and_empty_buffers() {
  TEST_ASSERT_EQUAL_UINT32(0, runtimeLogFormattedLength(-1, 256));
  TEST_ASSERT_EQUAL_UINT32(0, runtimeLogFormattedLength(0, 256));
  TEST_ASSERT_EQUAL_UINT32(0, runtimeLogFormattedLength(10, 0));
}

void test_runtime_log_formatted_length_uses_vsnprintf_contract() {
  TEST_ASSERT_EQUAL_UINT32(5, runtimeLogFormattedLength(5, 256));
  TEST_ASSERT_EQUAL_UINT32(255, runtimeLogFormattedLength(300, 256));
  TEST_ASSERT_EQUAL_UINT32(1, runtimeLogFormattedLength(9, 2));
}

void test_runtime_log_ble_forward_filter() {
  TEST_ASSERT_FALSE(runtimeLogShouldForwardToBle(nullptr));
  TEST_ASSERT_FALSE(runtimeLogShouldForwardToBle(""));
  TEST_ASSERT_TRUE(runtimeLogShouldForwardToBle("[BLE] advertising started"));
  TEST_ASSERT_TRUE(runtimeLogShouldForwardToBle("[EVENT] STOP"));
  TEST_ASSERT_TRUE(runtimeLogShouldForwardToBle("BLE without bracket"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_runtime_log_formatted_length_handles_errors_and_empty_buffers);
  RUN_TEST(test_runtime_log_formatted_length_uses_vsnprintf_contract);
  RUN_TEST(test_runtime_log_ble_forward_filter);
  return UNITY_END();
}
