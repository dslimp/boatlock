#include "MotorControl.h"
#include <unity.h>

void logMessage(const char *, ...) {}

void setUp() {
  mockSetMillis(0);
  g_lastLedcChannel = -1;
  g_lastLedcValue = -1;
  g_lastDigitalPin = -1;
  g_lastDigitalValue = -1;
}

void tearDown() {}

void test_anchor_auto_stays_off_inside_hold_plus_deadband() {
  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  mockSetMillis(1000);
  motor.driveAnchorAuto(3.4f, 2.0f, true); // idle radius = 3.5m

  TEST_ASSERT_FALSE(motor.autoThrustActive);
  TEST_ASSERT_EQUAL(0, motor.pwmRaw);
  TEST_ASSERT_EQUAL(0, motor.pwmPercent());
}

void test_anchor_auto_limits_max_thrust_to_75_percent() {
  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  mockSetMillis(1000);
  for (int i = 0; i < 30; ++i) {
    motor.driveAnchorAuto(40.0f, 2.0f, true);
    mockAdvanceMillis(200);
  }

  const int maxRaw = (75 * 255 + 50) / 100; // 191
  TEST_ASSERT_TRUE(motor.autoThrustActive);
  TEST_ASSERT_LESS_OR_EQUAL(maxRaw, motor.pwmRaw);
  TEST_ASSERT_EQUAL(maxRaw, motor.pwmRaw);
  TEST_ASSERT_EQUAL(75, motor.pwmPercent());
}

void test_anchor_auto_applies_pwm_ramp_limit() {
  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  mockSetMillis(1000);
  motor.driveAnchorAuto(30.0f, 2.0f, true);
  const int first = motor.pwmRaw;

  mockAdvanceMillis(100);
  motor.driveAnchorAuto(30.0f, 2.0f, true);
  const int second = motor.pwmRaw;

  mockAdvanceMillis(100);
  motor.driveAnchorAuto(30.0f, 2.0f, true);
  const int third = motor.pwmRaw;

  TEST_ASSERT_TRUE(second >= first);
  TEST_ASSERT_TRUE(third >= second);
  TEST_ASSERT_TRUE((second - first) <= 9);
  TEST_ASSERT_TRUE((third - second) <= 9);
}

void test_anchor_auto_anti_hunt_min_on_off_windows() {
  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  // Engage anchor thrust.
  mockSetMillis(1000);
  motor.driveAnchorAuto(20.0f, 2.0f, true);
  TEST_ASSERT_TRUE(motor.autoThrustActive);

  // Pull distance down; should not drop out before 1200 ms ON window.
  for (int i = 0; i < 8; ++i) {
    mockAdvanceMillis(100);
    motor.driveAnchorAuto(0.2f, 2.0f, true);
  }
  TEST_ASSERT_TRUE(motor.autoThrustActive);

  // After ON window elapsed, it may deactivate.
  mockSetMillis(2300);
  motor.driveAnchorAuto(0.2f, 2.0f, true);
  TEST_ASSERT_FALSE(motor.autoThrustActive);

  // Try to re-engage too early (< 900 ms OFF window): must stay OFF.
  mockSetMillis(2500);
  motor.driveAnchorAuto(20.0f, 2.0f, true);
  TEST_ASSERT_FALSE(motor.autoThrustActive);

  // Re-engage after OFF window elapsed.
  mockSetMillis(3300);
  motor.driveAnchorAuto(20.0f, 2.0f, true);
  TEST_ASSERT_TRUE(motor.autoThrustActive);
}

void test_anchor_auto_low_passes_distance_rate() {
  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  mockSetMillis(1000);
  motor.driveAnchorAuto(20.0f, 2.0f, true);
  TEST_ASSERT_FALSE(motor.filteredDistanceRateValid);

  mockSetMillis(2000);
  motor.driveAnchorAuto(16.0f, 2.0f, true);
  TEST_ASSERT_TRUE(motor.filteredDistanceRateValid);
  const float rate1 = motor.filteredDistanceRateMps;

  mockSetMillis(3000);
  motor.driveAnchorAuto(12.0f, 2.0f, true);
  const float rate2 = motor.filteredDistanceRateMps;

  TEST_ASSERT_TRUE(rate1 < -0.5f);
  TEST_ASSERT_TRUE(rate2 < rate1);
  TEST_ASSERT_TRUE(rate2 > -2.5f);
}

void test_anchor_auto_distance_rate_uses_zero_timestamp_sample() {
  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  mockSetMillis(0);
  motor.driveAnchorAuto(20.0f, 2.0f, true);
  TEST_ASSERT_FALSE(motor.filteredDistanceRateValid);

  mockSetMillis(1000);
  motor.driveAnchorAuto(16.0f, 2.0f, true);
  TEST_ASSERT_TRUE(motor.filteredDistanceRateValid);
  TEST_ASSERT_TRUE(motor.filteredDistanceRateMps < -0.5f);
}

void test_anchor_auto_distance_rate_survives_millis_rollover() {
  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  const unsigned long nearWrap = ~0UL - 100UL;
  mockSetMillis(nearWrap);
  motor.driveAnchorAuto(20.0f, 2.0f, true);

  mockSetMillis(50);
  motor.driveAnchorAuto(16.0f, 2.0f, true);

  TEST_ASSERT_TRUE(motor.filteredDistanceRateValid);
  TEST_ASSERT_TRUE(motor.filteredDistanceRateMps < -0.5f);
}

