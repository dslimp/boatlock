#include "AnchorReasons.h"
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_denied_reason_mapping_from_gnss_gate() {
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::GPS_NO_FIX,
                    (int)deniedReasonFromGnss(GnssQualityFailReason::NO_FIX));
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::GPS_DATA_STALE,
                    (int)deniedReasonFromGnss(GnssQualityFailReason::DATA_STALE));
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::GPS_SATS_TOO_LOW,
                    (int)deniedReasonFromGnss(GnssQualityFailReason::SATS_TOO_LOW));
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::GPS_HDOP_MISSING,
                    (int)deniedReasonFromGnss(GnssQualityFailReason::HDOP_MISSING));
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::GPS_HDOP_TOO_HIGH,
                    (int)deniedReasonFromGnss(GnssQualityFailReason::HDOP_TOO_HIGH));
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::GPS_POSITION_JUMP,
                    (int)deniedReasonFromGnss(GnssQualityFailReason::POSITION_JUMP));
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::GPS_SPEED_INVALID,
                    (int)deniedReasonFromGnss(GnssQualityFailReason::SPEED_INVALID));
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::GPS_ACCEL_INVALID,
                    (int)deniedReasonFromGnss(GnssQualityFailReason::ACCEL_INVALID));
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::GPS_SENTENCES_MISSING,
                    (int)deniedReasonFromGnss(GnssQualityFailReason::SENTENCES_MISSING));
}

void test_reason_strings_are_stable() {
  TEST_ASSERT_EQUAL_STRING("NO_ANCHOR_POINT",
                           anchorDeniedReasonString(AnchorDeniedReason::NO_ANCHOR_POINT));
  TEST_ASSERT_EQUAL_STRING("NO_HEADING",
                           anchorDeniedReasonString(AnchorDeniedReason::NO_HEADING));
  TEST_ASSERT_EQUAL_STRING("GPS_HDOP_TOO_HIGH",
                           anchorDeniedReasonString(AnchorDeniedReason::GPS_HDOP_TOO_HIGH));
  TEST_ASSERT_EQUAL_STRING("GPS_HDOP_MISSING",
                           anchorDeniedReasonString(AnchorDeniedReason::GPS_HDOP_MISSING));
  TEST_ASSERT_EQUAL_STRING("CONTAINMENT_BREACH",
                           failsafeReasonString(FailsafeReason::CONTAINMENT_BREACH));
  TEST_ASSERT_EQUAL_STRING("CONTROL_LOOP_TIMEOUT",
                           failsafeReasonString(FailsafeReason::CONTROL_LOOP_TIMEOUT));
  TEST_ASSERT_EQUAL_STRING("INTERNAL_ERROR_NAN",
                           failsafeReasonString(FailsafeReason::INTERNAL_ERROR_NAN));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_denied_reason_mapping_from_gnss_gate);
  RUN_TEST(test_reason_strings_are_stable);
  return UNITY_END();
}
