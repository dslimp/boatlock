#include "StepperControl.h"
#include <unity.h>

void logMessage(const char*, ...) {}

void setUp() {
  mockSetMillis(1);
}
void tearDown() {}

void test_normalize180_wraps_into_signed_range() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -170.0f, StepperControl::normalize180(190.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 170.0f, StepperControl::normalize180(-190.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 180.0f, StepperControl::normalize180(180.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -180.0f, StepperControl::normalize180(-180.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, StepperControl::normalize180(725.0f));
}

void test_point_to_bearing_uses_shortest_path() {
  StepperControl c(2, 4, 6, 16);
  c.stepsPerRev = 4096;
  c.stepper.moveTo(4000);
  while (c.stepper.distanceToGo() != 0) {
    c.stepper.run();
  }
  TEST_ASSERT_EQUAL_INT(4000, c.stepper.currentPosition());

  c.pointToBearing(0.0f, 350.0f);
  long d = c.stepper.distanceToGo();
  TEST_ASSERT_TRUE(d != 0);
  TEST_ASSERT_TRUE(d < 0);
  TEST_ASSERT_LESS_OR_EQUAL_INT(220, abs((int)d));
}

void test_direction_flip_preempts_old_target() {
  StepperControl c(2, 4, 6, 16);
  c.stepsPerRev = 4096;

  c.pointToBearing(90.0f, 0.0f);
  const long first = c.stepper.distanceToGo();
  TEST_ASSERT_TRUE(first < 0);

  c.pointToBearing(270.0f, 0.0f);
  const long second = c.stepper.distanceToGo();
  TEST_ASSERT_TRUE(second > 0);
}

void test_run_releases_coils_after_idle_timeout() {
  StepperControl c(2, 4, 6, 16);
  c.outputsEnabled = true;

  c.run();
  TEST_ASSERT_TRUE(c.outputsEnabled);

  mockAdvanceMillis(StepperControl::COIL_RELEASE_DELAY_MS + 1);
  c.run();
  TEST_ASSERT_FALSE(c.outputsEnabled);
}

void test_cancel_move_starts_idle_release_timer() {
  StepperControl c(2, 4, 6, 16);
  c.outputsEnabled = true;
  c.idleSinceMs = 0;

  mockSetMillis(100);
  c.cancelMove();

  TEST_ASSERT_EQUAL(100, c.idleSinceMs);
  mockAdvanceMillis(StepperControl::COIL_RELEASE_DELAY_MS + 1);
  c.run();
  TEST_ASSERT_FALSE(c.outputsEnabled);
}

void test_start_manual_zero_does_not_enable_outputs() {
  StepperControl c(2, 4, 6, 16);
  c.outputsEnabled = false;

  c.startManual(0);

  TEST_ASSERT_FALSE(c.manual);
  TEST_ASSERT_FALSE(c.outputsEnabled);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_normalize180_wraps_into_signed_range);
  RUN_TEST(test_point_to_bearing_uses_shortest_path);
  RUN_TEST(test_direction_flip_preempts_old_target);
  RUN_TEST(test_run_releases_coils_after_idle_timeout);
  RUN_TEST(test_cancel_move_starts_idle_release_timer);
  RUN_TEST(test_start_manual_zero_does_not_enable_outputs);
  return UNITY_END();
}
