#include "AnchorControl.h"
#include <unity.h>
#include <math.h>

void logMessage(const char *, ...) {}

void test_save_and_load() {
  EEPROM.clear();
  Settings s;
  s.reset();
  AnchorControl ac;
  ac.attachSettings(&s);
  TEST_ASSERT_TRUE(ac.saveAnchor(10.0f, 20.0f, 30.0f, true));
  TEST_ASSERT_EQUAL_FLOAT(10.0f, s.get("AnchorLat"));
  TEST_ASSERT_EQUAL_FLOAT(20.0f, s.get("AnchorLon"));
  TEST_ASSERT_EQUAL_FLOAT(30.0f, s.get("AnchorHead"));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, s.get("AnchorEnabled"));
  ac.anchorLat = ac.anchorLon = ac.anchorHeading = 0.0f;
  ac.loadAnchor();
  TEST_ASSERT_EQUAL_FLOAT(10.0f, ac.anchorLat);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, ac.anchorLon);
  TEST_ASSERT_EQUAL_FLOAT(30.0f, ac.anchorHeading);
}

void test_save_anchor_point_without_enabling_anchor() {
  EEPROM.clear();
  Settings s;
  s.reset();
  AnchorControl ac;
  ac.attachSettings(&s);

  TEST_ASSERT_TRUE(ac.saveAnchor(59.9386f, 30.3141f, 370.0f, false));

  TEST_ASSERT_EQUAL_FLOAT(59.9386f, s.get("AnchorLat"));
  TEST_ASSERT_EQUAL_FLOAT(30.3141f, s.get("AnchorLon"));
  TEST_ASSERT_EQUAL_FLOAT(10.0f, s.get("AnchorHead"));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, s.get("AnchorEnabled"));
}

void test_rejects_invalid_anchor_without_persisting_or_clamping() {
  EEPROM.clear();
  Settings s;
  s.reset();
  AnchorControl ac;
  ac.attachSettings(&s);
  TEST_ASSERT_TRUE(ac.saveAnchor(10.0f, 20.0f, 30.0f, false));
  EEPROM.commitCount = 0;

  TEST_ASSERT_FALSE(ac.saveAnchor(91.0f, 20.0f, 40.0f, false));
  TEST_ASSERT_FALSE(ac.saveAnchor(0.0f, 0.0f, 40.0f, false));
  TEST_ASSERT_FALSE(ac.saveAnchor(11.0f, 20.0f, NAN, false));

  TEST_ASSERT_EQUAL_FLOAT(10.0f, ac.anchorLat);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, ac.anchorLon);
  TEST_ASSERT_EQUAL_FLOAT(30.0f, ac.anchorHeading);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, s.get("AnchorLat"));
  TEST_ASSERT_EQUAL_FLOAT(20.0f, s.get("AnchorLon"));
  TEST_ASSERT_EQUAL_FLOAT(30.0f, s.get("AnchorHead"));
  TEST_ASSERT_EQUAL_INT(0, EEPROM.commitCount);
}

void test_save_failure_does_not_replace_live_anchor() {
  EEPROM.clear();
  Settings s;
  s.reset();
  AnchorControl ac;
  ac.attachSettings(&s);
  TEST_ASSERT_TRUE(ac.saveAnchor(10.0f, 20.0f, 30.0f, false));
  EEPROM.commitCount = 0;
  EEPROM.commitResult = false;

  TEST_ASSERT_FALSE(ac.saveAnchor(11.0f, 21.0f, 40.0f, true));

  TEST_ASSERT_EQUAL_FLOAT(10.0f, ac.anchorLat);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, ac.anchorLon);
  TEST_ASSERT_EQUAL_FLOAT(30.0f, ac.anchorHeading);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, s.get("AnchorLat"));
  TEST_ASSERT_EQUAL_FLOAT(20.0f, s.get("AnchorLon"));
  TEST_ASSERT_EQUAL_FLOAT(30.0f, s.get("AnchorHead"));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, s.get("AnchorEnabled"));
  TEST_ASSERT_EQUAL_INT(1, EEPROM.commitCount);

  EEPROM.commitResult = true;
  TEST_ASSERT_TRUE(s.save());
  TEST_ASSERT_EQUAL_INT(2, EEPROM.commitCount);
}

void test_load_anchor_normalizes_or_clears_persisted_values() {
  EEPROM.clear();
  Settings s;
  s.reset();
  AnchorControl ac;
  ac.attachSettings(&s);

  s.setStrict("AnchorLat", 12.0f);
  s.setStrict("AnchorLon", 34.0f);
  s.setStrict("AnchorHead", 360.0f);
  ac.loadAnchor();
  TEST_ASSERT_EQUAL_FLOAT(12.0f, ac.anchorLat);
  TEST_ASSERT_EQUAL_FLOAT(34.0f, ac.anchorLon);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, ac.anchorHeading);

  ac.anchorLat = 12.0f;
  ac.anchorLon = 34.0f;
  ac.anchorHeading = 90.0f;
  s.setStrict("AnchorLat", 0.0f);
  s.setStrict("AnchorLon", 0.0f);
  s.setStrict("AnchorHead", 90.0f);
  ac.loadAnchor();
  TEST_ASSERT_EQUAL_FLOAT(0.0f, ac.anchorLat);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, ac.anchorLon);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, ac.anchorHeading);
}

void test_heading_normalization_is_bounded() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, AnchorControl::normalizeHeading(720.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 355.0f, AnchorControl::normalizeHeading(-725.0f));
  TEST_ASSERT_TRUE(AnchorControl::normalizeHeading(1.0e20f) >= 0.0f);
  TEST_ASSERT_TRUE(AnchorControl::normalizeHeading(1.0e20f) < 360.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, AnchorControl::normalizeHeading(NAN));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_save_and_load);
  RUN_TEST(test_save_anchor_point_without_enabling_anchor);
  RUN_TEST(test_rejects_invalid_anchor_without_persisting_or_clamping);
  RUN_TEST(test_save_failure_does_not_replace_live_anchor);
  RUN_TEST(test_load_anchor_normalizes_or_clears_persisted_values);
  RUN_TEST(test_heading_normalization_is_bounded);
  return UNITY_END();
}
