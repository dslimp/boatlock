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

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_anchor_telemetry_accepts_valid_anchor_and_normalizes_heading);
  RUN_TEST(test_anchor_telemetry_clears_invalid_anchor_pair);
  RUN_TEST(test_anchor_telemetry_clears_nonfinite_heading);
  return UNITY_END();
}
