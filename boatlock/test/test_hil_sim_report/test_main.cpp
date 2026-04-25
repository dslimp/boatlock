#include "HilSimReport.h"
#include <string.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_report_json_preserves_metrics_and_event_shape() {
  hilsim::SimMetrics metrics;
  metrics.p95ErrorM = 1.25f;
  metrics.maxErrorM = 2.5f;
  metrics.timeInDeadbandPct = 80.0f;
  metrics.timeSaturatedPct = 4.0f;
  metrics.dirChangesPerMin = 3.0f;
  metrics.rampViolations = 2;
  metrics.maxBadGnssInAnchorS = 0.5f;

  std::vector<hilsim::SimEvent> events;
  hilsim::SimEvent ev;
  ev.atMs = 42;
  ev.code = "EVENT_A";
  ev.details = "detail";
  events.push_back(ev);

  const std::string json =
      hilsim::buildSimReportJson("S0", "DONE", true, "PASS", metrics, events, 64);

  TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"id\":\"S0\""));
  TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"state\":\"DONE\""));
  TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"pass\":true"));
  TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"p95_error_m\":1.250"));
  TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"ramp_violations\":2"));
  TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"events\":[{\"at_ms\":42"));
}

void test_report_json_keeps_bounded_event_tail() {
  hilsim::SimMetrics metrics;
  std::vector<hilsim::SimEvent> events;
  for (int i = 0; i < 3; ++i) {
    hilsim::SimEvent ev;
    ev.atMs = (unsigned long)i;
    ev.code = std::string("EVENT_") + (char)('0' + i);
    ev.details = "d";
    events.push_back(ev);
  }

  const std::string json =
      hilsim::buildSimReportJson("S0", "DONE", true, "PASS", metrics, events, 2);

  TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"events_total\":3,\"events_kept\":2"));
  TEST_ASSERT_NULL(strstr(json.c_str(), "\"EVENT_0\""));
  TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"EVENT_1\""));
  TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"EVENT_2\""));
}

void test_report_json_escapes_variable_strings() {
  hilsim::SimMetrics metrics;
  std::vector<hilsim::SimEvent> events;
  hilsim::SimEvent ev;
  ev.atMs = 1;
  ev.code = "E\"";
  ev.details = std::string(260, 'x') + "\n";
  events.push_back(ev);

  const std::string json =
      hilsim::buildSimReportJson("id\n", "DONE", false, "bad\"", metrics, events, 64);

  TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"id\":\"id\\n\""));
  TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"reason\":\"bad\\\"\""));
  TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"code\":\"E\\\"\""));
  TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\\n\"}]}"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_report_json_preserves_metrics_and_event_shape);
  RUN_TEST(test_report_json_keeps_bounded_event_tail);
  RUN_TEST(test_report_json_escapes_variable_strings);
  return UNITY_END();
}
