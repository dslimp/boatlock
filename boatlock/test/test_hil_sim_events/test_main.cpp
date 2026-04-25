#include "HilSimRunner.h"
#include <unity.h>

void setUp() {}
void tearDown() {}

static hilsim::SimScenario shortScenario() {
  hilsim::SimScenario scenario;
  scenario.id = "events";
  scenario.durationMs = 10;
  scenario.dtMs = 1;
  return scenario;
}

void test_duplicate_event_suppression_survives_rollover() {
  hilsim::HilScenarioRunner runner;
  TEST_ASSERT_TRUE(runner.start(shortScenario()));

  const unsigned long firstAt = ~0UL - 9UL;
  runner.onControlEvent(firstAt, "EVENT_A", "same");
  runner.onControlEvent(5, "EVENT_A", "same");
  runner.onControlEvent(1600, "EVENT_A", "same");
  runner.abort();

  TEST_ASSERT_EQUAL(2, runner.result().events.size());
  TEST_ASSERT_EQUAL(firstAt, runner.result().events[0].atMs);
  TEST_ASSERT_EQUAL(1600, runner.result().events[1].atMs);
}

void test_duplicate_event_suppression_keeps_different_details() {
  hilsim::HilScenarioRunner runner;
  TEST_ASSERT_TRUE(runner.start(shortScenario()));

  runner.onControlEvent(100, "EVENT_A", "one");
  runner.onControlEvent(101, "EVENT_A", "two");
  runner.abort();

  TEST_ASSERT_EQUAL(2, runner.result().events.size());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_duplicate_event_suppression_survives_rollover);
  RUN_TEST(test_duplicate_event_suppression_keeps_different_details);
  return UNITY_END();
}
