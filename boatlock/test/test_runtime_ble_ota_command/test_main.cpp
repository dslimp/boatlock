#include "RuntimeBleOtaCommand.h"
#include <unity.h>

void test_parse_ota_begin_accepts_size_and_sha() {
  RuntimeBleOtaBeginCommand out;
  TEST_ASSERT_TRUE(runtimeBleParseOtaBeginCommand(
      "OTA_BEGIN:1234,00112233445566778899aabbccddeeff00112233445566778899AABBCCDDEEFF",
      &out));
  TEST_ASSERT_EQUAL_UINT32(1234, out.imageSize);
  TEST_ASSERT_EQUAL_STRING("00112233445566778899aabbccddeeff00112233445566778899AABBCCDDEEFF",
                           out.sha256Hex);
}

void test_parse_ota_begin_rejects_bad_payloads() {
  RuntimeBleOtaBeginCommand out;
  TEST_ASSERT_FALSE(runtimeBleParseOtaBeginCommand("OTA_BEGIN:0,00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", &out));
  TEST_ASSERT_FALSE(runtimeBleParseOtaBeginCommand("OTA_BEGIN:12x,00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", &out));
  TEST_ASSERT_FALSE(runtimeBleParseOtaBeginCommand("OTA_BEGIN:12,001122", &out));
  TEST_ASSERT_FALSE(runtimeBleParseOtaBeginCommand("OTA_BEGIN:12,00112233445566778899aabbccddeeff00112233445566778899aabbccddeefg", &out));
  TEST_ASSERT_FALSE(runtimeBleParseOtaBeginCommand("OTA_FINISH", &out));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parse_ota_begin_accepts_size_and_sha);
  RUN_TEST(test_parse_ota_begin_rejects_bad_payloads);
  return UNITY_END();
}
