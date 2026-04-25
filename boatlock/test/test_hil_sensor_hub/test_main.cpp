#include "HilSimRunner.h"
#include <math.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_sensor_reset_clears_heading_drift_baseline() {
  hilsim::SensorConfig cfg;
  cfg.headingSigmaDeg = 0.0f;
  cfg.headingDriftDegS = 1.0f;

  hilsim::SimSensorHub hub;
  hilsim::BoatWorldState st;
  hilsim::XorShift32 rng(123);

  hub.configure(cfg);
  hub.reset();
  hub.update(1000, st, &rng);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, hub.getHeading().headingDeg);

  hub.reset();
  hub.update(0, st, &rng);
  TEST_ASSERT_TRUE(isfinite(hub.getHeading().headingDeg));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, hub.getHeading().headingDeg);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sensor_reset_clears_heading_drift_baseline);
  return UNITY_END();
}
