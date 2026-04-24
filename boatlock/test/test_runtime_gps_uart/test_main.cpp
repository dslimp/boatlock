#include "RuntimeGpsUart.h"

#include <unity.h>

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

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_first_data_seen_fires_once);
  RUN_TEST(test_no_data_warning_fires_once_after_boot_threshold);
  RUN_TEST(test_stale_stream_warns_and_requests_restart);
  RUN_TEST(test_restart_cooldown_blocks_repeat_restart_until_window_passes);
  return UNITY_END();
}
