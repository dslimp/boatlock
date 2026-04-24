#include "RuntimeCompassRetry.h"

#include <unity.h>
#include <climits>

void setUp() {}
void tearDown() {}

void test_retry_fires_only_after_interval_when_compass_missing() {
  RuntimeCompassRetry retry;

  TEST_ASSERT_FALSE(retry.shouldRetry(false, 1000, 5000));
  TEST_ASSERT_TRUE(retry.shouldRetry(false, 5000, 5000));
  TEST_ASSERT_FALSE(retry.shouldRetry(false, 9000, 5000));
  TEST_ASSERT_TRUE(retry.shouldRetry(false, 10000, 5000));
}

void test_retry_never_fires_when_compass_ready() {
  RuntimeCompassRetry retry;

  TEST_ASSERT_FALSE(retry.shouldRetry(true, 5000, 5000));
  TEST_ASSERT_FALSE(retry.shouldRetry(true, 10000, 5000));
}

void test_retry_interval_survives_unsigned_millis_wrap() {
  RuntimeCompassRetry retry;
  const unsigned long beforeWrap = ULONG_MAX - 50UL;

  TEST_ASSERT_TRUE(retry.shouldRetry(false, beforeWrap, 100));
  TEST_ASSERT_FALSE(retry.shouldRetry(false, beforeWrap + 99UL, 100));
  TEST_ASSERT_TRUE(retry.shouldRetry(false, beforeWrap + 100UL, 100));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_retry_fires_only_after_interval_when_compass_missing);
  RUN_TEST(test_retry_never_fires_when_compass_ready);
  RUN_TEST(test_retry_interval_survives_unsigned_millis_wrap);
  return UNITY_END();
}
