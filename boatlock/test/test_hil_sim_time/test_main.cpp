#include "HilSimTime.h"
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_time_window_excludes_zero_duration() {
  TEST_ASSERT_FALSE(hilsim::simTimeWindowContains(100, 100, 0));
}

void test_time_window_includes_start_and_excludes_end() {
  TEST_ASSERT_TRUE(hilsim::simTimeWindowContains(100, 100, 20));
  TEST_ASSERT_TRUE(hilsim::simTimeWindowContains(119, 100, 20));
  TEST_ASSERT_FALSE(hilsim::simTimeWindowContains(120, 100, 20));
}

void test_time_window_rejects_before_start() {
  TEST_ASSERT_FALSE(hilsim::simTimeWindowContains(99, 100, 20));
}

void test_time_window_survives_unsigned_rollover() {
  const unsigned long at = ~0UL - 9UL;

  TEST_ASSERT_TRUE(hilsim::simTimeWindowContains(at, at, 20));
  TEST_ASSERT_TRUE(hilsim::simTimeWindowContains(5, at, 20));
  TEST_ASSERT_FALSE(hilsim::simTimeWindowContains(11, at, 20));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_time_window_excludes_zero_duration);
  RUN_TEST(test_time_window_includes_start_and_excludes_end);
  RUN_TEST(test_time_window_rejects_before_start);
  RUN_TEST(test_time_window_survives_unsigned_rollover);
  return UNITY_END();
}
