#include "HilSimEvents.h"
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_event_log_suppresses_duplicate_across_rollover() {
  hilsim::SimEventLog log(10, 10, 1500);
  const unsigned long firstAt = ~0UL - 9UL;

  TEST_ASSERT_TRUE(log.record(firstAt, "EVENT_A", "same"));
  TEST_ASSERT_FALSE(log.record(5, "EVENT_A", "same"));
  TEST_ASSERT_TRUE(log.record(1600, "EVENT_A", "same"));

  TEST_ASSERT_EQUAL(2, log.events().size());
  TEST_ASSERT_EQUAL(firstAt, log.events()[0].atMs);
  TEST_ASSERT_EQUAL(1600, log.events()[1].atMs);
}

void test_event_log_keeps_bounded_tail_and_seen_tokens() {
  hilsim::SimEventLog log(3, 10, 0);

  TEST_ASSERT_TRUE(log.record(1, "EVENT_1", "detail_1"));
  TEST_ASSERT_TRUE(log.record(2, "EVENT_2", "detail_2"));
  TEST_ASSERT_TRUE(log.record(3, "EVENT_3", "detail_3"));
  TEST_ASSERT_TRUE(log.record(4, "EVENT_4", "detail_4"));

  TEST_ASSERT_EQUAL(3, log.events().size());
  TEST_ASSERT_EQUAL_STRING("EVENT_2", log.events()[0].code.c_str());
  TEST_ASSERT_EQUAL_STRING("EVENT_4", log.events()[2].code.c_str());
  TEST_ASSERT_TRUE(log.wasSeen("EVENT_1"));
  TEST_ASSERT_TRUE(log.wasSeen("detail_4"));
}

void test_event_log_clear_resets_history() {
  hilsim::SimEventLog log(3, 10, 1500);

  TEST_ASSERT_TRUE(log.record(10, "EVENT_A", "same"));
  log.clear();
  TEST_ASSERT_FALSE(log.wasSeen("EVENT_A"));
  TEST_ASSERT_EQUAL(0, log.events().size());
  TEST_ASSERT_TRUE(log.record(11, "EVENT_A", "same"));
  TEST_ASSERT_EQUAL(1, log.events().size());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_event_log_suppresses_duplicate_across_rollover);
  RUN_TEST(test_event_log_keeps_bounded_tail_and_seen_tokens);
  RUN_TEST(test_event_log_clear_resets_history);
  return UNITY_END();
}
