#include "DriftMonitor.h"
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_drift_speed_computed_from_distance_delta() {
  DriftMonitor monitor;
  monitor.update(1000, true, true, 3.0f, 8.0f, 20.0f);
  monitor.update(2000, true, true, 5.0f, 8.0f, 20.0f); // +2m in 1s

  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.60f, monitor.driftSpeedMps());
}

void test_drift_alert_and_fail_thresholds() {
  DriftMonitor monitor;
  monitor.update(1000, true, true, 9.0f, 8.0f, 20.0f);
  TEST_ASSERT_TRUE(monitor.alertActive());
  TEST_ASSERT_FALSE(monitor.failActive());

  monitor.update(2000, true, true, 21.0f, 8.0f, 20.0f);
  TEST_ASSERT_TRUE(monitor.alertActive());
  TEST_ASSERT_TRUE(monitor.failActive());
}

void test_drift_state_resets_when_anchor_or_gps_unavailable() {
  DriftMonitor monitor;
  monitor.update(1000, true, true, 9.0f, 8.0f, 20.0f);
  monitor.update(2000, true, true, 12.0f, 8.0f, 20.0f);
  TEST_ASSERT_TRUE(monitor.driftSpeedMps() > 0.0f);
  TEST_ASSERT_TRUE(monitor.alertActive());

  monitor.update(3000, false, true, 12.0f, 8.0f, 20.0f);
  TEST_ASSERT_FALSE(monitor.alertActive());
  TEST_ASSERT_FALSE(monitor.failActive());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, monitor.driftSpeedMps());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_drift_speed_computed_from_distance_delta);
  RUN_TEST(test_drift_alert_and_fail_thresholds);
  RUN_TEST(test_drift_state_resets_when_anchor_or_gps_unavailable);
  return UNITY_END();
}
