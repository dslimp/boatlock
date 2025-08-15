#include "AnchorControl.h"
#include <unity.h>

void logMessage(const char *, ...) {}

void test_save_and_load() {
  Settings s;
  s.reset();
  AnchorControl ac;
  ac.attachSettings(&s);
  ac.saveAnchor(10.0f, 20.0f, 30.0f);
  TEST_ASSERT_EQUAL_FLOAT(10.0f, s.get("AnchorLat"));
  TEST_ASSERT_EQUAL_FLOAT(20.0f, s.get("AnchorLon"));
  TEST_ASSERT_EQUAL_FLOAT(30.0f, s.get("AnchorHead"));
  ac.anchorLat = ac.anchorLon = ac.anchorHeading = 0.0f;
  ac.loadAnchor();
  TEST_ASSERT_EQUAL_FLOAT(10.0f, ac.anchorLat);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, ac.anchorLon);
  TEST_ASSERT_EQUAL_FLOAT(30.0f, ac.anchorHeading);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_save_and_load);
  return UNITY_END();
}
