#include "AnchorDiagnostics.h"
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_emits_drift_alert_on_rising_edge() {
  AnchorDiagnostics d;
  auto e1 = d.update(1000, true, true, true, false, 20, 75);
  auto e2 = d.update(1200, true, true, true, true, 20, 75);
  auto e3 = d.update(1400, true, true, true, true, 20, 75);

  TEST_ASSERT_FALSE(e1.driftAlert);
  TEST_ASSERT_TRUE(e2.driftAlert);
  TEST_ASSERT_FALSE(e3.driftAlert);
}

void test_emits_sensor_timeout_when_sensors_bad_too_long() {
  AnchorDiagnostics d;
  auto e1 = d.update(1000, true, false, true, false, 20, 75);
  auto e2 = d.update(4001, true, false, true, false, 20, 75);
  auto e3 = d.update(4100, true, false, true, false, 20, 75);

  TEST_ASSERT_FALSE(e1.sensorTimeout);
  TEST_ASSERT_TRUE(e2.sensorTimeout);
  TEST_ASSERT_FALSE(e3.sensorTimeout);
}

void test_emits_control_saturated_with_cooldown() {
  AnchorDiagnostics d;
  auto e1 = d.update(1000, true, true, true, false, 75, 75);
  auto e2 = d.update(3600, true, true, true, false, 75, 75);
  auto e3 = d.update(5000, true, true, true, false, 75, 75);
  auto e4 = d.update(14050, true, true, true, false, 75, 75);

  TEST_ASSERT_FALSE(e1.controlSaturated);
  TEST_ASSERT_TRUE(e2.controlSaturated);
  TEST_ASSERT_FALSE(e3.controlSaturated);
  TEST_ASSERT_TRUE(e4.controlSaturated);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_emits_drift_alert_on_rising_edge);
  RUN_TEST(test_emits_sensor_timeout_when_sensors_bad_too_long);
  RUN_TEST(test_emits_control_saturated_with_cooldown);
  return UNITY_END();
}
