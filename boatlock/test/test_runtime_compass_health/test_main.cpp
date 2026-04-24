#include "RuntimeCompassHealth.h"

#include <unity.h>
#include <climits>

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

void test_first_event_timeout_survives_unsigned_millis_wrap() {
  RuntimeCompassHealthInput input;
  input.compassReady = true;
  input.nowMs = ULONG_MAX - 1UL;
  input.readySinceMs = ULONG_MAX - 50UL;
  input.lastHeadingEventAgeMs = kRuntimeCompassNoEventAge;
  input.firstEventTimeoutMs = 1000UL;
  input.staleEventMs = 750;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::NONE, runtimeCompassLossReason(input));

  input.nowMs = input.readySinceMs + 1000UL;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::FIRST_EVENT_TIMEOUT, runtimeCompassLossReason(input));
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

void test_zero_timeouts_use_fail_closed_floors() {
  RuntimeCompassHealthInput input;
  input.compassReady = true;
  input.nowMs = 1999;
  input.readySinceMs = 1000;
  input.lastHeadingEventAgeMs = kRuntimeCompassNoEventAge;
  input.firstEventTimeoutMs = 0;
  input.staleEventMs = 0;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::NONE, runtimeCompassLossReason(input));

  input.nowMs = 2000;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::FIRST_EVENT_TIMEOUT, runtimeCompassLossReason(input));

  input.lastHeadingEventAgeMs = kRuntimeCompassStaleEventFloorMs - 1;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::NONE, runtimeCompassLossReason(input));

  input.lastHeadingEventAgeMs = kRuntimeCompassStaleEventFloorMs;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::EVENT_STALE, runtimeCompassLossReason(input));
}

void test_small_timeouts_are_floored() {
  RuntimeCompassHealthInput input;
  input.compassReady = true;
  input.nowMs = 1999;
  input.readySinceMs = 1000;
  input.lastHeadingEventAgeMs = kRuntimeCompassNoEventAge;
  input.firstEventTimeoutMs = 1;
  input.staleEventMs = 1;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::NONE, runtimeCompassLossReason(input));

  input.nowMs = 2000;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::FIRST_EVENT_TIMEOUT, runtimeCompassLossReason(input));

  input.lastHeadingEventAgeMs = kRuntimeCompassStaleEventFloorMs - 1;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::NONE, runtimeCompassLossReason(input));

  input.lastHeadingEventAgeMs = kRuntimeCompassStaleEventFloorMs;
  TEST_ASSERT_EQUAL(RuntimeCompassLossReason::EVENT_STALE, runtimeCompassLossReason(input));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_not_ready_never_reports_loss);
  RUN_TEST(test_first_event_timeout_is_fail_closed);
  RUN_TEST(test_ready_since_opens_new_first_event_window_after_reset);
  RUN_TEST(test_first_event_timeout_survives_unsigned_millis_wrap);
  RUN_TEST(test_stale_event_timeout_is_fail_closed);
  RUN_TEST(test_zero_timeouts_use_fail_closed_floors);
  RUN_TEST(test_small_timeouts_are_floored);
  return UNITY_END();
}
