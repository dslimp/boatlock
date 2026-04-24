#include "RuntimeCompassRecovery.h"

#include <unity.h>

#include <vector>

void setUp() {}
void tearDown() {}

void test_attempt_runtime_compass_recovery_skips_when_retry_not_needed() {
  int restartCalls = 0;
  int initCalls = 0;

  const RuntimeCompassRecoveryOutcome out = attemptRuntimeCompassRecovery(
      false,
      [&]() { ++restartCalls; },
      [&]() {
        ++initCalls;
        return true;
      });

  TEST_ASSERT_FALSE(out.attempted);
  TEST_ASSERT_FALSE(out.ready);
  TEST_ASSERT_EQUAL(0, restartCalls);
  TEST_ASSERT_EQUAL(0, initCalls);
}

void test_attempt_runtime_compass_recovery_succeeds_on_first_try() {
  int restartCalls = 0;
  int initCalls = 0;

  const RuntimeCompassRecoveryOutcome out = attemptRuntimeCompassRecovery(
      true,
      [&]() { ++restartCalls; },
      [&]() {
        ++initCalls;
        return true;
      });

  TEST_ASSERT_TRUE(out.attempted);
  TEST_ASSERT_TRUE(out.ready);
  TEST_ASSERT_TRUE(out.shouldReloadCalibration);
  TEST_ASSERT_FALSE(out.shouldLogInventory);
  TEST_ASSERT_EQUAL(1, out.initAttempts);
  TEST_ASSERT_EQUAL(1, restartCalls);
  TEST_ASSERT_EQUAL(1, initCalls);
}

void test_attempt_runtime_compass_recovery_retries_once_before_success() {
  int restartCalls = 0;
  int initCalls = 0;

  const RuntimeCompassRecoveryOutcome out = attemptRuntimeCompassRecovery(
      true,
      [&]() { ++restartCalls; },
      [&]() {
        ++initCalls;
        return initCalls >= 2;
      });

  TEST_ASSERT_TRUE(out.attempted);
  TEST_ASSERT_TRUE(out.ready);
  TEST_ASSERT_TRUE(out.shouldReloadCalibration);
  TEST_ASSERT_FALSE(out.shouldLogInventory);
  TEST_ASSERT_EQUAL(2, out.initAttempts);
  TEST_ASSERT_EQUAL(2, restartCalls);
  TEST_ASSERT_EQUAL(2, initCalls);
}

void test_attempt_runtime_compass_recovery_logs_inventory_after_double_failure() {
  int restartCalls = 0;
  int initCalls = 0;

  const RuntimeCompassRecoveryOutcome out = attemptRuntimeCompassRecovery(
      true,
      [&]() { ++restartCalls; },
      [&]() {
        ++initCalls;
        return false;
      });

  TEST_ASSERT_TRUE(out.attempted);
  TEST_ASSERT_FALSE(out.ready);
  TEST_ASSERT_FALSE(out.shouldReloadCalibration);
  TEST_ASSERT_TRUE(out.shouldLogInventory);
  TEST_ASSERT_EQUAL(2, out.initAttempts);
  TEST_ASSERT_EQUAL(2, restartCalls);
  TEST_ASSERT_EQUAL(2, initCalls);
}

void test_runtime_compass_recovery_log_lines_are_stable() {
  TEST_ASSERT_EQUAL_STRING("[COMPASS] retry ready=1 source=BNO08x addr=0x4B",
                           buildRuntimeCompassRecoveryStatusLine(true, "BNO08x", 0x4B).c_str());
  TEST_ASSERT_EQUAL_STRING("[COMPASS] retry ready=0 source=BNO08x",
                           buildRuntimeCompassRecoveryStatusLine(false, "BNO08x", 0x4B).c_str());
  TEST_ASSERT_EQUAL_STRING("[COMPASS] heading offset=12.3",
                           buildRuntimeCompassOffsetLine(12.34f).c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_attempt_runtime_compass_recovery_skips_when_retry_not_needed);
  RUN_TEST(test_attempt_runtime_compass_recovery_succeeds_on_first_try);
  RUN_TEST(test_attempt_runtime_compass_recovery_retries_once_before_success);
  RUN_TEST(test_attempt_runtime_compass_recovery_logs_inventory_after_double_failure);
  RUN_TEST(test_runtime_compass_recovery_log_lines_are_stable);
  return UNITY_END();
}
