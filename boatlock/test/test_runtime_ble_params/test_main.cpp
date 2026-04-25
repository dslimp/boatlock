#include "RuntimeBleParams.h"

#include <math.h>
#include <unity.h>

void logMessage(const char *, ...) {}

void setUp() {}
void tearDown() {}

void test_anchor_telemetry_accepts_valid_anchor_and_normalizes_heading() {
  AnchorControl anchor;
  anchor.anchorLat = 59.9386f;
  anchor.anchorLon = 30.3141f;
  anchor.anchorHeading = 370.0f;
  RuntimeBleLiveTelemetry telemetry;

  setRuntimeBleAnchorTelemetry(&telemetry, anchor);

  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 59.9386f, (float)telemetry.anchorLat);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 30.3141f, (float)telemetry.anchorLon);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, telemetry.anchorHeadingDeg);
}

void test_anchor_telemetry_clears_invalid_anchor_pair() {
  AnchorControl anchor;
  anchor.anchorLat = NAN;
  anchor.anchorLon = 30.3141f;
  anchor.anchorHeading = 90.0f;
  RuntimeBleLiveTelemetry telemetry;
  telemetry.anchorLat = 1.0;
  telemetry.anchorLon = 2.0;
  telemetry.anchorHeadingDeg = 3.0f;

  setRuntimeBleAnchorTelemetry(&telemetry, anchor);

  TEST_ASSERT_EQUAL_FLOAT(0.0f, (float)telemetry.anchorLat);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, (float)telemetry.anchorLon);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, telemetry.anchorHeadingDeg);

  anchor.anchorLat = 0.0f;
  anchor.anchorLon = 0.0f;
  setRuntimeBleAnchorTelemetry(&telemetry, anchor);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, (float)telemetry.anchorLat);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, (float)telemetry.anchorLon);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, telemetry.anchorHeadingDeg);
}

void test_anchor_telemetry_clears_nonfinite_heading() {
  AnchorControl anchor;
  anchor.anchorLat = 10.0f;
  anchor.anchorLon = 20.0f;
  anchor.anchorHeading = INFINITY;
  RuntimeBleLiveTelemetry telemetry;

  setRuntimeBleAnchorTelemetry(&telemetry, anchor);

  TEST_ASSERT_EQUAL_FLOAT(10.0f, (float)telemetry.anchorLat);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, (float)telemetry.anchorLon);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, telemetry.anchorHeadingDeg);
}

void test_runtime_ble_telemetry_u16_saturates_bad_values() {
  TEST_ASSERT_EQUAL_UINT16(0, runtimeBleTelemetryU16(NAN));
  TEST_ASSERT_EQUAL_UINT16(0, runtimeBleTelemetryU16(INFINITY));
  TEST_ASSERT_EQUAL_UINT16(0, runtimeBleTelemetryU16(-1.0f));
  TEST_ASSERT_EQUAL_UINT16(0, runtimeBleTelemetryU16(0.0f));
  TEST_ASSERT_EQUAL_UINT16(12, runtimeBleTelemetryU16(12.9f));
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, runtimeBleTelemetryU16(70000.0f));
}

void test_runtime_ble_telemetry_quality_rejects_invalid_ordinals() {
  TEST_ASSERT_EQUAL_UINT8(0, runtimeBleTelemetryQuality(-1, 3));
  TEST_ASSERT_EQUAL_UINT8(0, runtimeBleTelemetryQuality(4, 3));
  TEST_ASSERT_EQUAL_UINT8(0, runtimeBleTelemetryQuality(255, 3));
  TEST_ASSERT_EQUAL_UINT8(0, runtimeBleTelemetryQuality(3, 2));
  TEST_ASSERT_EQUAL_UINT8(2, runtimeBleTelemetryQuality(2, 3));
  TEST_ASSERT_EQUAL_UINT8(3, runtimeBleTelemetryQuality(3, 3));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_anchor_telemetry_accepts_valid_anchor_and_normalizes_heading);
  RUN_TEST(test_anchor_telemetry_clears_invalid_anchor_pair);
  RUN_TEST(test_anchor_telemetry_clears_nonfinite_heading);
  RUN_TEST(test_runtime_ble_telemetry_u16_saturates_bad_values);
  RUN_TEST(test_runtime_ble_telemetry_quality_rejects_invalid_ordinals);
  return UNITY_END();
}
