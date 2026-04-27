#include "HilSimStatus.h"
#include <string.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_status_json_preserves_current_shape() {
  const std::string json =
      hilsim::buildSimStatusJson("S0", "RUNNING", 12.345f, 50, 1000, 25);

  TEST_ASSERT_EQUAL_STRING(
      "{\"id\":\"S0\",\"state\":\"RUNNING\",\"progress_pct\":12.35,"
      "\"sim_ms\":50,\"duration_ms\":1000,\"dt_ms\":25}",
      json.c_str());
}

void test_status_json_escapes_and_does_not_truncate_id() {
  const std::string id = std::string(260, 'i') + "\n\"";
  const std::string json = hilsim::buildSimStatusJson(id, "IDLE", 0.0f, 0, 0, 0);
  const std::string expectedId = "\"id\":\"" + std::string(260, 'i') + "\\n\\\"\"";

  TEST_ASSERT_NOT_NULL(strstr(json.c_str(), expectedId.c_str()));
  TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"state\":\"IDLE\""));
  TEST_ASSERT_EQUAL('}', json[json.size() - 1]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_status_json_preserves_current_shape);
  RUN_TEST(test_status_json_escapes_and_does_not_truncate_id);
  return UNITY_END();
}
