#include "GnssQualityGate.h"
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_passes_with_valid_metrics() {
  GnssQualityConfig cfg;
  GnssQualitySample s;
  s.fix = true;
  s.ageMs = 800;
  s.sats = 9;
  s.hasHdop = true;
  s.hdop = 1.4f;
  s.jumpM = 2.0f;
  s.hasSpeed = true;
  s.speedMps = 1.0f;
  s.hasAccel = true;
  s.accelMps2 = 0.5f;

  TEST_ASSERT_EQUAL(GnssQualityFailReason::NONE, evaluateGnssQuality(cfg, s));
}

void test_rejects_by_age_sats_hdop_and_jump() {
  GnssQualityConfig cfg;
  GnssQualitySample s;
  s.fix = true;
  s.ageMs = 3000;
  s.sats = 9;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::DATA_STALE, evaluateGnssQuality(cfg, s));

  s.ageMs = 100;
  s.sats = 3;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::SATS_TOO_LOW, evaluateGnssQuality(cfg, s));

  s.sats = 8;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::HDOP_MISSING, evaluateGnssQuality(cfg, s));

  s.hasHdop = true;
  s.hdop = 5.2f;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::HDOP_TOO_HIGH, evaluateGnssQuality(cfg, s));

  s.hdop = 1.2f;
  s.jumpM = 50.0f;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::POSITION_JUMP, evaluateGnssQuality(cfg, s));
}

void test_rejects_missing_or_invalid_hdop() {
  GnssQualityConfig cfg;
  GnssQualitySample s;
  s.fix = true;
  s.ageMs = 100;
  s.sats = 8;

  TEST_ASSERT_EQUAL(GnssQualityFailReason::HDOP_MISSING, evaluateGnssQuality(cfg, s));

  s.hasHdop = true;
  s.hdop = NAN;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::HDOP_MISSING, evaluateGnssQuality(cfg, s));

  s.hdop = 0.0f;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::HDOP_MISSING, evaluateGnssQuality(cfg, s));

  s.hdop = 1.0f;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::NONE, evaluateGnssQuality(cfg, s));
}

void test_optional_speed_accel_and_sentences() {
  GnssQualityConfig cfg;
  cfg.enableSpeedAccelSanity = true;
  cfg.maxSpeedMps = 3.0f;
  cfg.maxAccelMps2 = 2.0f;
  cfg.requiredSentences = 3;

  GnssQualitySample s;
  s.fix = true;
  s.ageMs = 100;
  s.sats = 8;
  s.hasHdop = true;
  s.hdop = 1.0f;
  s.hasSpeed = true;
  s.speedMps = 4.0f;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::SPEED_INVALID, evaluateGnssQuality(cfg, s));

  s.speedMps = 2.0f;
  s.hasAccel = true;
  s.accelMps2 = 2.5f;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::ACCEL_INVALID, evaluateGnssQuality(cfg, s));

  s.accelMps2 = 1.0f;
  s.hasSentences = true;
  s.sentences = 1;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::SENTENCES_MISSING, evaluateGnssQuality(cfg, s));

  s.sentences = 4;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::NONE, evaluateGnssQuality(cfg, s));
}

void test_invalid_config_fails_closed() {
  GnssQualitySample s;
  s.fix = true;
  s.ageMs = 0;
  s.sats = 8;
  s.hasHdop = true;
  s.hdop = 1.0f;
  s.jumpM = 0.0f;

  GnssQualityConfig cfg;
  cfg.maxGpsAgeMs = 0;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::DATA_STALE, evaluateGnssQuality(cfg, s));

  cfg = GnssQualityConfig();
  cfg.minSats = 0;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::SATS_TOO_LOW, evaluateGnssQuality(cfg, s));

  cfg = GnssQualityConfig();
  cfg.maxHdop = NAN;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::HDOP_TOO_HIGH, evaluateGnssQuality(cfg, s));

  cfg = GnssQualityConfig();
  cfg.maxPositionJumpM = NAN;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::POSITION_JUMP, evaluateGnssQuality(cfg, s));

  cfg = GnssQualityConfig();
  cfg.requiredSentences = -1;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::SENTENCES_MISSING, evaluateGnssQuality(cfg, s));
}

void test_invalid_optional_motion_samples_fail_when_sanity_is_enabled() {
  GnssQualityConfig cfg;
  cfg.enableSpeedAccelSanity = true;

  GnssQualitySample s;
  s.fix = true;
  s.ageMs = 100;
  s.sats = 8;
  s.hasHdop = true;
  s.hdop = 1.0f;
  s.jumpM = 0.0f;
  s.hasSpeed = true;
  s.speedMps = NAN;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::SPEED_INVALID, evaluateGnssQuality(cfg, s));

  s.speedMps = -0.1f;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::SPEED_INVALID, evaluateGnssQuality(cfg, s));

  s.speedMps = 1.0f;
  s.hasAccel = true;
  s.accelMps2 = NAN;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::ACCEL_INVALID, evaluateGnssQuality(cfg, s));

  s.accelMps2 = 1.0f;
  cfg.maxSpeedMps = NAN;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::SPEED_INVALID, evaluateGnssQuality(cfg, s));

  cfg.maxSpeedMps = 25.0f;
  cfg.maxAccelMps2 = NAN;
  TEST_ASSERT_EQUAL(GnssQualityFailReason::ACCEL_INVALID, evaluateGnssQuality(cfg, s));
}

void test_nonfinite_jump_is_rejected() {
  GnssQualityConfig cfg;
  GnssQualitySample s;
  s.fix = true;
  s.ageMs = 100;
  s.sats = 8;
  s.hasHdop = true;
  s.hdop = 1.0f;
  s.jumpM = NAN;

  TEST_ASSERT_EQUAL(GnssQualityFailReason::POSITION_JUMP, evaluateGnssQuality(cfg, s));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_passes_with_valid_metrics);
  RUN_TEST(test_rejects_by_age_sats_hdop_and_jump);
  RUN_TEST(test_rejects_missing_or_invalid_hdop);
  RUN_TEST(test_optional_speed_accel_and_sentences);
  RUN_TEST(test_invalid_config_fails_closed);
  RUN_TEST(test_invalid_optional_motion_samples_fail_when_sanity_is_enabled);
  RUN_TEST(test_nonfinite_jump_is_rejected);
  return UNITY_END();
}
