#include "HilSimJson.h"
#include "HilSimRunner.h"
#include <string.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_sim_json_string_escapes_required_characters() {
  TEST_ASSERT_EQUAL_STRING(
      "\"quote\\\" slash\\\\ nl\\n tab\\t ctrl\\u0001\"",
      hilsim::simJsonString("quote\" slash\\ nl\n tab\t ctrl\001").c_str());
}

void test_sim_report_json_escapes_event_text() {
  hilsim::HilScenarioRunner runner;
  hilsim::SimScenario scenario;
  scenario.id = "json\"case";
  scenario.durationMs = 1;
  scenario.dtMs = 1;
  scenario.expect.requiredEvents = {"EVENT\"A"};

  TEST_ASSERT_TRUE(runner.start(scenario));
  runner.onControlEvent(0, "EVENT\"A", "line\nnext");
  while (runner.isRunning()) {
    runner.stepOnce();
  }

  const std::string report = runner.reportJson();
  TEST_ASSERT_NOT_NULL(strstr(report.c_str(), "\"id\":\"json\\\"case\""));
  TEST_ASSERT_NOT_NULL(strstr(report.c_str(), "\"reason\":\"PASS\""));
  TEST_ASSERT_NOT_NULL(strstr(report.c_str(), "\"code\":\"EVENT\\\"A\""));
  TEST_ASSERT_NOT_NULL(strstr(report.c_str(), "\"details\":\"line\\nnext\""));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sim_json_string_escapes_required_characters);
  RUN_TEST(test_sim_report_json_escapes_event_text);
  return UNITY_END();
}
