#include "RuntimeAnchorNudge.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_cardinal_bearing_mapping_matches_heading_frame() {
  float bearing = 0.0f;

  TEST_ASSERT_TRUE(resolveRuntimeCardinalNudgeBearing("LEFT", 100.0f, &bearing));
  TEST_ASSERT_EQUAL_FLOAT(10.0f, bearing);

  TEST_ASSERT_TRUE(resolveRuntimeCardinalNudgeBearing("RIGHT", 100.0f, &bearing));
  TEST_ASSERT_EQUAL_FLOAT(190.0f, bearing);

  TEST_ASSERT_TRUE(resolveRuntimeCardinalNudgeBearing("BACK", 100.0f, &bearing));
  TEST_ASSERT_EQUAL_FLOAT(280.0f, bearing);
}

void test_cardinal_bearing_accepts_forward_alias_and_rejects_unknown() {
  float bearing = 0.0f;

  TEST_ASSERT_TRUE(resolveRuntimeCardinalNudgeBearing("FORWARD", 42.0f, &bearing));
  TEST_ASSERT_EQUAL_FLOAT(42.0f, bearing);
  TEST_ASSERT_FALSE(resolveRuntimeCardinalNudgeBearing("UP", 42.0f, &bearing));
}

void test_nudge_range_validation_is_strict() {
  TEST_ASSERT_FALSE(runtimeAnchorNudgeRangeValid(0.5f));
  TEST_ASSERT_TRUE(runtimeAnchorNudgeRangeValid(1.0f));
  TEST_ASSERT_TRUE(runtimeAnchorNudgeRangeValid(5.0f));
  TEST_ASSERT_FALSE(runtimeAnchorNudgeRangeValid(5.1f));
}

void test_project_runtime_anchor_nudge_moves_point() {
  RuntimeAnchorNudgeTarget target;

  TEST_ASSERT_TRUE(projectRuntimeAnchorNudge(0.0f, 0.0f, 90.0f, 5.0f, &target));
  TEST_ASSERT_FLOAT_WITHIN(0.000001f, 0.0f, target.lat);
  TEST_ASSERT_TRUE(target.lon > 0.0f);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_cardinal_bearing_mapping_matches_heading_frame);
  RUN_TEST(test_cardinal_bearing_accepts_forward_alias_and_rejects_unknown);
  RUN_TEST(test_nudge_range_validation_is_strict);
  RUN_TEST(test_project_runtime_anchor_nudge_moves_point);
  return UNITY_END();
}
