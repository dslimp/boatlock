#include "RuntimeStatus.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_runtime_status_reasons_collect_expected_flags() {
  RuntimeStatusInput input;
  input.gpsUnavailable = true;
  input.headingAvailable = false;
  input.driftAlert = true;
  input.anchorDeniedReason = "NO_HEADING";
  input.failsafeReason = "COMM_TIMEOUT";
  input.safetyReason = "NUDGE_OK";

  const std::string reasons = buildRuntimeStatusReasons(input);

  TEST_ASSERT_EQUAL_STRING("NO_GPS,NO_COMPASS,DRIFT_ALERT,NO_HEADING,COMM_TIMEOUT,NUDGE_OK",
                           reasons.c_str());
}

void test_runtime_status_reasons_use_gnss_reason_and_skip_none() {
  RuntimeStatusInput input;
  input.gnssWeak = true;
  input.gnssWeakReason = "GPS_HDOP_TOO_HIGH";
  input.anchorActive = true;
  input.anchorDeniedReason = "NONE";
  input.failsafeReason = "NONE";

  const std::string reasons = buildRuntimeStatusReasons(input);

  TEST_ASSERT_EQUAL_STRING("GPS_HDOP_TOO_HIGH", reasons.c_str());
}

void test_runtime_status_summary_uses_alert_warn_ok_levels() {
  RuntimeStatusInput input;

  const std::string okReasons = buildRuntimeStatusReasons(input);
  TEST_ASSERT_EQUAL_STRING("OK", buildRuntimeStatusSummary(input, okReasons));

  input.headingAvailable = false;
  const std::string warnReasons = buildRuntimeStatusReasons(input);
  TEST_ASSERT_EQUAL_STRING("WARN", buildRuntimeStatusSummary(input, warnReasons));

  input.headingAvailable = true;
  input.holdMode = true;
  const std::string alertReasons = buildRuntimeStatusReasons(input);
  TEST_ASSERT_EQUAL_STRING("ALERT", buildRuntimeStatusSummary(input, alertReasons));
}

void test_runtime_status_summary_keeps_info_reason_ok() {
  RuntimeStatusInput input;
  input.safetyReason = "NUDGE_OK";

  const std::string reasons = buildRuntimeStatusReasons(input);

  TEST_ASSERT_EQUAL_STRING("NUDGE_OK", reasons.c_str());
  TEST_ASSERT_EQUAL_STRING("OK", buildRuntimeStatusSummary(input, reasons));
}

void test_runtime_status_summary_treats_unknown_safety_reason_as_warning() {
  RuntimeStatusInput input;
  input.safetyReason = "UNKNOWN_SENSOR_NOTE";

  const std::string reasons = buildRuntimeStatusReasons(input);

  TEST_ASSERT_EQUAL_STRING("UNKNOWN_SENSOR_NOTE", reasons.c_str());
  TEST_ASSERT_EQUAL_STRING("WARN", buildRuntimeStatusSummary(input, reasons));
}

void test_runtime_status_reason_tokens_are_sanitized_and_bounded() {
  RuntimeStatusInput input;
  input.safetyReason = "BAD,NO_GPS\nX\tY";

  std::string reasons = buildRuntimeStatusReasons(input);

  TEST_ASSERT_EQUAL_STRING("BAD_NO_GPS_X_Y", reasons.c_str());
  TEST_ASSERT_EQUAL_STRING("WARN", buildRuntimeStatusSummary(input, reasons));

  input.safetyReason = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789TAIL";
  reasons = buildRuntimeStatusReasons(input);

  TEST_ASSERT_EQUAL_UINT32(kRuntimeStatusReasonMaxLen, reasons.size());
  TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345", reasons.c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_runtime_status_reasons_collect_expected_flags);
  RUN_TEST(test_runtime_status_reasons_use_gnss_reason_and_skip_none);
  RUN_TEST(test_runtime_status_summary_uses_alert_warn_ok_levels);
  RUN_TEST(test_runtime_status_summary_keeps_info_reason_ok);
  RUN_TEST(test_runtime_status_summary_treats_unknown_safety_reason_as_warning);
  RUN_TEST(test_runtime_status_reason_tokens_are_sanitized_and_bounded);
  return UNITY_END();
}
