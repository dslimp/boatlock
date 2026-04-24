#include "RuntimeButtons.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_boot_long_press_saves_anchor_once_when_control_gps_available() {
  RuntimeButtons buttons;

  auto pressed = buttons.updateBoot(true, 100, 1500, true, AnchorDeniedReason::GPS_NO_FIX);
  auto held = buttons.updateBoot(true, 1600, 1500, true, AnchorDeniedReason::GPS_NO_FIX);
  auto stillHeld = buttons.updateBoot(true, 3000, 1500, true, AnchorDeniedReason::GPS_NO_FIX);

  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::NONE, (int)pressed.type);
  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::SAVE_ANCHOR_POINT, (int)held.type);
  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::NONE, (int)stillHeld.type);
}

void test_boot_long_press_returns_denied_reason_when_control_gps_unavailable() {
  RuntimeButtons buttons;

  buttons.updateBoot(true, 10, 1500, false, AnchorDeniedReason::GPS_HDOP_TOO_HIGH);
  const auto held =
      buttons.updateBoot(true, 1600, 1500, false, AnchorDeniedReason::GPS_HDOP_TOO_HIGH);

  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::DENY_ANCHOR_POINT, (int)held.type);
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::GPS_HDOP_TOO_HIGH, (int)held.deniedReason);
}

void test_stop_press_and_hold_produce_stop_then_pairing_actions() {
  RuntimeButtons buttons;

  const auto pressed = buttons.updateStop(true, 100, 3000);
  const auto held = buttons.updateStop(true, 3200, 3000);
  const auto stillHeld = buttons.updateStop(true, 6000, 3000);

  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::EMERGENCY_STOP, (int)pressed.type);
  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::OPEN_PAIRING_WINDOW, (int)held.type);
  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::NONE, (int)stillHeld.type);
}

void test_release_resets_cycle_for_next_long_press() {
  RuntimeButtons buttons;

  buttons.updateStop(true, 100, 3000);
  buttons.updateStop(false, 200, 3000);

  const auto pressedAgain = buttons.updateStop(true, 400, 3000);
  const auto heldAgain = buttons.updateStop(true, 3500, 3000);

  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::EMERGENCY_STOP, (int)pressedAgain.type);
  TEST_ASSERT_EQUAL((int)RuntimeButtonActionType::OPEN_PAIRING_WINDOW, (int)heldAgain.type);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_boot_long_press_saves_anchor_once_when_control_gps_available);
  RUN_TEST(test_boot_long_press_returns_denied_reason_when_control_gps_unavailable);
  RUN_TEST(test_stop_press_and_hold_produce_stop_then_pairing_actions);
  RUN_TEST(test_release_resets_cycle_for_next_long_press);
  return UNITY_END();
}
