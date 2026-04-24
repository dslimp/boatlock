#include "AnchorSupervisor.h"
#include <unity.h>

void setUp() {}
void tearDown() {}

static AnchorSupervisor::Config baseCfg() {
  AnchorSupervisor::Config cfg;
  cfg.commTimeoutMs = 7000;
  cfg.controlLoopTimeoutMs = 600;
  cfg.sensorTimeoutMs = 3000;
  cfg.gpsWeakGraceMs = 5000;
  cfg.maxCommandThrustPct = 100;
  return cfg;
}

static AnchorSupervisor::Input baseIn() {
  AnchorSupervisor::Input in;
  in.nowMs = 1000;
  in.anchorActive = true;
  in.gpsQualityOk = true;
  in.linkOk = true;
  in.sensorsOk = true;
  in.hasNaN = false;
  in.controlLoopDtMs = 40;
  in.commandThrustPct = 0;
  return in;
}

void test_gps_weak_exits_anchor_after_hysteresis() {
  AnchorSupervisor s;
  auto cfg = baseCfg();
  auto in = baseIn();

  in.gpsQualityOk = false;
  auto d1 = s.update(cfg, in);
  in.nowMs = 4500;
  auto d2 = s.update(cfg, in);
  in.nowMs = 6100;
  auto d3 = s.update(cfg, in);

  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::NONE, (int)d1.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::NONE, (int)d2.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::EXIT_ANCHOR, (int)d3.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::Reason::GPS_WEAK, (int)d3.reason);
}

void test_comm_timeout_when_link_lost() {
  AnchorSupervisor s;
  auto cfg = baseCfg();
  auto in = baseIn();
  in.linkOk = false;
  auto d = s.update(cfg, in);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::STOP, (int)d.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::Reason::COMM_TIMEOUT, (int)d.reason);
}

void test_containment_breach_exits_anchor_immediately() {
  AnchorSupervisor s;
  auto cfg = baseCfg();
  auto in = baseIn();
  in.containmentBreached = true;
  auto d = s.update(cfg, in);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::EXIT_ANCHOR, (int)d.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::Reason::CONTAINMENT_BREACH, (int)d.reason);
}

void test_containment_breach_has_priority_over_gps_grace() {
  AnchorSupervisor s;
  auto cfg = baseCfg();
  auto in = baseIn();
  in.nowMs = 0;
  in.gpsQualityOk = false;
  s.update(cfg, in);

  in.nowMs = 6000;
  in.containmentBreached = true;
  auto d = s.update(cfg, in);

  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::EXIT_ANCHOR, (int)d.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::Reason::CONTAINMENT_BREACH, (int)d.reason);
}

void test_comm_timeout_after_control_activity_deadline() {
  AnchorSupervisor s;
  auto cfg = baseCfg();
  auto in = baseIn();
  s.noteControlActivity(1000);

  in.nowMs = 6000;
  auto d1 = s.update(cfg, in);
  in.nowMs = 9001;
  auto d2 = s.update(cfg, in);

  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::NONE, (int)d1.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::STOP, (int)d2.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::Reason::COMM_TIMEOUT, (int)d2.reason);
}

void test_control_loop_timeout_triggers_failsafe() {
  AnchorSupervisor s;
  auto cfg = baseCfg();
  auto in = baseIn();
  in.controlLoopDtMs = 1000;
  auto d = s.update(cfg, in);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::STOP, (int)d.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::Reason::CONTROL_LOOP_TIMEOUT, (int)d.reason);
}

void test_sensor_timeout_triggers_failsafe() {
  AnchorSupervisor s;
  auto cfg = baseCfg();
  auto in = baseIn();
  in.sensorsOk = false;
  auto d1 = s.update(cfg, in);
  in.nowMs = 4501;
  auto d2 = s.update(cfg, in);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::NONE, (int)d1.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::STOP, (int)d2.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::Reason::SENSOR_TIMEOUT, (int)d2.reason);
}

void test_zero_timestamp_sensor_timeout_still_triggers() {
  AnchorSupervisor s;
  auto cfg = baseCfg();
  auto in = baseIn();
  in.nowMs = 0;
  in.sensorsOk = false;
  auto d1 = s.update(cfg, in);
  in.nowMs = 3000;
  auto d2 = s.update(cfg, in);

  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::NONE, (int)d1.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::STOP, (int)d2.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::Reason::SENSOR_TIMEOUT, (int)d2.reason);
}

void test_zero_timestamp_gps_weak_still_triggers() {
  AnchorSupervisor s;
  auto cfg = baseCfg();
  auto in = baseIn();
  in.nowMs = 0;
  in.gpsQualityOk = false;
  auto d1 = s.update(cfg, in);
  in.nowMs = 5000;
  auto d2 = s.update(cfg, in);

  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::NONE, (int)d1.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::EXIT_ANCHOR, (int)d2.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::Reason::GPS_WEAK, (int)d2.reason);
}

void test_nan_guard_always_stops() {
  AnchorSupervisor s;
  auto cfg = baseCfg();
  auto in = baseIn();
  in.hasNaN = true;
  auto d = s.update(cfg, in);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::STOP, (int)d.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::Reason::INTERNAL_ERROR_NAN, (int)d.reason);
}

void test_command_out_of_range_triggers_failsafe() {
  AnchorSupervisor s;
  auto cfg = baseCfg();
  auto in = baseIn();
  in.commandThrustPct = 130;
  auto d = s.update(cfg, in);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::STOP, (int)d.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::Reason::COMMAND_OUT_OF_RANGE, (int)d.reason);
}

void test_inactive_anchor_resets_and_returns_none() {
  AnchorSupervisor s;
  auto cfg = baseCfg();
  auto in = baseIn();
  in.anchorActive = false;
  in.linkOk = false;
  auto d = s.update(cfg, in);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::NONE, (int)d.action);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::Reason::NONE, (int)d.reason);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_gps_weak_exits_anchor_after_hysteresis);
  RUN_TEST(test_comm_timeout_when_link_lost);
  RUN_TEST(test_containment_breach_exits_anchor_immediately);
  RUN_TEST(test_containment_breach_has_priority_over_gps_grace);
  RUN_TEST(test_comm_timeout_after_control_activity_deadline);
  RUN_TEST(test_control_loop_timeout_triggers_failsafe);
  RUN_TEST(test_sensor_timeout_triggers_failsafe);
  RUN_TEST(test_zero_timestamp_sensor_timeout_still_triggers);
  RUN_TEST(test_zero_timestamp_gps_weak_still_triggers);
  RUN_TEST(test_nan_guard_always_stops);
  RUN_TEST(test_command_out_of_range_triggers_failsafe);
  RUN_TEST(test_inactive_anchor_resets_and_returns_none);
  return UNITY_END();
}
