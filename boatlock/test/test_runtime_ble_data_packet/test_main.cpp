#include "RuntimeBleDataPacket.h"
#include <unity.h>

void test_fixed_notify_payload_length_accepts_exact_size() {
  TEST_ASSERT_EQUAL_UINT32(70, runtimeBleFixedNotifyPayloadLength(70, 70));
}

void test_fixed_notify_payload_length_rejects_non_exact_values() {
  TEST_ASSERT_EQUAL_UINT32(0, runtimeBleFixedNotifyPayloadLength(0, 70));
  TEST_ASSERT_EQUAL_UINT32(0, runtimeBleFixedNotifyPayloadLength(69, 70));
  TEST_ASSERT_EQUAL_UINT32(0, runtimeBleFixedNotifyPayloadLength(71, 70));
  TEST_ASSERT_EQUAL_UINT32(0, runtimeBleFixedNotifyPayloadLength(96, 70));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_fixed_notify_payload_length_accepts_exact_size);
  RUN_TEST(test_fixed_notify_payload_length_rejects_non_exact_values);
  return UNITY_END();
}
