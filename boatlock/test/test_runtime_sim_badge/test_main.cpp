#include "RuntimeSimBadge.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_running_state_has_no_badge() {
  RuntimeSimBadge badge;

  TEST_ASSERT_NULL(badge.update(true, "S1_current_0p4_good", 1, 1000, 15000));
  TEST_ASSERT_NULL(badge.current(1000));
}

void test_completion_creates_trimmed_badge() {
  RuntimeSimBadge badge;

  badge.update(true, "S1_current_0p4_good", 1, 1000, 15000);
  const char* shown = badge.update(false, "S1_current_0p4_good", 1, 2000, 15000);

  TEST_ASSERT_NOT_NULL(shown);
  TEST_ASSERT_EQUAL_STRING("SIM S1 PASS", shown);
}

void test_abort_and_fail_verdicts_are_stable() {
  RuntimeSimBadge failBadge;
  failBadge.update(true, "S2", 0, 1000, 15000);
  TEST_ASSERT_EQUAL_STRING("SIM S2 FAIL", failBadge.update(false, "S2", 0, 2000, 15000));

  RuntimeSimBadge abortBadge;
  abortBadge.update(true, "", -1, 1000, 15000);
  TEST_ASSERT_EQUAL_STRING("SIM SIM ABRT", abortBadge.update(false, "", -1, 2000, 15000));
}

void test_badge_expires_after_duration() {
  RuntimeSimBadge badge;

  badge.update(true, "S3", 1, 1000, 15000);
  TEST_ASSERT_NOT_NULL(badge.update(false, "S3", 1, 2000, 15000));
  TEST_ASSERT_NOT_NULL(badge.current(16999));
  TEST_ASSERT_NULL(badge.current(17000));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_running_state_has_no_badge);
  RUN_TEST(test_completion_creates_trimmed_badge);
  RUN_TEST(test_abort_and_fail_verdicts_are_stable);
  RUN_TEST(test_badge_expires_after_duration);
  return UNITY_END();
}
