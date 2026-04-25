#include "RuntimeBleConnectionLog.h"
#include <string.h>
#include <unity.h>

void test_connect_log_uses_key_value_fields() {
  char line[128];
  const size_t len =
      runtimeBleFormatConnectLog(line, sizeof(line), "aa:bb:cc:dd:ee:ff", 247, 24, 300);

  TEST_ASSERT_EQUAL(strlen(line), len);
  TEST_ASSERT_EQUAL_STRING("[BLE] connect addr=aa:bb:cc:dd:ee:ff mtu=247 int=24 timeout=300\n",
                           line);
}

void test_disconnect_log_keeps_decimal_and_hex_reason() {
  char line[128];
  const size_t len = runtimeBleFormatDisconnectLog(line, sizeof(line), "", 531, 2);

  TEST_ASSERT_EQUAL(strlen(line), len);
  TEST_ASSERT_EQUAL_STRING("[BLE] disconnect addr=unknown reason=531 hex=0x213 clients=2\n",
                           line);
}

void test_connection_log_truncates_by_buffer_contract() {
  char line[24];
  const size_t len =
      runtimeBleFormatDisconnectLog(line, sizeof(line), "aa:bb:cc:dd:ee:ff", 574, 0);

  TEST_ASSERT_EQUAL(sizeof(line) - 1, len);
  TEST_ASSERT_EQUAL('\0', line[sizeof(line) - 1]);
}

void test_connection_log_rejects_missing_buffer() {
  TEST_ASSERT_EQUAL(0U, runtimeBleFormatConnectLog(nullptr, 16, "a", 1, 2, 3));
  char line[1];
  TEST_ASSERT_EQUAL(0U, runtimeBleFormatDisconnectLog(line, 0, "a", 1, 0));
}

void test_connection_log_rejects_unsafe_address_text() {
  TEST_ASSERT_EQUAL_STRING("unknown", runtimeBleLogAddress(nullptr));
  TEST_ASSERT_EQUAL_STRING("unknown", runtimeBleLogAddress(""));
  TEST_ASSERT_EQUAL_STRING("unknown", runtimeBleLogAddress("aa:bb\nfake"));
  TEST_ASSERT_EQUAL_STRING(
      "unknown",
      runtimeBleLogAddress("aa:bb:cc:dd:ee:ff:00:11:22:33:44"));
  TEST_ASSERT_EQUAL_STRING("AA:bb:01", runtimeBleLogAddress("AA:bb:01"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_connect_log_uses_key_value_fields);
  RUN_TEST(test_disconnect_log_keeps_decimal_and_hex_reason);
  RUN_TEST(test_connection_log_truncates_by_buffer_contract);
  RUN_TEST(test_connection_log_rejects_missing_buffer);
  RUN_TEST(test_connection_log_rejects_unsafe_address_text);
  return UNITY_END();
}
