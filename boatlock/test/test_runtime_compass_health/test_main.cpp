#include "RuntimeCompassHealth.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_not_ready_never_reports_loss() {
  RuntimeCompassHealthInput input;
  input.compassReady = false;
  input.nowMs = 10000;
  input.readySinceMs = 1000;
  input.lastHeadingEventAgeMs = kRuntimeCompassNoEventAge;
  input.firstEventTimeoutMs = 1000;
  input.staleEventMs = 750;

  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::NONE, runtimeCompassLossReason(input));
}

void test_first_event_timeout_is_fail_closed() {
  RuntimeCompassHealthInput input;
  input.compassReady = true;
  input.nowMs = 1500;
  input.readySinceMs = 1000;
  input.lastHeadingEventAgeMs = kRuntimeCompassNoEventAge;
  input.firstEventTimeoutMs = 1000;
  input.staleEventMs = 750;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::NONE, runtimeCompassLossReason(input));

  input.nowMs = 2000;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::FIRST_EVENT_TIMEOUT, runtimeCompassLossReason(input));
}

void test_ready_since_opens_new_first_event_window_after_reset() {
  RuntimeCompassHealthInput input;
  input.compassReady = true;
  input.nowMs = 10000;
  input.readySinceMs = 9000;
  input.lastHeadingEventAgeMs = kRuntimeCompassNoEventAge;
  input.firstEventTimeoutMs = 3000;
  input.staleEventMs = 750;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::NONE, runtimeCompassLossReason(input));

  input.nowMs = 12000;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::FIRST_EVENT_TIMEOUT, runtimeCompassLossReason(input));

  input.readySinceMs = 12000;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::NONE, runtimeCompassLossReason(input));
}

void test_stale_event_timeout_is_fail_closed() {
  RuntimeCompassHealthInput input;
  input.compassReady = true;
  input.nowMs = 3000;
  input.readySinceMs = 1000;
  input.lastHeadingEventAgeMs = 749;
  input.firstEventTimeoutMs = 1000;
  input.staleEventMs = 750;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::NONE, runtimeCompassLossReason(input));

  input.lastHeadingEventAgeMs = 750;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::EVENT_STALE, runtimeCompassLossReason(input));
  TEST_ASSERT_EQUAL_STRING("EVENT_STALE", runtimeCompassLossReasonString(runtimeCompassLossReason(input)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_not_ready_never_reports_loss);
  RUN_TEST(test_first_event_timeout_is_fail_closed);
  RUN_TEST(test_ready_since_opens_new_first_event_window_after_reset);
  RUN_TEST(test_stale_event_timeout_is_fail_closed);
  return UNITY_END();
}
