#include "HoldButtonController.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_pressed_hold_and_release_edges_fire_once() {
  HoldButtonController button;

  auto idle = button.update(false, 0, 1500);
  TEST_ASSERT_FALSE(idle.pressedEdge);
  TEST_ASSERT_FALSE(idle.holdFired);
  TEST_ASSERT_FALSE(idle.releasedEdge);

  auto pressed = button.update(true, 10, 1500);
  TEST_ASSERT_TRUE(pressed.pressedEdge);
  TEST_ASSERT_FALSE(pressed.holdFired);
  TEST_ASSERT_FALSE(pressed.releasedEdge);

  auto held = button.update(true, 1600, 1500);
  TEST_ASSERT_FALSE(held.pressedEdge);
  TEST_ASSERT_TRUE(held.holdFired);
  TEST_ASSERT_FALSE(held.releasedEdge);

  auto stillHeld = button.update(true, 3000, 1500);
  TEST_ASSERT_FALSE(stillHeld.pressedEdge);
  TEST_ASSERT_FALSE(stillHeld.holdFired);
  TEST_ASSERT_FALSE(stillHeld.releasedEdge);

  auto released = button.update(false, 3010, 1500);
  TEST_ASSERT_FALSE(released.pressedEdge);
  TEST_ASSERT_FALSE(released.holdFired);
  TEST_ASSERT_TRUE(released.releasedEdge);
}

void test_release_before_hold_does_not_fire_long_press() {
  HoldButtonController button;

  button.update(true, 100, 3000);
  auto released = button.update(false, 1000, 3000);

  TEST_ASSERT_TRUE(released.releasedEdge);
}

void test_zero_timestamp_press_still_triggers_hold() {
  HoldButtonController button;

  auto pressed = button.update(true, 0, 1500);
  auto held = button.update(true, 1500, 1500);

  TEST_ASSERT_TRUE(pressed.pressedEdge);
  TEST_ASSERT_TRUE(held.holdFired);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_pressed_hold_and_release_edges_fire_once);
  RUN_TEST(test_release_before_hold_does_not_fire_long_press);
  RUN_TEST(test_zero_timestamp_press_still_triggers_hold);
  return UNITY_END();
}
