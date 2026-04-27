#include "Settings.h"
#include <unity.h>
#include <cstring>
#include <cmath>

void logMessage(const char *, ...) {}

static constexpr uint8_t kSchemaBeforeQuietDefaults = 0x19;
static constexpr float kQuietHoldRadius = 3.0f;
static constexpr float kQuietDeadband = 2.2f;
static constexpr float kQuietMaxThrust = 45.0f;
static constexpr float kQuietRamp = 20.0f;
static constexpr float kOldHoldRadius = 2.5f;
static constexpr float kOldDeadband = 1.5f;
static constexpr float kOldMaxThrust = 60.0f;
static constexpr float kOldRamp = 35.0f;
static constexpr uint8_t kSchemaBeforeStepperDefaults = 0x1B;
static constexpr float kOldStepMaxSpd = 700.0f;
static constexpr float kOldStepAccel = 250.0f;
static constexpr float kCurrentStepMaxSpd = 1200.0f;
static constexpr float kCurrentStepAccel = 800.0f;

void setUp() {
  nvs_mock_clear();
}

void tearDown() {}

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

static void fillOldAnchorDefaults(float values[count]) {
  values[defaultIdxByKey("HoldRadius")] = kOldHoldRadius;
  values[defaultIdxByKey("DeadbandM")] = kOldDeadband;
  values[defaultIdxByKey("MaxThrustA")] = kOldMaxThrust;
  values[defaultIdxByKey("ThrRampA")] = kOldRamp;
}

static void storeNvsValues(const float values[count], uint8_t schema = Settings::VERSION) {
  nvs_mock_clear();
  nvs_mock_put_u8(Settings::NVS_NAMESPACE, Settings::NVS_SCHEMA_KEY, schema);
  for (int i = 0; i < count; ++i) {
    nvs_mock_put_blob(Settings::NVS_NAMESPACE, defaultEntries[i].key, &values[i], sizeof(float));
  }
}

static float persistedFloat(const char* key) {
  float value = NAN;
  TEST_ASSERT_TRUE(nvs_mock_get_blob(Settings::NVS_NAMESPACE, key, &value, sizeof(value)));
  return value;
}

static uint8_t persistedSchema() {
  uint8_t value = 0;
  TEST_ASSERT_TRUE(nvs_mock_get_blob(Settings::NVS_NAMESPACE, Settings::NVS_SCHEMA_KEY, &value, sizeof(value)));
  return value;
}

static void assertQuietAnchorDefaults(Settings& s) {
  TEST_ASSERT_EQUAL_FLOAT(kQuietHoldRadius, s.get("HoldRadius"));
  TEST_ASSERT_EQUAL_FLOAT(kQuietDeadband, s.get("DeadbandM"));
  TEST_ASSERT_EQUAL_FLOAT(kQuietMaxThrust, s.get("MaxThrustA"));
  TEST_ASSERT_EQUAL_FLOAT(kQuietRamp, s.get("ThrRampA"));
}

static void assertQuietAnchorDefaultsPersisted() {
  TEST_ASSERT_EQUAL_FLOAT(kQuietHoldRadius, persistedFloat("HoldRadius"));
  TEST_ASSERT_EQUAL_FLOAT(kQuietDeadband, persistedFloat("DeadbandM"));
  TEST_ASSERT_EQUAL_FLOAT(kQuietMaxThrust, persistedFloat("MaxThrustA"));
  TEST_ASSERT_EQUAL_FLOAT(kQuietRamp, persistedFloat("ThrRampA"));
}

void test_get_set() {
  Settings s;
  s.reset();
  TEST_ASSERT_FLOAT_WITHIN(0.001, kQuietHoldRadius, s.get("HoldRadius"));
  TEST_ASSERT_TRUE(s.set("HoldRadius", 4.0f));
  TEST_ASSERT_EQUAL_FLOAT(4.0f, s.get("HoldRadius"));
  s.set("HoldRadius", -1.0f);
  TEST_ASSERT_EQUAL_FLOAT(0.5f, s.get("HoldRadius"));
  s.set("HoldRadius", 1000.0f);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, s.get("HoldRadius"));
}

void test_constructor_provides_safe_defaults_without_load_or_reset() {
  Settings s;

  assertQuietAnchorDefaults(s);
  TEST_ASSERT_EQUAL_FLOAT(7200.0f, s.get("StepSpr"));
  TEST_ASSERT_EQUAL_FLOAT(kCurrentStepMaxSpd, s.get("StepMaxSpd"));
  TEST_ASSERT_EQUAL_FLOAT(kCurrentStepAccel, s.get("StepAccel"));
  TEST_ASSERT_TRUE(s.set("HoldRadius", 4.0f));
  TEST_ASSERT_EQUAL_FLOAT(4.0f, s.get("HoldRadius"));
}

