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

static int defaultIdxByKey(const char* key) {
  for (int i = 0; i < count; ++i) {
    if (std::strcmp(defaultEntries[i].key, key) == 0) {
      return i;
    }
  }
  return -1;
}

static void fillDefaultValues(float values[count]) {
  for (int i = 0; i < count; ++i) {
    values[i] = defaultEntries[i].defaultValue;
  }
}

static void storeValidValues(const float values[count]) {
  EEPROM.put(Settings::EEPROM_ADDR, Settings::VERSION);
  std::memcpy(EEPROM.data + Settings::VALUES_ADDR, values, Settings::VALUES_BYTES);
  const uint32_t crc = crc32(reinterpret_cast<const uint8_t*>(values), Settings::VALUES_BYTES);
  EEPROM.put(Settings::CRC_ADDR, crc);
}

void test_get_set() {
  Settings s;
  s.reset();
  TEST_ASSERT_FLOAT_WITHIN(0.001, 2.5f, s.get("HoldRadius"));
  TEST_ASSERT_TRUE(s.set("HoldRadius", 4.0f));
  TEST_ASSERT_EQUAL_FLOAT(4.0f, s.get("HoldRadius"));
  s.set("HoldRadius", -1.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.5f, s.get("HoldRadius"));
  s.set("HoldRadius", 1000.0f);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, s.get("HoldRadius"));
}

void test_save_load() {
  Settings s;
  s.reset();
  s.set("HoldRadius", 4.0f);
  s.save();
  s.set("HoldRadius", 10.0f);
  s.load();
  TEST_ASSERT_EQUAL_FLOAT(4.0f, s.get("HoldRadius"));
}

void test_save_skips_clean_state() {
  EEPROM.clear();
  Settings s;
  s.reset();
  const int resetCommits = EEPROM.commitCount;

  s.save();
  TEST_ASSERT_EQUAL_INT(resetCommits, EEPROM.commitCount);

  TEST_ASSERT_TRUE(s.set("HoldRadius", s.get("HoldRadius")));
  s.save();
  TEST_ASSERT_EQUAL_INT(resetCommits, EEPROM.commitCount);

  TEST_ASSERT_TRUE(s.set("HoldRadius", 4.1f));
  s.save();
  TEST_ASSERT_EQUAL_INT(resetCommits + 1, EEPROM.commitCount);
}

void test_save_failure_keeps_dirty_for_retry() {
  EEPROM.clear();
  Settings s;
  s.reset();
  EEPROM.commitCount = 0;
  EEPROM.commitResult = false;

  TEST_ASSERT_TRUE(s.set("HoldRadius", 4.1f));
  TEST_ASSERT_FALSE(s.save());
  TEST_ASSERT_EQUAL_INT(1, EEPROM.commitCount);

  EEPROM.commitResult = true;
  TEST_ASSERT_TRUE(s.save());
  TEST_ASSERT_EQUAL_INT(2, EEPROM.commitCount);
}

void test_load_valid_image_stays_clean() {
  EEPROM.clear();
  float values[count];
  fillDefaultValues(values);
  values[defaultIdxByKey("HoldRadius")] = 3.3f;
  storeValidValues(values);

  Settings s;
  s.load();
  TEST_ASSERT_EQUAL_INT(0, EEPROM.commitCount);
  TEST_ASSERT_EQUAL_FLOAT(3.3f, s.get("HoldRadius"));

  s.save();
  TEST_ASSERT_EQUAL_INT(0, EEPROM.commitCount);
}

void test_set_rejects_nonfinite_without_dirtying() {
  EEPROM.clear();
  Settings s;
  s.reset();
  EEPROM.commitCount = 0;

  TEST_ASSERT_FALSE(s.set("HoldRadius", NAN));
  TEST_ASSERT_EQUAL_FLOAT(2.5f, s.get("HoldRadius"));
  s.save();
  TEST_ASSERT_EQUAL_INT(0, EEPROM.commitCount);
}

