#include "RuntimeGpsUart.h"

#include <unity.h>
#include <climits>

void setUp() {}
void tearDown() {}

void test_first_data_seen_fires_once() {
  RuntimeGpsUart uart;
  uart.reset(0);
  RuntimeGpsUartConfig config;

  const RuntimeGpsUartActions first = uart.update(5, 1000, config);
  const RuntimeGpsUartActions second = uart.update(3, 1100, config);

  TEST_ASSERT_TRUE(first.firstDataSeen);
  TEST_ASSERT_FALSE(second.firstDataSeen);
}

void test_no_data_warning_fires_once_after_boot_threshold() {
  RuntimeGpsUart uart;
  uart.reset(0);
  RuntimeGpsUartConfig config;

  const RuntimeGpsUartActions before = uart.update(0, 6000, config);
  const RuntimeGpsUartActions warned = uart.update(0, 6001, config);
  const RuntimeGpsUartActions after = uart.update(0, 7000, config);

  TEST_ASSERT_FALSE(before.warnNoData);
  TEST_ASSERT_TRUE(warned.warnNoData);
  TEST_ASSERT_FALSE(after.warnNoData);
}

void test_stale_stream_warns_and_requests_restart() {
  RuntimeGpsUart uart;
  uart.reset(0);
  RuntimeGpsUartConfig config;

  uart.update(8, 1000, config);
  const RuntimeGpsUartActions stale = uart.update(0, 7000, config);

  TEST_ASSERT_TRUE(stale.warnStale);
  TEST_ASSERT_TRUE(stale.restartSerial);
  TEST_ASSERT_EQUAL(6000UL, stale.staleAgeMs);
}

void test_first_byte_at_zero_can_still_go_stale() {
  RuntimeGpsUart uart;
  uart.reset(0);
  RuntimeGpsUartConfig config;

  uart.update(8, 0, config);
  const RuntimeGpsUartActions stale = uart.update(0, 6000, config);

  TEST_ASSERT_TRUE(stale.warnStale);
  TEST_ASSERT_TRUE(stale.restartSerial);
  TEST_ASSERT_EQUAL(6000UL, stale.staleAgeMs);
}

void test_restart_cooldown_blocks_repeat_restart_until_window_passes() {
  RuntimeGpsUart uart;
  uart.reset(0);
  RuntimeGpsUartConfig config;
  config.staleRestartMs = 1000;
  config.restartCooldownMs = 4000;

  uart.update(4, 100, config);
  const RuntimeGpsUartActions firstStale = uart.update(0, 1200, config);
  uart.update(2, 1500, config);
  const RuntimeGpsUartActions cooldownBlocked = uart.update(0, 2600, config);
  const RuntimeGpsUartActions cooldownPassed = uart.update(0, 5300, config);

  TEST_ASSERT_TRUE(firstStale.restartSerial);
  TEST_ASSERT_TRUE(cooldownBlocked.warnStale);
  TEST_ASSERT_FALSE(cooldownBlocked.restartSerial);
  TEST_ASSERT_FALSE(cooldownPassed.warnStale);
  TEST_ASSERT_TRUE(cooldownPassed.restartSerial);
}

void test_restart_opens_new_no_data_grace_window() {
  RuntimeGpsUart uart;
  uart.reset(0);
  RuntimeGpsUartConfig config;
  config.noDataWarnMs = 6000;
  config.staleRestartMs = 1000;

  uart.update(4, 100, config);
  const RuntimeGpsUartActions restarted = uart.update(0, 1100, config);
  const RuntimeGpsUartActions immediate = uart.update(0, 1101, config);
  const RuntimeGpsUartActions boundary = uart.update(0, 7100, config);
  const RuntimeGpsUartActions warned = uart.update(0, 7101, config);

  TEST_ASSERT_TRUE(restarted.restartSerial);
  TEST_ASSERT_FALSE(immediate.warnNoData);
  TEST_ASSERT_FALSE(boundary.warnNoData);
  TEST_ASSERT_TRUE(warned.warnNoData);
}

