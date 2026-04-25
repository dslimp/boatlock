#include "HilSimSensors.h"
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

void test_sensor_gnss_non_positive_rate_uses_one_hz() {
  hilsim::SensorConfig cfg;
  cfg.gnssHz = 0;
  cfg.posSigmaM = 0.0f;
  cfg.headingSigmaDeg = 0.0f;

  hilsim::SimSensorHub hub;
  hilsim::BoatWorldState st;
  st.xM = 2.0f;
  st.yM = -3.0f;
  hilsim::XorShift32 rng(1);

  hub.configure(cfg);
  hub.reset();
  hub.update(999, st, &rng);
  TEST_ASSERT_FALSE(hub.getFix().valid);

  hub.update(1000, st, &rng);
  TEST_ASSERT_TRUE(hub.getFix().valid);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, hub.getFix().xM);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.0f, hub.getFix().yM);
}

void test_sensor_heading_dropout_fails_closed() {
  hilsim::SensorConfig cfg;
  cfg.headingSigmaDeg = 0.0f;
  hilsim::TimedHeadingDropout dropout;
  dropout.atMs = 10;
  dropout.durationMs = 5;
  cfg.headingDropouts.push_back(dropout);

  hilsim::SimSensorHub hub;
  hilsim::BoatWorldState st;
  hilsim::XorShift32 rng(1);

  hub.configure(cfg);
  hub.reset();
  hub.update(10, st, &rng);

  TEST_ASSERT_FALSE(hub.getHeading().valid);
  TEST_ASSERT_EQUAL(999999UL, hub.getHeading().ageMs);
}

void test_sensor_gnss_dropout_freezes_last_fix_and_ages() {
  hilsim::SensorConfig cfg;
  cfg.gnssHz = 10;
  cfg.posSigmaM = 0.0f;
  cfg.headingSigmaDeg = 0.0f;
  hilsim::TimedDropout dropout;
  dropout.atMs = 200;
  dropout.durationMs = 200;
  cfg.dropouts.push_back(dropout);

  hilsim::SimSensorHub hub;
  hilsim::BoatWorldState st;
  hilsim::XorShift32 rng(1);

  hub.configure(cfg);
  hub.reset();
  hub.update(100, st, &rng);
  TEST_ASSERT_TRUE(hub.getFix().valid);

  st.xM = 5.0f;
  hub.update(200, st, &rng);
  TEST_ASSERT_TRUE(hub.getFix().valid);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, hub.getFix().xM);
  TEST_ASSERT_EQUAL(100UL, hub.getFix().ageMs);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sensor_reset_clears_heading_drift_baseline);
  RUN_TEST(test_sensor_gnss_non_positive_rate_uses_one_hz);
  RUN_TEST(test_sensor_heading_dropout_fails_closed);
  RUN_TEST(test_sensor_gnss_dropout_freezes_last_fix_and_ages);
  return UNITY_END();
}