void test_anchor_auto_reduces_pwm_on_fast_approach() {
  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  mockSetMillis(1000);
  for (int i = 0; i < 20; ++i) {
    motor.driveAnchorAuto(30.0f, 2.0f, true);
    mockAdvanceMillis(200);
  }
  const int highPwm = motor.pwmRaw;

  for (int i = 0; i < 8; ++i) {
    motor.driveAnchorAuto(4.0f, 2.0f, true);
    mockAdvanceMillis(150);
  }
  const int nearPwm = motor.pwmRaw;

  TEST_ASSERT_TRUE(highPwm > 0);
  TEST_ASSERT_TRUE(nearPwm < highPwm);
}

void test_stop_clears_auto_thrust_state() {
  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  mockSetMillis(1000);
  motor.driveAnchorAuto(20.0f, 2.0f, true);
  TEST_ASSERT_TRUE(motor.autoThrustActive);
  TEST_ASSERT_TRUE(motor.filteredAnchorDistanceValid);

  mockSetMillis(1500);
  motor.stop();

  TEST_ASSERT_FALSE(motor.autoThrustActive);
  TEST_ASSERT_FALSE(motor.filteredAnchorDistanceValid);
  TEST_ASSERT_FALSE(motor.filteredDistanceRateValid);
  TEST_ASSERT_EQUAL(0, motor.pwmRaw);
  TEST_ASSERT_EQUAL(0, motor.autoPwmRaw);
  TEST_ASSERT_EQUAL(1500, motor.autoThrustStateChangedMs);
  TEST_ASSERT_EQUAL(10, g_lastDigitalPin);
  TEST_ASSERT_EQUAL(LOW, g_lastDigitalValue);
}

void test_manual_zero_is_quiet_idle_output() {
  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  mockSetMillis(1000);
  motor.driveManual(0);

  TEST_ASSERT_EQUAL(0, motor.pwmRaw);
  TEST_ASSERT_EQUAL(0, motor.autoPwmRaw);
  TEST_ASSERT_EQUAL(0, motor.autoRampUpdatedMs);
  TEST_ASSERT_EQUAL(10, g_lastDigitalPin);
  TEST_ASSERT_EQUAL(LOW, g_lastDigitalValue);
}

void test_manual_drive_does_not_seed_anchor_auto_ramp() {
  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  mockSetMillis(1000);
  motor.driveManual(255);
  TEST_ASSERT_EQUAL(255, motor.pwmRaw);
  TEST_ASSERT_EQUAL(0, motor.autoPwmRaw);
  TEST_ASSERT_EQUAL(0, motor.autoRampUpdatedMs);

  mockSetMillis(2000);
  motor.driveAnchorAuto(40.0f, 2.0f, true);

  TEST_ASSERT_TRUE(motor.autoThrustActive);
  TEST_ASSERT_LESS_THAN(MotorControl::AUTO_MIN_PWM, motor.pwmRaw);
}

void test_anchor_auto_rejects_nonfinite_tuning_without_output() {
  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  mockSetMillis(1000);
  motor.driveAnchorAuto(40.0f, 2.0f, true);
  TEST_ASSERT_TRUE(motor.autoThrustActive);

  mockSetMillis(1500);
  motor.driveAnchorAuto(40.0f, NAN, true);
  TEST_ASSERT_FALSE(motor.autoThrustActive);
  TEST_ASSERT_EQUAL(0, motor.pwmRaw);
  TEST_ASSERT_EQUAL(LOW, g_lastDigitalValue);

  mockSetMillis(2000);
  motor.driveAnchorAuto(40.0f, 2.0f, true, NAN);
  TEST_ASSERT_FALSE(motor.autoThrustActive);
  TEST_ASSERT_EQUAL(0, motor.pwmRaw);

  mockSetMillis(2500);
  motor.driveAnchorAuto(40.0f, 2.0f, true, MotorControl::AUTO_DEADBAND_M, 75, NAN);
  TEST_ASSERT_FALSE(motor.autoThrustActive);
  TEST_ASSERT_EQUAL(0, motor.pwmRaw);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_anchor_auto_stays_off_inside_hold_plus_deadband);
  RUN_TEST(test_anchor_auto_limits_max_thrust_to_75_percent);
  RUN_TEST(test_anchor_auto_applies_pwm_ramp_limit);
  RUN_TEST(test_anchor_auto_anti_hunt_min_on_off_windows);
  RUN_TEST(test_anchor_auto_low_passes_distance_rate);
  RUN_TEST(test_anchor_auto_distance_rate_uses_zero_timestamp_sample);
  RUN_TEST(test_anchor_auto_distance_rate_survives_millis_rollover);
  RUN_TEST(test_anchor_auto_reduces_pwm_on_fast_approach);
  RUN_TEST(test_stop_clears_auto_thrust_state);
  RUN_TEST(test_manual_zero_is_quiet_idle_output);
  RUN_TEST(test_manual_drive_does_not_seed_anchor_auto_ramp);
  RUN_TEST(test_anchor_auto_rejects_nonfinite_tuning_without_output);
  return UNITY_END();
}
