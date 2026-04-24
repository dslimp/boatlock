#include "RuntimeAnchorGate.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_resolve_runtime_anchor_enable_denied_reason_prefers_anchor_point_first() {
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::NO_ANCHOR_POINT,
                    (int)resolveRuntimeAnchorEnableDeniedReason(
                        false, true, AnchorDeniedReason::GPS_HDOP_TOO_HIGH));
}

void test_resolve_runtime_anchor_enable_denied_reason_requires_heading_after_anchor_point() {
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::NO_HEADING,
                    (int)resolveRuntimeAnchorEnableDeniedReason(
                        true, false, AnchorDeniedReason::GPS_HDOP_TOO_HIGH));
}

void test_resolve_runtime_anchor_enable_denied_reason_returns_gnss_result_when_prereqs_hold() {
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::GPS_HDOP_TOO_HIGH,
                    (int)resolveRuntimeAnchorEnableDeniedReason(
                        true, true, AnchorDeniedReason::GPS_HDOP_TOO_HIGH));
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::NONE,
                    (int)resolveRuntimeAnchorEnableDeniedReason(
                        true, true, AnchorDeniedReason::NONE));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_resolve_runtime_anchor_enable_denied_reason_prefers_anchor_point_first);
  RUN_TEST(test_resolve_runtime_anchor_enable_denied_reason_requires_heading_after_anchor_point);
  RUN_TEST(test_resolve_runtime_anchor_enable_denied_reason_returns_gnss_result_when_prereqs_hold);
  return UNITY_END();
}
