#include "RuntimeBleLogText.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_runtime_ble_log_value_uses_bounded_c_string_length() {
  const char payload[] = {'[', 'O', 'K', ']', '\n', '\0', 'x', 'x'};

  const std::string value = runtimeBleLogValue(payload, sizeof(payload));

  TEST_ASSERT_EQUAL_STRING("[OK]\n", value.c_str());
  TEST_ASSERT_EQUAL_UINT32(5, value.size());
}

void test_runtime_ble_log_value_handles_missing_terminator() {
  const char payload[] = {'A', 'B', 'C'};

  const std::string value = runtimeBleLogValue(payload, sizeof(payload));

  TEST_ASSERT_EQUAL_STRING("ABC", value.c_str());
  TEST_ASSERT_EQUAL_UINT32(3, value.size());
}

void test_runtime_ble_log_value_handles_null_input() {
  const std::string value = runtimeBleLogValue(nullptr, 8);

  TEST_ASSERT_TRUE(value.empty());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_runtime_ble_log_value_uses_bounded_c_string_length);
  RUN_TEST(test_runtime_ble_log_value_handles_missing_terminator);
  RUN_TEST(test_runtime_ble_log_value_handles_null_input);
  return UNITY_END();
}
