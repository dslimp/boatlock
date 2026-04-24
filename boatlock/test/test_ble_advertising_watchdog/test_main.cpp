#include "BleAdvertisingWatchdog.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_no_action_when_connected_state_matches_clients() {
  TEST_ASSERT_EQUAL((int)BleAdvertisingWatchdogAction::NONE,
                    (int)bleAdvertisingWatchdogAction(true, true, false));
}

void test_marks_connected_when_server_has_clients() {
  TEST_ASSERT_EQUAL((int)BleAdvertisingWatchdogAction::MARK_CONNECTED,
                    (int)bleAdvertisingWatchdogAction(false, true, false));
}

void test_restarts_when_status_is_connected_without_clients() {
  TEST_ASSERT_EQUAL((int)BleAdvertisingWatchdogAction::RESTART_ADVERTISING,
                    (int)bleAdvertisingWatchdogAction(true, false, false));
}

void test_restarts_when_advertising_stopped_without_clients() {
  TEST_ASSERT_EQUAL((int)BleAdvertisingWatchdogAction::RESTART_ADVERTISING,
                    (int)bleAdvertisingWatchdogAction(false, false, false));
}

void test_no_action_when_advertising_without_clients() {
  TEST_ASSERT_EQUAL((int)BleAdvertisingWatchdogAction::NONE,
                    (int)bleAdvertisingWatchdogAction(false, false, true));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_no_action_when_connected_state_matches_clients);
  RUN_TEST(test_marks_connected_when_server_has_clients);
  RUN_TEST(test_restarts_when_status_is_connected_without_clients);
  RUN_TEST(test_restarts_when_advertising_stopped_without_clients);
  RUN_TEST(test_no_action_when_advertising_without_clients);
  return UNITY_END();
}
