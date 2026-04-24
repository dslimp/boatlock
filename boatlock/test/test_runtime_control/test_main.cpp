#include "RuntimeControl.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_resolve_core_mode_prefers_sim_over_other_modes() {
  TEST_ASSERT_EQUAL((int)CoreMode::SIM, (int)resolveCoreMode(true, true, true, true));
}

void test_resolve_core_mode_prefers_manual_over_anchor() {
  TEST_ASSERT_EQUAL((int)CoreMode::MANUAL, (int)resolveCoreMode(false, true, true, true));
}

void test_resolve_core_mode_uses_anchor_before_safe_hold() {
  TEST_ASSERT_EQUAL((int)CoreMode::ANCHOR, (int)resolveCoreMode(false, false, true, true));
}

void test_resolve_core_mode_uses_safe_hold_before_idle() {
  TEST_ASSERT_EQUAL((int)CoreMode::SAFE_HOLD, (int)resolveCoreMode(false, false, false, true));
  TEST_ASSERT_EQUAL((int)CoreMode::IDLE, (int)resolveCoreMode(false, false, false, false));
}

void test_core_mode_labels_match_release_surface() {
  TEST_ASSERT_EQUAL_STRING("IDLE", coreModeLabel(CoreMode::IDLE));
  TEST_ASSERT_EQUAL_STRING("HOLD", coreModeLabel(CoreMode::SAFE_HOLD));
  TEST_ASSERT_EQUAL_STRING("ANCHOR", coreModeLabel(CoreMode::ANCHOR));
  TEST_ASSERT_EQUAL_STRING("MANUAL", coreModeLabel(CoreMode::MANUAL));
  TEST_ASSERT_EQUAL_STRING("SIM", coreModeLabel(CoreMode::SIM));
}

void test_only_anchor_mode_uses_anchor_control() {
  TEST_ASSERT_TRUE(coreModeUsesAnchorControl(CoreMode::ANCHOR));
  TEST_ASSERT_FALSE(coreModeUsesAnchorControl(CoreMode::IDLE));
  TEST_ASSERT_FALSE(coreModeUsesAnchorControl(CoreMode::SAFE_HOLD));
  TEST_ASSERT_FALSE(coreModeUsesAnchorControl(CoreMode::MANUAL));
  TEST_ASSERT_FALSE(coreModeUsesAnchorControl(CoreMode::SIM));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_resolve_core_mode_prefers_sim_over_other_modes);
  RUN_TEST(test_resolve_core_mode_prefers_manual_over_anchor);
  RUN_TEST(test_resolve_core_mode_uses_anchor_before_safe_hold);
  RUN_TEST(test_resolve_core_mode_uses_safe_hold_before_idle);
  RUN_TEST(test_core_mode_labels_match_release_surface);
  RUN_TEST(test_only_anchor_mode_uses_anchor_control);
  return UNITY_END();
}