void test_zero_config_values_are_floored_without_losing_zero_timestamp_support() {
  RuntimeGpsUart uart;
  uart.reset(0);
  RuntimeGpsUartConfig config;
  config.noDataWarnMs = 0;
  config.staleRestartMs = 0;
  config.restartCooldownMs = 0;

  const RuntimeGpsUartActions firstByte = uart.update(4, 0, config);
  const RuntimeGpsUartActions beforeFloor = uart.update(0, RuntimeGpsUart::kMinStaleRestartMs - 1, config);
  const RuntimeGpsUartActions staleAtFloor = uart.update(0, RuntimeGpsUart::kMinStaleRestartMs, config);
  uart.update(4, RuntimeGpsUart::kMinStaleRestartMs + 10, config);
  const RuntimeGpsUartActions cooldownBlocked =
      uart.update(0, RuntimeGpsUart::kMinStaleRestartMs * 2 - 1, config);
  const RuntimeGpsUartActions cooldownPassed =
      uart.update(0, RuntimeGpsUart::kMinStaleRestartMs * 2 + 10, config);

  TEST_ASSERT_TRUE(firstByte.firstDataSeen);
  TEST_ASSERT_FALSE(firstByte.restartSerial);
  TEST_ASSERT_FALSE(beforeFloor.warnStale);
  TEST_ASSERT_FALSE(beforeFloor.restartSerial);
  TEST_ASSERT_TRUE(staleAtFloor.warnStale);
  TEST_ASSERT_TRUE(staleAtFloor.restartSerial);
  TEST_ASSERT_FALSE(cooldownBlocked.restartSerial);
  TEST_ASSERT_TRUE(cooldownPassed.restartSerial);
}

void test_zero_no_data_warning_config_is_floored() {
  RuntimeGpsUart uart;
  uart.reset(0);
  RuntimeGpsUartConfig config;
  config.noDataWarnMs = 0;

  TEST_ASSERT_FALSE(uart.update(0, RuntimeGpsUart::kMinNoDataWarnMs, config).warnNoData);
  TEST_ASSERT_TRUE(uart.update(0, RuntimeGpsUart::kMinNoDataWarnMs + 1, config).warnNoData);
}

void test_timers_survive_unsigned_millis_wrap() {
  RuntimeGpsUart uart;
  RuntimeGpsUartConfig config;
  config.noDataWarnMs = RuntimeGpsUart::kMinNoDataWarnMs + 100UL;
  config.staleRestartMs = RuntimeGpsUart::kMinStaleRestartMs + 100UL;

  const unsigned long boot = ULONG_MAX - 50UL;
  uart.reset(boot);
  TEST_ASSERT_TRUE(uart.update(0, boot + config.noDataWarnMs + 1UL, config).warnNoData);

  RuntimeGpsUart staleUart;
  staleUart.reset(boot);
  staleUart.update(1, boot + 10UL, config);
  const RuntimeGpsUartActions stale = staleUart.update(0, boot + 10UL + config.staleRestartMs, config);
  TEST_ASSERT_TRUE(stale.warnStale);
  TEST_ASSERT_TRUE(stale.restartSerial);
  TEST_ASSERT_EQUAL(config.staleRestartMs, stale.staleAgeMs);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_first_data_seen_fires_once);
  RUN_TEST(test_no_data_warning_fires_once_after_boot_threshold);
  RUN_TEST(test_stale_stream_warns_and_requests_restart);
  RUN_TEST(test_first_byte_at_zero_can_still_go_stale);
  RUN_TEST(test_restart_cooldown_blocks_repeat_restart_until_window_passes);
  RUN_TEST(test_restart_opens_new_no_data_grace_window);
  RUN_TEST(test_zero_config_values_are_floored_without_losing_zero_timestamp_support);
  RUN_TEST(test_zero_no_data_warning_config_is_floored);
  RUN_TEST(test_timers_survive_unsigned_millis_wrap);
  return UNITY_END();
}
