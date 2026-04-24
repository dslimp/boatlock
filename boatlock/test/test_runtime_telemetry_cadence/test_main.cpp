#include "RuntimeTelemetryCadence.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_ui_refresh_fires_only_after_interval() {
  RuntimeTelemetryCadence cadence;

  TEST_ASSERT_FALSE(cadence.shouldRefreshUi(100, 120));
  TEST_ASSERT_TRUE(cadence.shouldRefreshUi(120, 120));
  TEST_ASSERT_FALSE(cadence.shouldRefreshUi(200, 120));
  TEST_ASSERT_TRUE(cadence.shouldRefreshUi(240, 120));
}

void test_ble_notify_fires_only_after_interval() {
  RuntimeTelemetryCadence cadence;

  TEST_ASSERT_FALSE(cadence.shouldNotifyBle(500, 1000));
  TEST_ASSERT_TRUE(cadence.shouldNotifyBle(1000, 1000));
  TEST_ASSERT_FALSE(cadence.shouldNotifyBle(1500, 1000));
  TEST_ASSERT_TRUE(cadence.shouldNotifyBle(2000, 1000));
}

void test_ui_and_ble_cadence_are_independent() {
  RuntimeTelemetryCadence cadence;

  TEST_ASSERT_TRUE(cadence.shouldRefreshUi(120, 120));
  TEST_ASSERT_FALSE(cadence.shouldNotifyBle(120, 1000));
  TEST_ASSERT_FALSE(cadence.shouldRefreshUi(200, 120));
  TEST_ASSERT_TRUE(cadence.shouldNotifyBle(1000, 1000));
  TEST_ASSERT_TRUE(cadence.shouldRefreshUi(240, 120));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_ui_refresh_fires_only_after_interval);
  RUN_TEST(test_ble_notify_fires_only_after_interval);
  RUN_TEST(test_ui_and_ble_cadence_are_independent);
  return UNITY_END();
}
