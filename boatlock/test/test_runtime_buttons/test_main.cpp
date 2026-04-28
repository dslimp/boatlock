#include "RuntimeButtons.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_boot_long_press_saves_anchor_once_when_control_gps_available() {
  RuntimeButtons buttons;

  auto pressed = buttons.updateBoot(true, 100, 1500, true, AnchorDeniedReason::GPS_NO_FIX);
  auto debounced = buttons.updateBoot(true, 140, 1500, true, AnchorDeniedReason::GPS_NO_FIX);
  auto held = buttons.updateBoot(true, 1640, 1500, true, AnchorDeniedReason::GPS_NO_FIX);
  auto stillHeld = buttons.updateBoot(true, 3000, 1500, true, AnchorDeniedReason::GPS_NO_FIX);

  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::NONE, (int)pressed.type);
  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::NONE, (int)debounced.type);
  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::SAVE_ANCHOR_POINT, (int)held.type);
  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::NONE, (int)stillHeld.type);
}

void test_boot_long_press_returns_denied_reason_when_control_gps_unavailable() {
  RuntimeButtons buttons;

  buttons.updateBoot(true, 10, 1500, false, AnchorDeniedReason::GPS_HDOP_TOO_HIGH);
  buttons.updateBoot(true, 50, 1500, false, AnchorDeniedReason::GPS_HDOP_TOO_HIGH);
  const auto held =
      buttons.updateBoot(true, 1550, 1500, false, AnchorDeniedReason::GPS_HDOP_TOO_HIGH);

  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::DENY_ANCHOR_POINT, (int)held.type);
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::GPS_HDOP_TOO_HIGH, (int)held.deniedReason);
}

void test_stop_press_fires_emergency_stop_only() {
  RuntimeButtons buttons;

  const auto pressed = buttons.updateStop(true, 100);
  const auto debounced = buttons.updateStop(true, 140);
  const auto held = buttons.updateStop(true, 3140);
  const auto stillHeld = buttons.updateStop(true, 6000);

  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::NONE, (int)pressed.type);
  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::EMERGENCY_STOP, (int)debounced.type);
  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::NONE, (int)held.type);
  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::NONE, (int)stillHeld.type);
}

void test_release_resets_cycle_for_next_long_press() {
  RuntimeButtons buttons;

  buttons.updateStop(true, 100);
  buttons.updateStop(true, 140);
  buttons.updateStop(false, 200);
  buttons.updateStop(false, 240);

  const auto pressedAgain = buttons.updateStop(true, 400);
  const auto debouncedAgain = buttons.updateStop(true, 440);
  const auto heldAgain = buttons.updateStop(true, 3440);

  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::NONE, (int)pressedAgain.type);
  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::EMERGENCY_STOP, (int)debouncedAgain.type);
  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::NONE, (int)heldAgain.type);
}

void test_stop_bounce_does_not_fire_emergency_stop() {
  RuntimeButtons buttons;

  const auto bounceDown = buttons.updateStop(true, 100);
  const auto bounceUp = buttons.updateStop(false, 120);
  const auto after = buttons.updateStop(false, 200);

  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::NONE, (int)bounceDown.type);
  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::NONE, (int)bounceUp.type);
  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::NONE, (int)after.type);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boot_long_press_saves_anchor_once_when_control_gps_available);
  RUN_TEST(test_boot_long_press_returns_denied_reason_when_control_gps_unavailable);
  RUN_TEST(test_stop_press_fires_emergency_stop_only);
  RUN_TEST(test_release_resets_cycle_for_next_long_press);
  RUN_TEST(test_stop_bounce_does_not_fire_emergency_stop);
  return UNITY_END();
}
