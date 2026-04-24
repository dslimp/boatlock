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

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_save_and_load);
  RUN_TEST(test_save_anchor_point_without_enabling_anchor);
  RUN_TEST(test_rejects_invalid_anchor_without_persisting_or_clamping);
  return UNITY_END();
}