void test_default_constructor_save_is_noop() {
  Settings s;

  TEST_ASSERT_TRUE(s.save());
  TEST_ASSERT_EQUAL_INT(0, nvs_mock_commit_count());
}

void test_nvs_names_fit_platform_limits() {
  TEST_ASSERT_TRUE(std::strlen(Settings::NVS_NAMESPACE) <= 15);
  TEST_ASSERT_TRUE(std::strlen(Settings::NVS_SCHEMA_KEY) <= 15);
  for (int i = 0; i < count; ++i) {
    TEST_ASSERT_TRUE_MESSAGE(std::strlen(defaultEntries[i].key) <= 15, defaultEntries[i].key);
  }
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
  Settings s;
  s.reset();
  const int resetCommits = nvs_mock_commit_count();

  s.save();
  TEST_ASSERT_EQUAL_INT(resetCommits, nvs_mock_commit_count());

  TEST_ASSERT_TRUE(s.set("HoldRadius", s.get("HoldRadius")));
  s.save();
  TEST_ASSERT_EQUAL_INT(resetCommits, nvs_mock_commit_count());

  TEST_ASSERT_TRUE(s.set("HoldRadius", 4.1f));
  s.save();
  TEST_ASSERT_EQUAL_INT(resetCommits + 1, nvs_mock_commit_count());
}