void test_set_strict_rejects_out_of_range() {
  Settings s;
  s.reset();
  TEST_ASSERT_FALSE(s.setStrict("HoldRadius", -1.0f));
  TEST_ASSERT_EQUAL_FLOAT(2.5f, s.get("HoldRadius"));
  TEST_ASSERT_TRUE(s.setStrict("HoldRadius", 4.0f));
  TEST_ASSERT_EQUAL_FLOAT(4.0f, s.get("HoldRadius"));
}

void test_crc_mismatch_restores_defaults() {
  Settings s;
  s.reset();
  s.set("HoldRadius", 4.0f);
  s.save();

  uint32_t badCrc = 0xDEADBEEF;
  EEPROM.put(Settings::CRC_ADDR, badCrc);

  s.set("HoldRadius", 10.0f);
  s.load();
  TEST_ASSERT_EQUAL_FLOAT(2.5f, s.get("HoldRadius"));
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
  s.set("HoldRadius", 4.2f);
  s.save();

  uint8_t oldVersion = 0x01;
  EEPROM.put(Settings::EEPROM_ADDR, oldVersion);

  s.set("HoldRadius", 5.0f);
  s.load();
  TEST_ASSERT_EQUAL_FLOAT(2.5f, s.get("HoldRadius"));

  uint8_t loadedVersion = 0;
  EEPROM.get(Settings::EEPROM_ADDR, loadedVersion);
  TEST_ASSERT_EQUAL_UINT8(Settings::VERSION, loadedVersion);
}

void test_load_sanitizes_nan_and_out_of_range_values() {
  Settings s;
  s.reset();

  float values[count];
  fillDefaultValues(values);

  values[s.idxByKey("MaxHdop")] = NAN;
  values[s.idxByKey("AnchorProf")] = 9.0f;
  values[s.idxByKey("MinSats")] = 7.6f;

  EEPROM.put(Settings::EEPROM_ADDR, Settings::VERSION);
  EEPROM.put(Settings::VALUES_ADDR, values);
  const uint32_t crc = crc32(reinterpret_cast<const uint8_t*>(values), Settings::VALUES_BYTES);
  EEPROM.put(Settings::CRC_ADDR, crc);

  EEPROM.commitCount = 0;
  s.load();
  TEST_ASSERT_EQUAL_INT(1, EEPROM.commitCount);
  TEST_ASSERT_EQUAL_FLOAT(1.8f, s.get("MaxHdop"));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, s.get("AnchorProf"));
  TEST_ASSERT_EQUAL_FLOAT(8.0f, s.get("MinSats"));
}

void test_comm_timeout_floor_is_safe() {
  Settings s;
  s.reset();
  TEST_ASSERT_EQUAL_FLOAT(4000.0f, s.get("CommToutMs"));
  TEST_ASSERT_FALSE(s.setStrict("CommToutMs", 1200.0f));
  TEST_ASSERT_EQUAL_FLOAT(4000.0f, s.get("CommToutMs"));

  float values[count];
  fillDefaultValues(values);
  values[s.idxByKey("CommToutMs")] = 1200.0f;

  EEPROM.put(Settings::EEPROM_ADDR, Settings::VERSION);
  EEPROM.put(Settings::VALUES_ADDR, values);
  const uint32_t crc = crc32(reinterpret_cast<const uint8_t*>(values), Settings::VALUES_BYTES);
  EEPROM.put(Settings::CRC_ADDR, crc);

  s.load();
  TEST_ASSERT_TRUE(s.get("CommToutMs") >= 3000.0f);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_get_set);
  RUN_TEST(test_save_load);
  RUN_TEST(test_save_skips_clean_state);
  RUN_TEST(test_save_failure_keeps_dirty_for_retry);
  RUN_TEST(test_load_valid_image_stays_clean);
  RUN_TEST(test_set_rejects_nonfinite_without_dirtying);
  RUN_TEST(test_set_strict_rejects_out_of_range);
  RUN_TEST(test_crc_mismatch_restores_defaults);
  RUN_TEST(test_anchor_profile_setting_bounds);
  RUN_TEST(test_set_strict_rounds_integer_values);
  RUN_TEST(test_load_migrates_when_version_mismatch);
  RUN_TEST(test_load_sanitizes_nan_and_out_of_range_values);
  RUN_TEST(test_comm_timeout_floor_is_safe);
  return UNITY_END();
}
