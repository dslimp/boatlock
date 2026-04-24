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
  TEST_ASSERT_FALSE(pressed.pressedEdge);
  TEST_ASSERT_FALSE(pressed.holdFired);
  TEST_ASSERT_FALSE(pressed.releasedEdge);

  auto debounced = button.update(true, 50, 1500);
  TEST_ASSERT_TRUE(debounced.pressedEdge);
  TEST_ASSERT_FALSE(debounced.holdFired);
  TEST_ASSERT_FALSE(debounced.releasedEdge);

  auto held = button.update(true, 1550, 1500);
  TEST_ASSERT_FALSE(held.pressedEdge);
  TEST_ASSERT_TRUE(held.holdFired);
  TEST_ASSERT_FALSE(held.releasedEdge);

  auto stillHeld = button.update(true, 3000, 1500);
  TEST_ASSERT_FALSE(stillHeld.pressedEdge);
  TEST_ASSERT_FALSE(stillHeld.holdFired);
  TEST_ASSERT_FALSE(stillHeld.releasedEdge);

  auto releaseStart = button.update(false, 3010, 1500);
  TEST_ASSERT_FALSE(releaseStart.releasedEdge);

  auto released = button.update(false, 3050, 1500);
  TEST_ASSERT_FALSE(released.pressedEdge);
  TEST_ASSERT_FALSE(released.holdFired);
  TEST_ASSERT_TRUE(released.releasedEdge);
}

void test_release_before_hold_does_not_fire_long_press() {
  HoldButtonController button;

  button.update(true, 100, 3000);
  button.update(true, 140, 3000);
  button.update(false, 1000, 3000);
  auto released = button.update(false, 1040, 3000);

  TEST_ASSERT_TRUE(released.releasedEdge);
}

void test_zero_timestamp_press_still_triggers_hold() {
  HoldButtonController button;

  auto pressed = button.update(true, 0, 1500);
  auto debounced = button.update(true, 40, 1500);
  auto held = button.update(true, 1540, 1500);

  TEST_ASSERT_FALSE(pressed.pressedEdge);
  TEST_ASSERT_TRUE(debounced.pressedEdge);
  TEST_ASSERT_TRUE(held.holdFired);
}

void test_short_bounce_does_not_fire_edges() {
  HoldButtonController button;

  auto bounceDown = button.update(true, 100, 1500);
  auto bounceUp = button.update(false, 120, 1500);
  auto after = button.update(false, 200, 1500);

  TEST_ASSERT_FALSE(bounceDown.pressedEdge);
  TEST_ASSERT_FALSE(bounceUp.pressedEdge);
  TEST_ASSERT_FALSE(bounceUp.releasedEdge);
  TEST_ASSERT_FALSE(after.pressedEdge);
  TEST_ASSERT_FALSE(after.releasedEdge);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_pressed_hold_and_release_edges_fire_once);
  RUN_TEST(test_release_before_hold_does_not_fire_long_press);
  RUN_TEST(test_zero_timestamp_press_still_triggers_hold);
  RUN_TEST(test_short_bounce_does_not_fire_edges);
  return UNITY_END();
}
