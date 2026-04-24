#include "RuntimeGnss.h"

#include <unity.h>

void logMessage(const char*, ...) {}

void setUp() {}
void tearDown() {}

void test_phone_fix_is_not_control_fix() {
  Settings settings;
  settings.reset();

  RuntimeGnss gnss;
  gnss.setPhoneFix(59.9386f, 30.3141f, 4.0f, 7, 1000);

  TEST_ASSERT_TRUE(gnss.applyPhoneFallback(false, 0.0f, 2000));
  TEST_ASSERT_TRUE(gnss.fix());
  TEST_ASSERT_TRUE(gnss.gpsSourcePhone());
  TEST_ASSERT_FALSE(gnss.controlGpsAvailable());
  TEST_ASSERT_FALSE(gnss.gpsQualityGoodForAnchorOn(settings));
  TEST_ASSERT_EQUAL((int)GnssQualityFailReason::NO_FIX, (int)gnss.lastGnssFailReason());
}

void test_phone_fix_expires_after_timeout() {
  RuntimeGnss gnss;
  gnss.setPhoneFix(59.9386f, 30.3141f, 2.0f, 5, 1000);

  TEST_ASSERT_TRUE(gnss.applyPhoneFallback(false, 0.0f, 5900));
  TEST_ASSERT_FALSE(gnss.applyPhoneFallback(false, 0.0f, 7001));
}

void test_cached_anchor_bearing_survives_brief_gps_loss() {
  Settings settings;
  settings.reset();

  RuntimeGnss gnss;
  RuntimeGnss::HardwareFixInput fix;
  fix.lat = 59.9386f;
  fix.lon = 30.3141f;
  fix.speedKmh = 3.0f;
  fix.satellites = 8;
  fix.hdop = 1.0f;
  fix.ageMs = 100;
  TEST_ASSERT_EQUAL((int)RuntimeGnss::ApplyResult::APPLIED,
                    (int)gnss.applyHardwareFix(fix, settings, false, 0.0f, 1000));

  gnss.updateDistanceAndBearing(false, true, false, 59.9396f, 30.3141f, 0.0f, 1000);
  const float firstBearing = gnss.bearingDeg();
  TEST_ASSERT_TRUE(isfinite(firstBearing));
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, firstBearing);

  gnss.clearFix();
  gnss.updateDistanceAndBearing(false, true, false, 59.9396f, 30.3141f, 0.0f, 2000);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, firstBearing, gnss.bearingDeg());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, gnss.distanceM());
}

void test_jump_rejection_keeps_previous_fix() {
  Settings settings;
  settings.reset();
  settings.set("MaxPosJumpM", 5.0f);

  RuntimeGnss gnss;
  RuntimeGnss::HardwareFixInput first;
  first.lat = 59.9386f;
  first.lon = 30.3141f;
  first.speedKmh = 2.0f;
  first.satellites = 8;
  first.hdop = 1.0f;
  first.ageMs = 100;
  TEST_ASSERT_EQUAL((int)RuntimeGnss::ApplyResult::APPLIED,
                    (int)gnss.applyHardwareFix(first, settings, false, 0.0f, 1000));

  const float prevLat = gnss.lastLat();
  const float prevLon = gnss.lastLon();

  RuntimeGnss::HardwareFixInput jump = first;
  jump.lat = 59.9486f;
  jump.ageMs = 120;
  TEST_ASSERT_EQUAL((int)RuntimeGnss::ApplyResult::JUMP_REJECTED,
                    (int)gnss.applyHardwareFix(jump, settings, false, 0.0f, 1500));
  TEST_ASSERT_FLOAT_WITHIN(0.000001f, prevLat, gnss.lastLat());
  TEST_ASSERT_FLOAT_WITHIN(0.000001f, prevLon, gnss.lastLon());
  TEST_ASSERT_TRUE(gnss.currentPosJumpRejected());
}

void test_gnss_quality_level_reevaluates_current_sample() {
  Settings settings;
  settings.reset();

  RuntimeGnss gnss;
  RuntimeGnss::HardwareFixInput fix;
  fix.lat = 59.9386f;
  fix.lon = 30.3141f;
  fix.speedKmh = 1.0f;
  fix.satellites = 10;
  fix.hdop = 1.0f;
  fix.ageMs = 100;

  TEST_ASSERT_EQUAL((int)RuntimeGnss::ApplyResult::APPLIED,
                    (int)gnss.applyHardwareFix(fix, settings, false, 0.0f, 1000));
  TEST_ASSERT_TRUE(gnss.gpsQualityGoodForAnchorOn(settings));
  TEST_ASSERT_EQUAL(2, gnss.gnssQualityLevel(settings));

  fix.ageMs = 100;
  fix.hdop = NAN;
  fix.lat = 59.93862f;
  TEST_ASSERT_EQUAL((int)RuntimeGnss::ApplyResult::APPLIED,
                    (int)gnss.applyHardwareFix(fix, settings, false, 0.0f, 2000));
  TEST_ASSERT_EQUAL(1, gnss.gnssQualityLevel(settings));
  TEST_ASSERT_EQUAL((int)GnssQualityFailReason::HDOP_MISSING,
                    (int)gnss.lastGnssFailReason());
}

