#include "ManualControl.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_manual_control_accepts_atomic_command() {
  ManualControl manual;
  TEST_ASSERT_TRUE(manual.apply(ManualControlSource::BLE_PHONE, -45.0f, 40, 500, 1000));
  TEST_ASSERT_TRUE(manual.active());
  TEST_ASSERT_EQUAL((int)ManualControlSource::BLE_PHONE, (int)manual.source());
  TEST_ASSERT_EQUAL_FLOAT(-45.0f, manual.angleDeg());
  TEST_ASSERT_EQUAL(40, manual.throttlePct());
  TEST_ASSERT_EQUAL(102, manual.motorPwm());
}

void test_manual_control_rejects_out_of_range_values() {
  ManualControl manual;
  TEST_ASSERT_FALSE(manual.apply(ManualControlSource::NONE, 0, 0, 500, 1000));
  TEST_ASSERT_FALSE(manual.apply(ManualControlSource::BLE_PHONE, -91.0f, 0, 500, 1000));
  TEST_ASSERT_FALSE(manual.apply(ManualControlSource::BLE_PHONE, 91.0f, 0, 500, 1000));
  TEST_ASSERT_FALSE(manual.apply(ManualControlSource::BLE_PHONE, NAN, 0, 500, 1000));
  TEST_ASSERT_FALSE(manual.apply(ManualControlSource::BLE_PHONE, 0, 101, 500, 1000));
  TEST_ASSERT_FALSE(manual.apply(ManualControlSource::BLE_PHONE, 0, 0, 99, 1000));
  TEST_ASSERT_FALSE(manual.apply(ManualControlSource::BLE_PHONE, 0, 0, 1001, 1000));
  TEST_ASSERT_FALSE(manual.active());
}

void test_manual_control_reject_does_not_refresh_existing_command() {
  ManualControl manual;
  TEST_ASSERT_TRUE(manual.apply(ManualControlSource::BLE_PHONE, 30.0f, 20, 500, 1000));
  TEST_ASSERT_FALSE(manual.apply(ManualControlSource::NONE, 0, 0, 500, 1200));

  TEST_ASSERT_TRUE(manual.active());
  TEST_ASSERT_EQUAL_FLOAT(30.0f, manual.angleDeg());
  TEST_ASSERT_EQUAL(20, manual.throttlePct());
  TEST_ASSERT_TRUE(manual.update(1500));
  TEST_ASSERT_FALSE(manual.active());
}

void test_manual_control_deadman_expires_to_zero() {
  ManualControl manual;
  TEST_ASSERT_TRUE(manual.apply(ManualControlSource::BLE_PHONE, 60.0f, -30, 500, 1000));
  TEST_ASSERT_FALSE(manual.update(1499));
  TEST_ASSERT_TRUE(manual.active());
  TEST_ASSERT_TRUE(manual.update(1500));
  TEST_ASSERT_FALSE(manual.active());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, manual.angleDeg());
  TEST_ASSERT_EQUAL(0, manual.motorPwm());
}

void test_manual_control_deadman_accepts_zero_timestamp() {
  ManualControl manual;
  TEST_ASSERT_TRUE(manual.apply(ManualControlSource::BLE_PHONE, 1, 10, 500, 0));
  TEST_ASSERT_FALSE(manual.update(499));
  TEST_ASSERT_TRUE(manual.update(500));
  TEST_ASSERT_FALSE(manual.active());
}

void test_manual_control_deadman_survives_unsigned_millis_rollover() {
  ManualControl manual;
  const unsigned long nearWrap = ~0UL - 200UL;
  TEST_ASSERT_TRUE(manual.apply(ManualControlSource::BLE_PHONE, 1, 10, 500, nearWrap));
  TEST_ASSERT_FALSE(manual.update(100));
  TEST_ASSERT_TRUE(manual.update(400));
  TEST_ASSERT_FALSE(manual.active());
}

void test_manual_control_refresh_extends_deadman() {
  ManualControl manual;
  TEST_ASSERT_TRUE(manual.apply(ManualControlSource::BLE_PHONE, 1, 10, 500, 1000));
  TEST_ASSERT_TRUE(manual.apply(ManualControlSource::BLE_PHONE, 0, 20, 500, 1300));
  TEST_ASSERT_FALSE(manual.update(1799));
  TEST_ASSERT_TRUE(manual.active());
  TEST_ASSERT_EQUAL((int)ManualControlSource::BLE_PHONE, (int)manual.source());
  TEST_ASSERT_TRUE(manual.update(1800));
  TEST_ASSERT_FALSE(manual.active());
}

void test_manual_control_rejects_competing_source_until_deadman_expires() {
  ManualControl manual;
  TEST_ASSERT_TRUE(manual.apply(ManualControlSource::BLE_PHONE, 20.0f, 10, 500, 1000));
  TEST_ASSERT_FALSE(manual.apply(ManualControlSource::BLE_REMOTE, -30.0f, 30, 500, 1300));
  TEST_ASSERT_TRUE(manual.active());
  TEST_ASSERT_EQUAL((int)ManualControlSource::BLE_PHONE, (int)manual.source());
  TEST_ASSERT_EQUAL_FLOAT(20.0f, manual.angleDeg());
  TEST_ASSERT_EQUAL(10, manual.throttlePct());

  TEST_ASSERT_TRUE(manual.apply(ManualControlSource::BLE_REMOTE, -30.0f, 30, 500, 1500));
  TEST_ASSERT_TRUE(manual.active());
  TEST_ASSERT_EQUAL((int)ManualControlSource::BLE_REMOTE, (int)manual.source());
  TEST_ASSERT_EQUAL_FLOAT(-30.0f, manual.angleDeg());
  TEST_ASSERT_EQUAL(30, manual.throttlePct());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_manual_control_accepts_atomic_command);
  RUN_TEST(test_manual_control_rejects_out_of_range_values);
  RUN_TEST(test_manual_control_reject_does_not_refresh_existing_command);
  RUN_TEST(test_manual_control_deadman_expires_to_zero);
  RUN_TEST(test_manual_control_deadman_accepts_zero_timestamp);
  RUN_TEST(test_manual_control_deadman_survives_unsigned_millis_rollover);
  RUN_TEST(test_manual_control_refresh_extends_deadman);
  RUN_TEST(test_manual_control_rejects_competing_source_until_deadman_expires);
  return UNITY_END();
}
