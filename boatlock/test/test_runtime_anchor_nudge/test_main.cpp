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

  TEST_ASSERT_TRUE(resolveRuntimeCardinalNudgeBearing("RIGHT", 350.0f, &bearing));
  TEST_ASSERT_EQUAL_FLOAT(80.0f, bearing);
}

void test_cardinal_bearing_accepts_forward_alias_and_rejects_unknown() {
  float bearing = 0.0f;

  TEST_ASSERT_TRUE(resolveRuntimeCardinalNudgeBearing("FORWARD", 42.0f, &bearing));
  TEST_ASSERT_EQUAL_FLOAT(42.0f, bearing);
  TEST_ASSERT_FALSE(resolveRuntimeCardinalNudgeBearing("UP", 42.0f, &bearing));
}

void test_nudge_distance_is_fixed_small_step() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.5f, kRuntimeAnchorNudgeMeters);
}

void test_nudge_normalization_uses_bounded_math() {
  TEST_ASSERT_EQUAL_FLOAT(5.0f, normalizeRuntimeAnchorNudgeBearing(725.0f));
  TEST_ASSERT_EQUAL_FLOAT(350.0f, normalizeRuntimeAnchorNudgeBearing(-10.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, normalizeRuntimeAnchorNudgeBearing(NAN));
  TEST_ASSERT_EQUAL_FLOAT(-179.0f, normalizeRuntimeAnchorNudgeLon(181.0f));
  TEST_ASSERT_EQUAL_FLOAT(179.0f, normalizeRuntimeAnchorNudgeLon(-181.0f));
  TEST_ASSERT_TRUE(isnan(normalizeRuntimeAnchorNudgeLon(NAN)));
}

void test_project_runtime_anchor_nudge_moves_point() {
  RuntimeAnchorNudgeTarget target;

  TEST_ASSERT_TRUE(projectRuntimeAnchorNudge(10.0f, 20.0f, 90.0f, &target));
  TEST_ASSERT_FLOAT_WITHIN(0.000001f, 10.0f, target.lat);
  TEST_ASSERT_TRUE(target.lon > 20.0f);
}

void test_project_runtime_anchor_nudge_rejects_invalid_source_point() {
  RuntimeAnchorNudgeTarget target;

  TEST_ASSERT_FALSE(projectRuntimeAnchorNudge(0.0f, 0.0f, 90.0f, &target));
  TEST_ASSERT_FALSE(projectRuntimeAnchorNudge(91.0f, 20.0f, 90.0f, &target));
  TEST_ASSERT_FALSE(projectRuntimeAnchorNudge(10.0f, 20.0f, NAN, &target));
}

void test_project_runtime_anchor_nudge_does_not_mutate_target_on_reject() {
  RuntimeAnchorNudgeTarget target;
  target.lat = 1.0f;
  target.lon = 2.0f;

  TEST_ASSERT_FALSE(projectRuntimeAnchorNudge(91.0f, 20.0f, 90.0f, &target));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, target.lat);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, target.lon);

  TEST_ASSERT_FALSE(projectRuntimeAnchorNudge(10.0f, 20.0f, NAN, &target));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, target.lat);
  TEST_ASSERT_EQUAL_FLOAT(2.0f, target.lon);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_cardinal_bearing_mapping_matches_heading_frame);
  RUN_TEST(test_cardinal_bearing_accepts_forward_alias_and_rejects_unknown);
  RUN_TEST(test_nudge_distance_is_fixed_small_step);
  RUN_TEST(test_nudge_normalization_uses_bounded_math);
  RUN_TEST(test_project_runtime_anchor_nudge_moves_point);
  RUN_TEST(test_project_runtime_anchor_nudge_rejects_invalid_source_point);
  RUN_TEST(test_project_runtime_anchor_nudge_does_not_mutate_target_on_reject);
  return UNITY_END();
}
