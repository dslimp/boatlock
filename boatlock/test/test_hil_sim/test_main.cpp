#include "HilSimRunner.h"
#include <unity.h>

using hilsim::HilScenarioRunner;
using hilsim::SimScenario;

void setUp() {}
void tearDown() {}

static bool findScenario(const char* id, SimScenario* out) {
  if (!out) return false;
  const auto scenarios = hilsim::defaultScenarios();
  for (const SimScenario& s : scenarios) {
    if (s.id == id) {
      *out = s;
      return true;
    }
  }
  return false;
}

static HilScenarioRunner::Result runScenario(const SimScenario& s) {
  HilScenarioRunner runner;
  TEST_ASSERT_TRUE(runner.start(s));
  int guard = 0;
  while (runner.isRunning() && guard < 100000) {
    runner.stepOnce();
    ++guard;
  }
  TEST_ASSERT_TRUE(guard < 100000);
  return runner.result();
}

void test_deterministic_repeatable_for_same_seed() {
  SimScenario s;
  TEST_ASSERT_TRUE(findScenario("S1_current_0p4_good", &s));

  const auto r1 = runScenario(s);
  const auto r2 = runScenario(s);

  TEST_ASSERT_EQUAL(r1.pass, r2.pass);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, r1.metrics.p95ErrorM, r2.metrics.p95ErrorM);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, r1.metrics.maxErrorM, r2.metrics.maxErrorM);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, r1.metrics.timeInDeadbandPct, r2.metrics.timeInDeadbandPct);
}

void test_dropout_produces_stale_and_failsafe_events() {
  SimScenario s;
  TEST_ASSERT_TRUE(findScenario("S4_gnss_dropout_3s", &s));
  const auto r = runScenario(s);

  bool hasStale = false;
  bool hasFailsafe = false;
  for (const auto& ev : r.events) {
    if (ev.code == "GPS_DATA_STALE" || ev.details == "GPS_DATA_STALE") {
      hasStale = true;
    }
    if (ev.code == "FAILSAFE_TRIGGERED" || ev.details == "FAILSAFE_TRIGGERED") {
      hasFailsafe = true;
    }
  }

  TEST_ASSERT_TRUE(hasStale);
  TEST_ASSERT_TRUE(hasFailsafe);
}

void test_control_loop_stall_produces_timeout_event() {
  SimScenario s;
  TEST_ASSERT_TRUE(findScenario("S8_control_loop_stall", &s));
  const auto r = runScenario(s);

  bool hasLoopTimeout = false;
  for (const auto& ev : r.events) {
    if (ev.code == "CONTROL_LOOP_TIMEOUT" || ev.details == "CONTROL_LOOP_TIMEOUT") {
      hasLoopTimeout = true;
      break;
    }
  }
  TEST_ASSERT_TRUE(hasLoopTimeout);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_deterministic_repeatable_for_same_seed);
  RUN_TEST(test_dropout_produces_stale_and_failsafe_events);
  RUN_TEST(test_control_loop_stall_produces_timeout_event);
  return UNITY_END();
}

