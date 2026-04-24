#include "RuntimeSupervisorPolicy.h"

#include <unity.h>

void logMessage(const char*, ...) {}

void setUp() {}
void tearDown() {}

void test_supervisor_config_clamps_ranges() {
  Settings settings;
  settings.reset();
  settings.set("CommToutMs", 1000.0f);
  settings.set("CtrlLoopMs", 50000.0f);
  settings.set("SensorTout", 100.0f);
  settings.set("GpsWeakHys", 120.0f);

  const AnchorSupervisor::Config config = buildRuntimeSupervisorConfig(settings);

  TEST_ASSERT_EQUAL_UINT32(3000UL, config.commTimeoutMs);
  TEST_ASSERT_EQUAL_UINT32(10000UL, config.controlLoopTimeoutMs);
  TEST_ASSERT_EQUAL_UINT32(300UL, config.sensorTimeoutMs);
  TEST_ASSERT_EQUAL_UINT32(60000UL, config.gpsWeakGraceMs);
  TEST_ASSERT_EQUAL(100, config.maxCommandThrustPct);
}

void test_supervisor_config_nonfinite_values_fail_closed() {
  Settings settings;
  settings.reset();
  settings.entries[settings.idxByKey("CommToutMs")].value = NAN;
  settings.entries[settings.idxByKey("CtrlLoopMs")].value = NAN;
  settings.entries[settings.idxByKey("SensorTout")].value = NAN;
  settings.entries[settings.idxByKey("GpsWeakHys")].value = NAN;

  const AnchorSupervisor::Config config = buildRuntimeSupervisorConfig(settings);

  TEST_ASSERT_EQUAL_UINT32(3000UL, config.commTimeoutMs);
  TEST_ASSERT_EQUAL_UINT32(100UL, config.controlLoopTimeoutMs);
  TEST_ASSERT_EQUAL_UINT32(300UL, config.sensorTimeoutMs);
  TEST_ASSERT_EQUAL_UINT32(500UL, config.gpsWeakGraceMs);
  TEST_ASSERT_EQUAL(100, config.maxCommandThrustPct);
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
  RUN_TEST(test_supervisor_config_clamps_ranges);
  RUN_TEST(test_supervisor_config_nonfinite_values_fail_closed);
  RUN_TEST(test_supervisor_input_builder_keeps_runtime_flags);
  return UNITY_END();
}
