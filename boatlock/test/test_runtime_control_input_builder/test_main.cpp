#include "RuntimeControlInputBuilder.h"

#include <math.h>

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_builder_keeps_manual_fields_and_disables_bearing_outside_anchor_mode() {
  const RuntimeControlState state = buildRuntimeControlState(
      1000, CoreMode::MANUAL, 35.0f, 120, true, true, 45.0f, true, 90.0f, true, 2.5f, 3, 8.0f);

  TEST_ASSERT_EQUAL((int)CoreMode::MANUAL, (int)state.mode);
  TEST_ASSERT_FALSE(state.autoControlActive);
  TEST_ASSERT_TRUE(state.hasHeading);
  TEST_ASSERT_FALSE(state.hasBearing);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, state.diffDeg);
  TEST_ASSERT_EQUAL_FLOAT(35.0f, state.input.manualAngleDeg);
  TEST_ASSERT_EQUAL(120, state.input.manualSpeed);
}

void test_builder_computes_anchor_diff_when_heading_and_bearing_exist() {
  const RuntimeControlState state = buildRuntimeControlState(
      2000, CoreMode::ANCHOR, -1, 0, true, true, 350.0f, true, 10.0f, true, 1.2f, 2, 4.0f);

  TEST_ASSERT_TRUE(state.autoControlActive);
  TEST_ASSERT_TRUE(state.hasHeading);
  TEST_ASSERT_TRUE(state.hasBearing);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, state.diffDeg);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, state.input.diffDeg);
}

void test_builder_zeroes_diff_when_heading_missing() {
  const RuntimeControlState state = buildRuntimeControlState(
      3000, CoreMode::ANCHOR, -1, 0, false, false, 0.0f, true, 100.0f, false, NAN, 0, 0.0f);

  TEST_ASSERT_FALSE(state.hasHeading);
  TEST_ASSERT_TRUE(state.hasBearing);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, state.diffDeg);
}

void test_builder_rejects_nonfinite_heading_as_unavailable() {
  const RuntimeControlState state = buildRuntimeControlState(
      4000, CoreMode::ANCHOR, -1, 0, true, true, NAN, true, 100.0f, true, 2.0f, 3, 5.0f);

  TEST_ASSERT_FALSE(state.hasHeading);
  TEST_ASSERT_TRUE(state.hasBearing);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, state.headingDeg);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, state.diffDeg);
  TEST_ASSERT_FALSE(state.input.hasHeading);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, state.input.headingDeg);
}

void test_builder_rejects_nonfinite_bearing_as_unavailable() {
  const RuntimeControlState state = buildRuntimeControlState(
      5000, CoreMode::ANCHOR, -1, 0, true, true, 20.0f, true, INFINITY, true, 2.0f, 3, 5.0f);

  TEST_ASSERT_TRUE(state.hasHeading);
  TEST_ASSERT_FALSE(state.hasBearing);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, state.diffDeg);
  TEST_ASSERT_FALSE(state.input.hasBearing);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, state.input.bearingDeg);
}

void test_builder_sanitizes_invalid_distance() {
  const RuntimeControlState state = buildRuntimeControlState(
      6000, CoreMode::ANCHOR, -1, 0, true, true, 20.0f, true, 30.0f, true, 2.0f, 3, NAN);

  TEST_ASSERT_EQUAL_FLOAT(0.0f, state.input.distanceM);
  TEST_ASSERT_FALSE(state.input.controlGpsAvailable);
}

void test_builder_keeps_manual_gps_flag_when_distance_is_invalid_outside_auto() {
  const RuntimeControlState state = buildRuntimeControlState(
      7000, CoreMode::MANUAL, 10.0f, 50, true, true, 20.0f, true, 30.0f, true, 2.0f, 3, NAN);

  TEST_ASSERT_FALSE(state.autoControlActive);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, state.input.distanceM);
  TEST_ASSERT_TRUE(state.input.controlGpsAvailable);
}

void test_builder_sanitizes_compass_accuracy_and_quality() {
  const RuntimeControlState highQuality = buildRuntimeControlState(
      8000, CoreMode::ANCHOR, -1, 0, true, true, 20.0f, true, 30.0f, true, 1.5f, 3, 4.0f);
  const RuntimeControlState badHighQuality = buildRuntimeControlState(
      8000, CoreMode::ANCHOR, -1, 0, true, true, 20.0f, true, 30.0f, true, INFINITY, 99, 4.0f);
  const RuntimeControlState badLowQuality = buildRuntimeControlState(
      8000, CoreMode::ANCHOR, -1, 0, true, true, 20.0f, true, 30.0f, true, -1.0f, -1, 4.0f);

  TEST_ASSERT_EQUAL_FLOAT(1.5f, highQuality.input.rvAccuracyDeg);
  TEST_ASSERT_EQUAL(3, highQuality.input.rvQuality);
  TEST_ASSERT_TRUE(isnan(badHighQuality.input.rvAccuracyDeg));
  TEST_ASSERT_EQUAL(0, badHighQuality.input.rvQuality);
  TEST_ASSERT_TRUE(isnan(badLowQuality.input.rvAccuracyDeg));
  TEST_ASSERT_EQUAL(0, badLowQuality.input.rvQuality);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_builder_keeps_manual_fields_and_disables_bearing_outside_anchor_mode);
  RUN_TEST(test_builder_computes_anchor_diff_when_heading_and_bearing_exist);
  RUN_TEST(test_builder_zeroes_diff_when_heading_missing);
  RUN_TEST(test_builder_rejects_nonfinite_heading_as_unavailable);
  RUN_TEST(test_builder_rejects_nonfinite_bearing_as_unavailable);
  RUN_TEST(test_builder_sanitizes_invalid_distance);
  RUN_TEST(test_builder_keeps_manual_gps_flag_when_distance_is_invalid_outside_auto);
  RUN_TEST(test_builder_sanitizes_compass_accuracy_and_quality);
  return UNITY_END();
}
