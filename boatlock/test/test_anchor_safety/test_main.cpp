#include "AnchorSafety.h"
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_exits_anchor_when_gps_weak_persists() {
  AnchorSafety s;
  auto a1 = s.update(1000, true, false, true, 7000, 5000);
  auto a2 = s.update(4000, true, false, true, 7000, 5000);
  auto a3 = s.update(6001, true, false, true, 7000, 5000);

  TEST_ASSERT_EQUAL(AnchorSafety::NONE, a1);
  TEST_ASSERT_EQUAL(AnchorSafety::NONE, a2);
  TEST_ASSERT_EQUAL(AnchorSafety::EXIT_GPS_WEAK, a3);
}

void test_failsafe_on_link_lost() {
  AnchorSafety s;
  const auto action = s.update(2000, true, true, false, 7000, 5000);
  TEST_ASSERT_EQUAL(AnchorSafety::FAILSAFE_LINK_LOST, action);
}

void test_failsafe_on_heartbeat_timeout_after_activity() {
  AnchorSafety s;
  s.notifyControlActivity(1000);
  auto a1 = s.update(5000, true, true, true, 7000, 5000);
  auto a2 = s.update(9001, true, true, true, 7000, 5000);

  TEST_ASSERT_EQUAL(AnchorSafety::NONE, a1);
  TEST_ASSERT_EQUAL(AnchorSafety::FAILSAFE_HEARTBEAT_TIMEOUT, a2);
}

void test_no_failsafe_when_anchor_not_active() {
  AnchorSafety s;
  s.notifyControlActivity(1000);
  const auto action = s.update(12000, false, false, false, 7000, 5000);
  TEST_ASSERT_EQUAL(AnchorSafety::NONE, action);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_exits_anchor_when_gps_weak_persists);
  RUN_TEST(test_failsafe_on_link_lost);
  RUN_TEST(test_failsafe_on_heartbeat_timeout_after_activity);
  RUN_TEST(test_no_failsafe_when_anchor_not_active);
  return UNITY_END();
}
