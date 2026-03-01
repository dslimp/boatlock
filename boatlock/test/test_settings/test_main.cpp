#include "Settings.h"
#include <unity.h>
#include <cstring>
#include <cmath>

void logMessage(const char *, ...) {}

void setUp() {}
void tearDown() {}

static uint32_t crc32(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      const uint32_t mask = static_cast<uint32_t>(-(static_cast<int>(crc & 1u)));
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}

void test_get_set() {
  Settings s;
  s.reset();
  TEST_ASSERT_FLOAT_WITHIN(0.001, 20.0, s.get("Kp"));
  TEST_ASSERT_TRUE(s.set("Kp", 30.0));
  TEST_ASSERT_EQUAL_FLOAT(30.0, s.get("Kp"));
  s.set("Kp", -1.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.01f, s.get("Kp"));
  s.set("Kp", 1000.0f);
  TEST_ASSERT_EQUAL_FLOAT(200.0f, s.get("Kp"));
}

void test_save_load() {
  Settings s;
  s.reset();
  s.set("Kp", 40.0f);
  s.save();
  s.set("Kp", 10.0f);
  s.load();
  TEST_ASSERT_EQUAL_FLOAT(40.0f, s.get("Kp"));
}

void test_set_strict_rejects_out_of_range() {
  Settings s;
  s.reset();
  TEST_ASSERT_FALSE(s.setStrict("Kp", -1.0f));
  TEST_ASSERT_EQUAL_FLOAT(20.0f, s.get("Kp"));
  TEST_ASSERT_TRUE(s.setStrict("Kp", 30.0f));
  TEST_ASSERT_EQUAL_FLOAT(30.0f, s.get("Kp"));
}

void test_crc_mismatch_restores_defaults() {
  Settings s;
  s.reset();
  s.set("Kp", 40.0f);
  s.save();

  uint32_t badCrc = 0xDEADBEEF;
  EEPROM.put(Settings::CRC_ADDR, badCrc);

  s.set("Kp", 10.0f);
  s.load();
  TEST_ASSERT_EQUAL_FLOAT(20.0f, s.get("Kp"));
}

void test_anchor_profile_setting_bounds() {
  Settings s;
  s.reset();
  TEST_ASSERT_EQUAL_FLOAT(1.0f, s.get("AnchorProf"));
  TEST_ASSERT_TRUE(s.setStrict("AnchorProf", 0.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, s.get("AnchorProf"));
  TEST_ASSERT_FALSE(s.setStrict("AnchorProf", 3.0f));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, s.get("AnchorProf"));
}

void test_set_strict_rounds_integer_values() {
  Settings s;
  s.reset();
  TEST_ASSERT_TRUE(s.setStrict("MinSats", 7.6f));
  TEST_ASSERT_EQUAL_FLOAT(8.0f, s.get("MinSats"));
}

void test_load_migrates_when_version_mismatch() {
  Settings s;
  s.reset();
  s.set("Kp", 42.0f);
  s.save();

  uint8_t oldVersion = 0x01;
  EEPROM.put(Settings::EEPROM_ADDR, oldVersion);

  s.set("Kp", 5.0f);
  s.load();
  TEST_ASSERT_EQUAL_FLOAT(20.0f, s.get("Kp"));

  uint8_t loadedVersion = 0;
  EEPROM.get(Settings::EEPROM_ADDR, loadedVersion);
  TEST_ASSERT_EQUAL_UINT8(Settings::VERSION, loadedVersion);
}

void test_load_sanitizes_nan_and_out_of_range_values() {
  Settings s;
  s.reset();

  float values[count];
  for (int i = 0; i < count; ++i) {
    values[i] = defaultEntries[i].defaultValue;
  }

  values[s.idxByKey("MaxHdop")] = NAN;
  values[s.idxByKey("AnchorProf")] = 9.0f;
  values[s.idxByKey("MinSats")] = 7.6f;

  EEPROM.put(Settings::EEPROM_ADDR, Settings::VERSION);
  EEPROM.put(Settings::VALUES_ADDR, values);
  const uint32_t crc = crc32(reinterpret_cast<const uint8_t*>(values), Settings::VALUES_BYTES);
  EEPROM.put(Settings::CRC_ADDR, crc);

  s.load();
  TEST_ASSERT_EQUAL_FLOAT(3.5f, s.get("MaxHdop"));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, s.get("AnchorProf"));
  TEST_ASSERT_EQUAL_FLOAT(8.0f, s.get("MinSats"));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_get_set);
  RUN_TEST(test_save_load);
  RUN_TEST(test_set_strict_rejects_out_of_range);
  RUN_TEST(test_crc_mismatch_restores_defaults);
  RUN_TEST(test_anchor_profile_setting_bounds);
  RUN_TEST(test_set_strict_rounds_integer_values);
  RUN_TEST(test_load_migrates_when_version_mismatch);
  RUN_TEST(test_load_sanitizes_nan_and_out_of_range_values);
  return UNITY_END();
}
