#include "RuntimeBleDataPacket.h"
#include <unity.h>

void test_notify_payload_length_accepts_valid_sizes() {
  TEST_ASSERT_EQUAL_UINT32(1, runtimeBleNotifyPayloadLength(1, 96));
  TEST_ASSERT_EQUAL_UINT32(70, runtimeBleNotifyPayloadLength(70, 96));
  TEST_ASSERT_EQUAL_UINT32(96, runtimeBleNotifyPayloadLength(96, 96));
}

void test_notify_payload_length_rejects_empty_and_oversized_values() {
  TEST_ASSERT_EQUAL_UINT32(0, runtimeBleNotifyPayloadLength(0, 96));
  TEST_ASSERT_EQUAL_UINT32(0, runtimeBleNotifyPayloadLength(97, 96));
  TEST_ASSERT_EQUAL_UINT32(0, runtimeBleNotifyPayloadLength(1, 0));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_notify_payload_length_accepts_valid_sizes);
  RUN_TEST(test_notify_payload_length_rejects_empty_and_oversized_values);
  return UNITY_END();
}
