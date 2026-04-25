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

void test_sim_report_json_does_not_truncate_long_event_text() {
  hilsim::HilScenarioRunner runner;
  hilsim::SimScenario scenario;
  scenario.id = std::string(260, 'i');
  scenario.durationMs = 10;
  scenario.dtMs = 1;

  const std::string details = std::string(300, 'x') + "\n";
  TEST_ASSERT_TRUE(runner.start(scenario));
  runner.onControlEvent(0, "LONG_EVENT", details.c_str());
  runner.abort();

  const std::string report = runner.reportJson();
  const std::string expectedId = "\"id\":\"" + std::string(260, 'i') + "\"";
  const std::string expectedDetails = "\"details\":\"" + std::string(300, 'x') + "\\n\"";
  TEST_ASSERT_NOT_NULL(strstr(report.c_str(), expectedId.c_str()));
  TEST_ASSERT_NOT_NULL(strstr(report.c_str(), expectedDetails.c_str()));
  TEST_ASSERT_EQUAL('}', report[report.size() - 1]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sim_json_string_escapes_required_characters);
  RUN_TEST(test_sim_report_json_escapes_event_text);
  RUN_TEST(test_sim_report_json_does_not_truncate_long_event_text);
  return UNITY_END();
}
