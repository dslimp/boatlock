#include "ManualControl.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

void test_manual_control_accepts_atomic_command() {
  ManualControl manual;
  TEST_ASSERT_TRUE(manual.apply(ManualControlSource::BLE_PHONE, -1, 40, 500, 1000));
  TEST_ASSERT_TRUE(manual.active());
  TEST_ASSERT_EQUAL((int)ManualControlSource::BLE_PHONE, (int)manual.source());
  TEST_ASSERT_EQUAL(-1, manual.steer());
  TEST_ASSERT_EQUAL(40, manual.throttlePct());
  TEST_ASSERT_EQUAL(0, manual.stepperDir());
  TEST_ASSERT_EQUAL(102, manual.motorPwm());
}

void test_manual_control_rejects_out_of_range_values() {
  ManualControl manual;
  TEST_ASSERT_FALSE(manual.apply(ManualControlSource::BLE_PHONE, -2, 0, 500, 1000));
  TEST_ASSERT_FALSE(manual.apply(ManualControlSource::BLE_PHONE, 0, 101, 500, 1000));
  TEST_ASSERT_FALSE(manual.apply(ManualControlSource::BLE_PHONE, 0, 0, 99, 1000));
  TEST_ASSERT_FALSE(manual.apply(ManualControlSource::BLE_PHONE, 0, 0, 1001, 1000));
  TEST_ASSERT_FALSE(manual.active());
}

void test_manual_control_deadman_expires_to_zero() {
  ManualControl manual;
  TEST_ASSERT_TRUE(manual.apply(ManualControlSource::BLE_PHONE, 1, -30, 500, 1000));
  TEST_ASSERT_FALSE(manual.update(1499));
  TEST_ASSERT_TRUE(manual.active());
  TEST_ASSERT_TRUE(manual.update(1500));
  TEST_ASSERT_FALSE(manual.active());
  TEST_ASSERT_EQUAL(-1, manual.stepperDir());
  TEST_ASSERT_EQUAL(0, manual.motorPwm());
}

void test_manual_control_refresh_extends_deadman() {
  ManualControl manual;
  TEST_ASSERT_TRUE(manual.apply(ManualControlSource::BLE_PHONE, 1, 10, 500, 1000));
  TEST_ASSERT_TRUE(manual.apply(ManualControlSource::BLE_REMOTE, 0, 20, 500, 1300));
  TEST_ASSERT_FALSE(manual.update(1799));
  TEST_ASSERT_TRUE(manual.active());
  TEST_ASSERT_EQUAL((int)ManualControlSource::BLE_REMOTE, (int)manual.source());
  TEST_ASSERT_TRUE(manual.update(1800));
  TEST_ASSERT_FALSE(manual.active());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_manual_control_accepts_atomic_command);
  RUN_TEST(test_manual_control_rejects_out_of_range_values);
  RUN_TEST(test_manual_control_deadman_expires_to_zero);
  RUN_TEST(test_manual_control_refresh_extends_deadman);
  return UNITY_END();
}
