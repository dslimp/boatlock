#include "HilSimClock.h"
#include <limits.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_virtual_clock_starts_at_zero_and_sets_time() {
  hilsim::VirtualClock clock;

  TEST_ASSERT_EQUAL(0UL, clock.now_ms());
  clock.set(1234UL);
  TEST_ASSERT_EQUAL(1234UL, clock.now_ms());
}

void test_virtual_clock_advances_time() {
  hilsim::VirtualClock clock;

  clock.set(100UL);
  clock.advance(25UL);
  TEST_ASSERT_EQUAL(125UL, clock.now_ms());
}

void test_virtual_clock_uses_unsigned_wrap() {
  hilsim::VirtualClock clock;

  clock.set(ULONG_MAX - 4UL);
  clock.advance(10UL);
  TEST_ASSERT_EQUAL(5UL, clock.now_ms());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_virtual_clock_starts_at_zero_and_sets_time);
  RUN_TEST(test_virtual_clock_advances_time);
  RUN_TEST(test_virtual_clock_uses_unsigned_wrap);
  return UNITY_END();
}