void test_phone_fallback_quality_sample_does_not_reuse_hardware_metrics() {
  Settings settings;
  settings.reset();

  RuntimeGnss gnss;
  RuntimeGnss::HardwareFixInput fix;
  fix.lat = 59.9386f;
  fix.lon = 30.3141f;
  fix.speedKmh = 4.0f;
  fix.satellites = 10;
  fix.hdop = 1.0f;
  fix.ageMs = 100;
  TEST_ASSERT_EQUAL((int)RuntimeGnss::ApplyResult::APPLIED,
                    (int)gnss.applyHardwareFix(fix, settings, false, 0.0f, 1000));
  TEST_ASSERT_TRUE(gnss.gpsQualityGoodForAnchorOn(settings));

  gnss.setPhoneFix(59.9387f, 30.3142f, 4.0f, 10, 2000);
  TEST_ASSERT_TRUE(gnss.applyPhoneFallback(false, 0.0f, 2500));

  const GnssQualitySample sample = gnss.qualitySample();
  TEST_ASSERT_FALSE(sample.fix);
  TEST_ASSERT_EQUAL(0, sample.sats);
  TEST_ASSERT_FALSE(sample.hasHdop);
  TEST_ASSERT_FALSE(sample.hasSpeed);
  TEST_ASSERT_FALSE(sample.hasAccel);
  TEST_ASSERT_FALSE(sample.hasSentences);
  TEST_ASSERT_EQUAL(0, sample.sentences);
  TEST_ASSERT_FALSE(gnss.gpsQualityGoodForAnchorOn(settings));
  TEST_ASSERT_EQUAL((int)GnssQualityFailReason::NO_FIX,
                    (int)gnss.lastGnssFailReason());
}

void test_hardware_accel_uses_zero_timestamp_sample() {
  Settings settings;
  settings.reset();

  RuntimeGnss gnss;
  RuntimeGnss::HardwareFixInput fix;
  fix.lat = 59.9386f;
  fix.lon = 30.3141f;
  fix.speedKmh = 0.0f;
  fix.satellites = 10;
  fix.hdop = 1.0f;
  fix.ageMs = 100;
  TEST_ASSERT_EQUAL((int)RuntimeGnss::ApplyResult::APPLIED,
                    (int)gnss.applyHardwareFix(fix, settings, false, 0.0f, 0));
  TEST_ASSERT_FALSE(gnss.currentAccelValid());

  fix.lat = 59.93861f;
  fix.speedKmh = 3.6f;
  TEST_ASSERT_EQUAL((int)RuntimeGnss::ApplyResult::APPLIED,
                    (int)gnss.applyHardwareFix(fix, settings, false, 0.0f, 1000));
  TEST_ASSERT_TRUE(gnss.currentAccelValid());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, gnss.currentAccelMps2());
}

void test_phone_fallback_does_not_seed_hardware_accel() {
  Settings settings;
  settings.reset();

  RuntimeGnss gnss;
  RuntimeGnss::HardwareFixInput fix;
  fix.lat = 59.9386f;
  fix.lon = 30.3141f;
  fix.speedKmh = 3.6f;
  fix.satellites = 10;
  fix.hdop = 1.0f;
  fix.ageMs = 100;
  TEST_ASSERT_EQUAL((int)RuntimeGnss::ApplyResult::APPLIED,
                    (int)gnss.applyHardwareFix(fix, settings, false, 0.0f, 1000));

  gnss.setPhoneFix(59.9387f, 30.3142f, 100.0f, 10, 1500);
  TEST_ASSERT_TRUE(gnss.applyPhoneFallback(false, 0.0f, 2000));

  fix.lat = 59.93862f;
  fix.speedKmh = 7.2f;
  TEST_ASSERT_EQUAL((int)RuntimeGnss::ApplyResult::APPLIED,
                    (int)gnss.applyHardwareFix(fix, settings, false, 0.0f, 2500));
  TEST_ASSERT_FALSE(gnss.currentAccelValid());
}

void test_hardware_reacquires_after_clear_fix_without_jump_lockout() {
  Settings settings;
  settings.reset();
  settings.set("MaxPosJumpM", 5.0f);

  RuntimeGnss gnss;
  RuntimeGnss::HardwareFixInput fix;
  fix.lat = 59.9386f;
  fix.lon = 30.3141f;
  fix.speedKmh = 3.6f;
  fix.satellites = 10;
  fix.hdop = 1.0f;
  fix.ageMs = 100;
  TEST_ASSERT_EQUAL((int)RuntimeGnss::ApplyResult::APPLIED,
                    (int)gnss.applyHardwareFix(fix, settings, false, 0.0f, 1000));

  gnss.clearFix();
  fix.lat = 59.9486f;
  TEST_ASSERT_EQUAL((int)RuntimeGnss::ApplyResult::APPLIED,
                    (int)gnss.applyHardwareFix(fix, settings, false, 0.0f, 7000));
}

void test_angle_normalization_uses_bounded_math() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, RuntimeGnss::normalize360Deg(1080.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 270.0f, RuntimeGnss::normalize360Deg(-450.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 180.0f, RuntimeGnss::normalize180Deg(900.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -180.0f, RuntimeGnss::normalize180Deg(-900.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, RuntimeGnss::normalizeDiffDeg(1080.0f, 0.0f));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_phone_fix_is_not_control_fix);
  RUN_TEST(test_phone_fix_expires_after_timeout);
  RUN_TEST(test_cached_anchor_bearing_survives_brief_gps_loss);
  RUN_TEST(test_jump_rejection_keeps_previous_fix);
  RUN_TEST(test_gnss_quality_level_reevaluates_current_sample);
  RUN_TEST(test_phone_fallback_quality_sample_does_not_reuse_hardware_metrics);
  RUN_TEST(test_hardware_accel_uses_zero_timestamp_sample);
  RUN_TEST(test_phone_fallback_does_not_seed_hardware_accel);
  RUN_TEST(test_hardware_reacquires_after_clear_fix_without_jump_lockout);
  RUN_TEST(test_angle_normalization_uses_bounded_math);
  return UNITY_END();
}