void test_save_failure_keeps_dirty_for_retry() {
  Settings s;
  s.reset();
  nvs_mock_clear();
  s.reset();
  nvs_mock_set_commit_result(0x1200);

  TEST_ASSERT_TRUE(s.set("HoldRadius", 4.1f));
  TEST_ASSERT_FALSE(s.save());
  TEST_ASSERT_EQUAL_INT(2, nvs_mock_commit_count());

  nvs_mock_set_commit_result(ESP_OK);
  TEST_ASSERT_TRUE(s.save());
  TEST_ASSERT_EQUAL_INT(3, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(4.1f, persistedFloat("HoldRadius"));
}

void test_load_valid_image_stays_clean() {
  float values[count];
  fillDefaultValues(values);
  values[defaultIdxByKey("HoldRadius")] = 3.3f;
  storeNvsValues(values);

  Settings s;
  s.load();
  TEST_ASSERT_EQUAL_INT(0, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(3.3f, s.get("HoldRadius"));

  s.save();
  TEST_ASSERT_EQUAL_INT(0, nvs_mock_commit_count());
}

void test_load_bad_nvs_value_restores_default() {
  float values[count];
  fillDefaultValues(values);
  storeNvsValues(values);

  uint16_t bad = 0xBEEF;
  nvs_mock_put_blob(Settings::NVS_NAMESPACE, "HoldRadius", &bad, sizeof(bad));

  Settings s;
  s.load();
  TEST_ASSERT_EQUAL_INT(1, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(kQuietHoldRadius, s.get("HoldRadius"));
  TEST_ASSERT_EQUAL_FLOAT(kQuietHoldRadius, persistedFloat("HoldRadius"));
}

void test_set_rejects_nonfinite_without_dirtying() {
  Settings s;
  s.reset();
  const int resetCommits = nvs_mock_commit_count();

  TEST_ASSERT_FALSE(s.set("HoldRadius", NAN));
  TEST_ASSERT_EQUAL_FLOAT(kQuietHoldRadius, s.get("HoldRadius"));
  s.save();
  TEST_ASSERT_EQUAL_INT(resetCommits, nvs_mock_commit_count());
}

void test_set_strict_rejects_out_of_range() {
  Settings s;
  s.reset();
  TEST_ASSERT_FALSE(s.setStrict("HoldRadius", -1.0f));
  TEST_ASSERT_EQUAL_FLOAT(kQuietHoldRadius, s.get("HoldRadius"));
  TEST_ASSERT_TRUE(s.setStrict("HoldRadius", 4.0f));
  TEST_ASSERT_EQUAL_FLOAT(4.0f, s.get("HoldRadius"));
}

void test_set_strict_rounds_integer_values() {
  Settings s;
  s.reset();
  TEST_ASSERT_TRUE(s.setStrict("MinSats", 7.6f));
  TEST_ASSERT_EQUAL_FLOAT(8.0f, s.get("MinSats"));
}

void test_set_normalizes_integer_values() {
  Settings s;
  s.reset();

  TEST_ASSERT_TRUE(s.set("MinSats", 7.6f));
  TEST_ASSERT_EQUAL_FLOAT(8.0f, s.get("MinSats"));
  TEST_ASSERT_TRUE(s.set("HoldHeading", 0.6f));
  TEST_ASSERT_EQUAL_FLOAT(1.0f, s.get("HoldHeading"));
}

void test_load_schema_mismatch_preserves_keyed_values() {
  float values[count];
  fillDefaultValues(values);
  values[defaultIdxByKey("HoldRadius")] = 4.2f;
  values[defaultIdxByKey("MaxHdop")] = 2.4f;
  values[defaultIdxByKey("MinSats")] = 7.6f;
  values[defaultIdxByKey("CommToutMs")] = 1200.0f;
  storeNvsValues(values, 0x01);

  Settings s;
  s.load();
  TEST_ASSERT_EQUAL_INT(1, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(4.2f, s.get("HoldRadius"));
  TEST_ASSERT_EQUAL_FLOAT(2.4f, s.get("MaxHdop"));
  TEST_ASSERT_EQUAL_FLOAT(8.0f, s.get("MinSats"));
  TEST_ASSERT_EQUAL_FLOAT(4000.0f, s.get("CommToutMs"));
  TEST_ASSERT_EQUAL_UINT8(Settings::VERSION, persistedSchema());
}

void test_load_old_default_bundle_migrates_to_quiet_defaults() {
  float values[count];
  fillDefaultValues(values);
  fillOldAnchorDefaults(values);
  storeNvsValues(values, kSchemaBeforeQuietDefaults);

  Settings s;
  s.load();

  TEST_ASSERT_EQUAL_INT(1, nvs_mock_commit_count());
  assertQuietAnchorDefaults(s);
  assertQuietAnchorDefaultsPersisted();
  TEST_ASSERT_EQUAL_UINT8(Settings::VERSION, persistedSchema());
}

void test_load_old_default_migration_preserves_custom_anchor_values() {
  float values[count];
  fillDefaultValues(values);
  fillOldAnchorDefaults(values);
  values[defaultIdxByKey("MaxThrustA")] = 55.0f;
  storeNvsValues(values, kSchemaBeforeQuietDefaults);

  Settings s;
  s.load();

  TEST_ASSERT_EQUAL_INT(1, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(kOldHoldRadius, s.get("HoldRadius"));
  TEST_ASSERT_EQUAL_FLOAT(kOldDeadband, s.get("DeadbandM"));
  TEST_ASSERT_EQUAL_FLOAT(55.0f, s.get("MaxThrustA"));
  TEST_ASSERT_EQUAL_FLOAT(kOldRamp, s.get("ThrRampA"));
  TEST_ASSERT_EQUAL_FLOAT(kOldHoldRadius, persistedFloat("HoldRadius"));
  TEST_ASSERT_EQUAL_FLOAT(kOldDeadband, persistedFloat("DeadbandM"));
  TEST_ASSERT_EQUAL_FLOAT(55.0f, persistedFloat("MaxThrustA"));
  TEST_ASSERT_EQUAL_FLOAT(kOldRamp, persistedFloat("ThrRampA"));
  TEST_ASSERT_EQUAL_UINT8(Settings::VERSION, persistedSchema());
}

void test_load_old_stepper_defaults_migrates_for_drv8825_vanchor() {
  float values[count];
  fillDefaultValues(values);
  values[defaultIdxByKey("StepMaxSpd")] = kOldStepMaxSpd;
  values[defaultIdxByKey("StepAccel")] = kOldStepAccel;
  storeNvsValues(values, kSchemaBeforeStepperDefaults);

  Settings s;
  s.load();

  TEST_ASSERT_EQUAL_INT(1, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(kCurrentStepMaxSpd, s.get("StepMaxSpd"));
  TEST_ASSERT_EQUAL_FLOAT(kCurrentStepAccel, s.get("StepAccel"));
  TEST_ASSERT_EQUAL_FLOAT(kCurrentStepMaxSpd, persistedFloat("StepMaxSpd"));
  TEST_ASSERT_EQUAL_FLOAT(kCurrentStepAccel, persistedFloat("StepAccel"));
  TEST_ASSERT_EQUAL_UINT8(Settings::VERSION, persistedSchema());
}

void test_load_old_stepper_defaults_preserves_custom_values() {
  float values[count];
  fillDefaultValues(values);
  values[defaultIdxByKey("StepMaxSpd")] = 900.0f;
  values[defaultIdxByKey("StepAccel")] = kOldStepAccel;
  storeNvsValues(values, kSchemaBeforeStepperDefaults);

  Settings s;
  s.load();

  TEST_ASSERT_EQUAL_INT(1, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(900.0f, s.get("StepMaxSpd"));
  TEST_ASSERT_EQUAL_FLOAT(kOldStepAccel, s.get("StepAccel"));
  TEST_ASSERT_EQUAL_FLOAT(900.0f, persistedFloat("StepMaxSpd"));
  TEST_ASSERT_EQUAL_FLOAT(kOldStepAccel, persistedFloat("StepAccel"));
  TEST_ASSERT_EQUAL_UINT8(Settings::VERSION, persistedSchema());
}

void test_load_old_schema_migrates_legacy_key_when_current_key_missing() {
  float values[count];
  fillDefaultValues(values);
  storeNvsValues(values, 0x17);
  nvs_mock_state().namespaces[Settings::NVS_NAMESPACE].committed.erase("MaxThrustA");
  const float oldMaxThrust = 72.4f;
  nvs_mock_put_blob(Settings::NVS_NAMESPACE, "MaxThrust", &oldMaxThrust, sizeof(oldMaxThrust));

  Settings s;
  s.load();

  TEST_ASSERT_EQUAL_INT(1, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(72.0f, s.get("MaxThrustA"));
  TEST_ASSERT_EQUAL_FLOAT(72.0f, persistedFloat("MaxThrustA"));
  TEST_ASSERT_EQUAL_UINT8(Settings::VERSION, persistedSchema());
}

void test_load_missing_schema_migrates_legacy_key() {
  nvs_mock_clear();
  const float oldMaxThrust = 83.2f;
  nvs_mock_put_blob(Settings::NVS_NAMESPACE, "MaxThrust", &oldMaxThrust, sizeof(oldMaxThrust));

  Settings s;
  s.load();

  TEST_ASSERT_EQUAL_INT(1, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(83.0f, s.get("MaxThrustA"));
  TEST_ASSERT_EQUAL_FLOAT(83.0f, persistedFloat("MaxThrustA"));
  TEST_ASSERT_EQUAL_UINT8(Settings::VERSION, persistedSchema());
}

void test_load_old_schema_keeps_current_key_over_legacy_key() {
  float values[count];
  fillDefaultValues(values);
  values[defaultIdxByKey("MaxThrustA")] = 64.0f;
  storeNvsValues(values, 0x17);
  const float oldMaxThrust = 72.0f;
  nvs_mock_put_blob(Settings::NVS_NAMESPACE, "MaxThrust", &oldMaxThrust, sizeof(oldMaxThrust));

  Settings s;
  s.load();

  TEST_ASSERT_EQUAL_INT(1, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(64.0f, s.get("MaxThrustA"));
  TEST_ASSERT_EQUAL_FLOAT(64.0f, persistedFloat("MaxThrustA"));
  TEST_ASSERT_EQUAL_UINT8(Settings::VERSION, persistedSchema());
}

void test_load_future_schema_does_not_downgrade_or_write_defaults() {
  float values[count];
  fillDefaultValues(values);
  values[defaultIdxByKey("HoldRadius")] = 4.4f;
  storeNvsValues(values, 0x7F);
  nvs_mock_state().namespaces[Settings::NVS_NAMESPACE].committed.erase("DeadbandM");

  Settings s;
  s.load();

  TEST_ASSERT_EQUAL_INT(0, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(4.4f, s.get("HoldRadius"));
  TEST_ASSERT_EQUAL_FLOAT(kQuietDeadband, s.get("DeadbandM"));
  TEST_ASSERT_FALSE(nvs_mock_get_blob(Settings::NVS_NAMESPACE, "DeadbandM", &values[0], sizeof(float)));
  TEST_ASSERT_EQUAL_UINT8(0x7F, persistedSchema());
}

void test_load_missing_nvs_key_uses_default_and_persists_it() {
  float values[count];
  fillDefaultValues(values);
  values[defaultIdxByKey("HoldRadius")] = 3.3f;
  storeNvsValues(values);
  nvs_mock_state().namespaces[Settings::NVS_NAMESPACE].committed.erase("DeadbandM");

  Settings s;
  s.load();
  TEST_ASSERT_EQUAL_INT(1, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(3.3f, s.get("HoldRadius"));
  TEST_ASSERT_EQUAL_FLOAT(kQuietDeadband, s.get("DeadbandM"));
  TEST_ASSERT_EQUAL_FLOAT(kQuietDeadband, persistedFloat("DeadbandM"));
}

void test_load_sanitizes_nan_and_out_of_range_values() {
  float values[count];
  fillDefaultValues(values);

  values[defaultIdxByKey("MaxHdop")] = NAN;
  values[defaultIdxByKey("MinSats")] = 7.6f;
  storeNvsValues(values);

  Settings s;
  s.load();
  TEST_ASSERT_EQUAL_INT(1, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(1.8f, s.get("MaxHdop"));
  TEST_ASSERT_EQUAL_FLOAT(8.0f, s.get("MinSats"));
}

void test_save_uses_one_commit_for_multiple_dirty_keys() {
  Settings s;
  s.reset();
  nvs_mock_state().commitCount = 0;

  TEST_ASSERT_TRUE(s.set("HoldRadius", 4.1f));
  TEST_ASSERT_TRUE(s.set("MaxHdop", 2.2f));
  TEST_ASSERT_TRUE(s.save());

  TEST_ASSERT_EQUAL_INT(1, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(4.1f, persistedFloat("HoldRadius"));
  TEST_ASSERT_EQUAL_FLOAT(2.2f, persistedFloat("MaxHdop"));
}

void test_failed_save_does_not_publish_partial_values() {
  Settings s;
  s.reset();
  nvs_mock_state().commitCount = 0;
  nvs_mock_set_commit_result(0x1200);

  TEST_ASSERT_TRUE(s.set("HoldRadius", 4.1f));
  TEST_ASSERT_TRUE(s.set("MaxHdop", 2.2f));
  TEST_ASSERT_FALSE(s.save());
  TEST_ASSERT_EQUAL_INT(1, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(kQuietHoldRadius, persistedFloat("HoldRadius"));
  TEST_ASSERT_EQUAL_FLOAT(1.8f, persistedFloat("MaxHdop"));

  nvs_mock_set_commit_result(ESP_OK);
  TEST_ASSERT_TRUE(s.save());
  TEST_ASSERT_EQUAL_INT(2, nvs_mock_commit_count());
  TEST_ASSERT_EQUAL_FLOAT(4.1f, persistedFloat("HoldRadius"));
  TEST_ASSERT_EQUAL_FLOAT(2.2f, persistedFloat("MaxHdop"));
}

void test_comm_timeout_floor_is_safe() {
  Settings s;
  s.reset();
  TEST_ASSERT_EQUAL_FLOAT(4000.0f, s.get("CommToutMs"));
  TEST_ASSERT_FALSE(s.setStrict("CommToutMs", 1200.0f));
  TEST_ASSERT_EQUAL_FLOAT(4000.0f, s.get("CommToutMs"));

  float values[count];
  fillDefaultValues(values);
  values[defaultIdxByKey("CommToutMs")] = 1200.0f;
  storeNvsValues(values);

  s.load();
  TEST_ASSERT_TRUE(s.get("CommToutMs") >= 3000.0f);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_get_set);
  RUN_TEST(test_constructor_provides_safe_defaults_without_load_or_reset);
  RUN_TEST(test_default_constructor_save_is_noop);
  RUN_TEST(test_nvs_names_fit_platform_limits);
  RUN_TEST(test_save_load);
  RUN_TEST(test_save_skips_clean_state);
  RUN_TEST(test_save_failure_keeps_dirty_for_retry);
  RUN_TEST(test_load_valid_image_stays_clean);
  RUN_TEST(test_load_bad_nvs_value_restores_default);
  RUN_TEST(test_set_rejects_nonfinite_without_dirtying);
  RUN_TEST(test_set_strict_rejects_out_of_range);
  RUN_TEST(test_set_strict_rounds_integer_values);
  RUN_TEST(test_set_normalizes_integer_values);
  RUN_TEST(test_load_schema_mismatch_preserves_keyed_values);
  RUN_TEST(test_load_old_default_bundle_migrates_to_quiet_defaults);
  RUN_TEST(test_load_old_default_migration_preserves_custom_anchor_values);
  RUN_TEST(test_load_old_stepper_defaults_migrates_for_drv8825_vanchor);
  RUN_TEST(test_load_old_stepper_defaults_preserves_custom_values);
  RUN_TEST(test_load_old_schema_migrates_legacy_key_when_current_key_missing);
  RUN_TEST(test_load_missing_schema_migrates_legacy_key);
  RUN_TEST(test_load_old_schema_keeps_current_key_over_legacy_key);
  RUN_TEST(test_load_future_schema_does_not_downgrade_or_write_defaults);
  RUN_TEST(test_load_missing_nvs_key_uses_default_and_persists_it);
  RUN_TEST(test_load_sanitizes_nan_and_out_of_range_values);
  RUN_TEST(test_save_uses_one_commit_for_multiple_dirty_keys);
  RUN_TEST(test_failed_save_does_not_publish_partial_values);
  RUN_TEST(test_comm_timeout_floor_is_safe);
  return UNITY_END();
}
