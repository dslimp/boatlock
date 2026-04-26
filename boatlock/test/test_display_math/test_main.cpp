#include "DisplayMath.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_anchor_arrow_uses_world_bearing_minus_heading() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, displayAnchorArrowScreenDeg(90.0f, 90.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 270.0f, displayAnchorArrowScreenDeg(0.0f, 90.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 90.0f, displayAnchorArrowScreenDeg(0.0f, 270.0f));
}

void test_compass_card_keeps_existing_bno_screen_sign() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 90.0f, displayCompassCardScreenDeg(0.0f, 90.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, displayCompassCardScreenDeg(270.0f, 90.0f));
}

void test_display_normalization_is_bounded_and_safe() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, displayNormalize360Deg(NAN));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 270.0f, displayNormalize360Deg(-450.0f));
  TEST_ASSERT_EQUAL(0, displayNormalize360Int(360.0f));
  TEST_ASSERT_EQUAL(0, displayNormalize360Int(NAN));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_anchor_arrow_uses_world_bearing_minus_heading);
  RUN_TEST(test_compass_card_keeps_existing_bno_screen_sign);
  RUN_TEST(test_display_normalization_is_bounded_and_safe);
  return UNITY_END();
}
