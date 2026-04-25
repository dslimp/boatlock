#include "RuntimeBleCommandQueue.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_command_queue_copy_accepts_payload_with_terminator_room() {
  char dest[8] = {};

  TEST_ASSERT_TRUE(runtimeBleCopyCommandForQueue(dest, sizeof(dest), "1234567"));
  TEST_ASSERT_EQUAL_STRING("1234567", dest);
  TEST_ASSERT_EQUAL('\0', dest[7]);
}

void test_command_queue_copy_rejects_overlong_without_truncation() {
  char dest[8] = {'x', 'x', 'x', 'x', 'x', 'x', 'x', '\0'};

  TEST_ASSERT_FALSE(runtimeBleCopyCommandForQueue(dest, sizeof(dest), "12345678"));
  TEST_ASSERT_EQUAL('\0', dest[0]);
}

void test_command_queue_copy_rejects_missing_destination() {
  TEST_ASSERT_FALSE(runtimeBleCopyCommandForQueue(nullptr, 8, "STOP"));
}

void test_command_queue_copy_rejects_zero_length_slot() {
  char dest[1] = {'x'};

  TEST_ASSERT_FALSE(runtimeBleCopyCommandForQueue(dest, 0, "STOP"));
  TEST_ASSERT_EQUAL('x', dest[0]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_command_queue_copy_accepts_payload_with_terminator_room);
  RUN_TEST(test_command_queue_copy_rejects_overlong_without_truncation);
  RUN_TEST(test_command_queue_copy_rejects_missing_destination);
  RUN_TEST(test_command_queue_copy_rejects_zero_length_slot);
  return UNITY_END();
}
