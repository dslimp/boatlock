#include "RuntimeControl.h"

#include <stdio.h>

#include <unity.h>

void setUp() {}
void tearDown() {}

struct CoreModeCase {
  bool sim;
  bool manual;
  bool anchor;
  bool safeHold;
  CoreMode expected;
};

void test_resolve_core_mode_is_deterministic_for_all_flag_combinations() {
  const CoreModeCase cases[] = {
      {false, false, false, false, CoreMode::IDLE},
      {false, false, false, true, CoreMode::SAFE_HOLD},
      {false, false, true, false, CoreMode::ANCHOR},
      {false, false, true, true, CoreMode::ANCHOR},
      {false, true, false, false, CoreMode::MANUAL},
      {false, true, false, true, CoreMode::MANUAL},
      {false, true, true, false, CoreMode::MANUAL},
      {false, true, true, true, CoreMode::MANUAL},
      {true, false, false, false, CoreMode::SIM},
      {true, false, false, true, CoreMode::SIM},
      {true, false, true, false, CoreMode::SIM},
      {true, false, true, true, CoreMode::SIM},
      {true, true, false, false, CoreMode::SIM},
      {true, true, false, true, CoreMode::SIM},
      {true, true, true, false, CoreMode::SIM},
      {true, true, true, true, CoreMode::SIM},
  };

  char message[96];
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    const CoreModeCase& c = cases[i];
    snprintf(message,
             sizeof(message),
             "case=%u sim=%d manual=%d anchor=%d hold=%d",
             static_cast<unsigned>(i),
             c.sim,
             c.manual,
             c.anchor,
             c.safeHold);
    TEST_ASSERT_EQUAL_MESSAGE((int)c.expected,
                              (int)resolveCoreMode(c.sim, c.manual, c.anchor, c.safeHold),
                              message);
  }
}

void test_core_mode_labels_match_release_surface() {
  TEST_ASSERT_EQUAL_STRING("IDLE", coreModeLabel(CoreMode::IDLE));
  TEST_ASSERT_EQUAL_STRING("HOLD", coreModeLabel(CoreMode::SAFE_HOLD));
  TEST_ASSERT_EQUAL_STRING("ANCHOR", coreModeLabel(CoreMode::ANCHOR));
  TEST_ASSERT_EQUAL_STRING("MANUAL", coreModeLabel(CoreMode::MANUAL));
  TEST_ASSERT_EQUAL_STRING("SIM", coreModeLabel(CoreMode::SIM));
}

void test_core_mode_label_fails_closed_for_unknown_value() {
  TEST_ASSERT_EQUAL_STRING("IDLE", coreModeLabel((CoreMode)255));
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
  RUN_TEST(test_resolve_core_mode_is_deterministic_for_all_flag_combinations);
  RUN_TEST(test_core_mode_labels_match_release_surface);
  RUN_TEST(test_core_mode_label_fails_closed_for_unknown_value);
  RUN_TEST(test_only_anchor_mode_uses_anchor_control);
  return UNITY_END();
}
