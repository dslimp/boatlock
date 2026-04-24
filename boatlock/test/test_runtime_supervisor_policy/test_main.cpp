#include "RuntimeSupervisorPolicy.h"

#include <unity.h>

void logMessage(const char*, ...) {}

void setUp() {}
void tearDown() {}

void test_supervisor_config_clamps_ranges_and_maps_actions() {
  Settings settings;
  settings.reset();
  settings.set("CommToutMs", 1000.0f);
  settings.set("CtrlLoopMs", 50000.0f);
  settings.set("SensorTout", 100.0f);
  settings.set("GpsWeakHys", 120.0f);
  settings.set("FailAct", 1.0f);
  settings.set("NanAct", 0.0f);

  const AnchorSupervisor::Config config = buildRuntimeSupervisorConfig(settings);

  TEST_ASSERT_EQUAL_UINT32(3000UL, config.commTimeoutMs);
  TEST_ASSERT_EQUAL_UINT32(10000UL, config.controlLoopTimeoutMs);
  TEST_ASSERT_EQUAL_UINT32(300UL, config.sensorTimeoutMs);
  TEST_ASSERT_EQUAL_UINT32(60000UL, config.gpsWeakGraceMs);
  TEST_ASSERT_EQUAL(100, config.maxCommandThrustPct);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::MANUAL, (int)config.failsafeAction);
  TEST_ASSERT_EQUAL((int)AnchorSupervisor::SafeAction::STOP, (int)config.nanGuardAction);
}

void test_supervisor_input_builder_keeps_runtime_flags() {
  const AnchorSupervisor::Input input = buildRuntimeSupervisorInput(
      1234, true, false, true, false, true, true, 250, 77);

  TEST_ASSERT_EQUAL_UINT32(1234UL, input.nowMs);
  TEST_ASSERT_TRUE(input.anchorActive);
  TEST_ASSERT_FALSE(input.gpsQualityOk);
  TEST_ASSERT_TRUE(input.linkOk);
  TEST_ASSERT_FALSE(input.sensorsOk);
  TEST_ASSERT_TRUE(input.hasNaN);
  TEST_ASSERT_TRUE(input.containmentBreached);
  TEST_ASSERT_EQUAL_UINT32(250UL, input.controlLoopDtMs);
  TEST_ASSERT_EQUAL(77, input.commandThrustPct);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_supervisor_config_clamps_ranges_and_maps_actions);
  RUN_TEST(test_supervisor_input_builder_keeps_runtime_flags);
  return UNITY_END();
}
