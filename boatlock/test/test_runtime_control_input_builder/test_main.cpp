#include "RuntimeControlInputBuilder.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_builder_keeps_manual_fields_and_disables_bearing_outside_anchor_mode() {
  const RuntimeControlState state = buildRuntimeControlState(
      1000, CoreMode::MANUAL, 1, 120, true, true, 45.0f, true, 90.0f, true, 2.5f, 3, 8.0f);

  TEST_ASSERT_EQUAL((int)CoreMode::MANUAL, (int)state.mode);
  TEST_ASSERT_FALSE(state.autoControlActive);
  TEST_ASSERT_TRUE(state.hasHeading);
  TEST_ASSERT_FALSE(state.hasBearing);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, state.diffDeg);
  TEST_ASSERT_EQUAL(1, state.input.manualDir);
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

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_builder_keeps_manual_fields_and_disables_bearing_outside_anchor_mode);
  RUN_TEST(test_builder_computes_anchor_diff_when_heading_and_bearing_exist);
  RUN_TEST(test_builder_zeroes_diff_when_heading_missing);
  return UNITY_END();
}
