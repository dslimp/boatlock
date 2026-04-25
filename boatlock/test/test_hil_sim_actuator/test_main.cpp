#include "HilSimActuator.h"
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_actuator_capture_starts_with_safe_default() {
  hilsim::ActuatorCapture actuator;

  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, actuator.last().thrust);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, actuator.last().steerDeg);
  TEST_ASSERT_TRUE(actuator.last().stop);
}

void test_actuator_capture_stores_last_command() {
  hilsim::ActuatorCapture actuator;
  ActuatorCmd cmd;
  cmd.thrust = 0.42f;
  cmd.steerDeg = -35.0f;
  cmd.stop = true;

  actuator.apply(cmd);

  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.42f, actuator.last().thrust);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -35.0f, actuator.last().steerDeg);
  TEST_ASSERT_TRUE(actuator.last().stop);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_actuator_capture_starts_with_safe_default);
  RUN_TEST(test_actuator_capture_stores_last_command);
  return UNITY_END();
}
