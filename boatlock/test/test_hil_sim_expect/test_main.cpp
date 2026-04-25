#include "HilSimExpect.h"
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_nan_requires_expected_failsafe_event() {
  hilsim::ScenarioExpect hold;
  hilsim::SimMetrics metrics;
  metrics.nanDetected = true;

  hilsim::SimExpectationEval eval = hilsim::evaluateSimMetrics(hold, metrics);
  TEST_ASSERT_FALSE(eval.pass);
  TEST_ASSERT_EQUAL_STRING("NAN_DETECTED", eval.reason.c_str());

  hilsim::ScenarioExpect expectedFail;
  expectedFail.type = hilsim::ScenarioExpect::Type::FAILSAFE;
  expectedFail.requiredEvents = {"INTERNAL_ERROR_NAN", "FAILSAFE_TRIGGERED"};
  eval = hilsim::evaluateSimMetrics(expectedFail, metrics);
  TEST_ASSERT_TRUE(eval.pass);
  TEST_ASSERT_EQUAL_STRING("PASS", eval.reason.c_str());
}

void test_expectation_reason_priority_is_stable() {
  hilsim::ScenarioExpect ex;
  ex.p95ErrorMaxM = 2.0f;
  ex.maxErrorMaxM = 4.0f;
  ex.timeInDeadbandMinPct = 70.0f;

  hilsim::SimMetrics metrics;
  metrics.outOfRangeCommand = true;
  metrics.p95ErrorM = 5.0f;
  metrics.maxErrorM = 8.0f;
  metrics.timeInDeadbandPct = 10.0f;

  const hilsim::SimExpectationEval eval = hilsim::evaluateSimMetrics(ex, metrics);
  TEST_ASSERT_FALSE(eval.pass);
  TEST_ASSERT_EQUAL_STRING("COMMAND_OUT_OF_RANGE", eval.reason.c_str());
}

void test_threshold_failures_report_specific_reason() {
  hilsim::ScenarioExpect ex;
  hilsim::SimMetrics metrics;

  ex.timeSaturatedMaxPct = 20.0f;
  metrics.timeSaturatedPct = 20.1f;
  hilsim::SimExpectationEval eval = hilsim::evaluateSimMetrics(ex, metrics);
  TEST_ASSERT_FALSE(eval.pass);
  TEST_ASSERT_EQUAL_STRING("SATURATED_PCT", eval.reason.c_str());

  ex = hilsim::ScenarioExpect();
  metrics = hilsim::SimMetrics();
  ex.maxBadGnssInAnchorMaxS = 1.0f;
  metrics.maxBadGnssInAnchorS = 1.1f;
  eval = hilsim::evaluateSimMetrics(ex, metrics);
  TEST_ASSERT_FALSE(eval.pass);
  TEST_ASSERT_EQUAL_STRING("BAD_GNSS_TIME", eval.reason.c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_nan_requires_expected_failsafe_event);
  RUN_TEST(test_expectation_reason_priority_is_stable);
  RUN_TEST(test_threshold_failures_report_specific_reason);
  return UNITY_END();
}
