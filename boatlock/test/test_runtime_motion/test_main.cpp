#include "RuntimeMotion.h"

#include <unity.h>

void logMessage(const char*, ...) {}

void setUp() {}
void tearDown() {}

void test_stop_all_motion_disarms_and_clears_manual() {
  Settings settings;
  settings.reset();
  settings.set("AnchorEnabled", 1.0f);

  StepperControl stepper(1, 2, 3, 4);
  stepper.attachSettings(&settings);
  stepper.loadFromSettings();

  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  DriftMonitor driftMonitor;
  RuntimeMotion motion(settings, stepper, motor, driftMonitor);

  ManualControl manualControl;
  manualControl.apply(ManualControlSource::BLE_PHONE, 1, 50, 500, 1000);
  motion.stopAllMotionNow(manualControl);

  TEST_ASSERT_FALSE(manualControl.active());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorEnabled"));
  TEST_ASSERT_EQUAL(0, motor.pwmPercent());
  TEST_ASSERT_TRUE(motion.safeHoldActive());
}

void test_apply_control_in_manual_mode_runs_manual_outputs() {
  Settings settings;
  settings.reset();
  settings.set("StepMaxSpd", 800.0f);

  StepperControl stepper(1, 2, 3, 4);
  stepper.attachSettings(&settings);
  stepper.loadFromSettings();

  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  DriftMonitor driftMonitor;
  RuntimeMotion motion(settings, stepper, motor, driftMonitor);

  RuntimeControlInput input;
  input.nowMs = 1000;
  input.mode = CoreMode::MANUAL;
  input.manualDir = 1;
  input.manualSpeed = 120;

  motion.applyControl(input);

  TEST_ASSERT_TRUE(stepper.manual);
  TEST_ASSERT_EQUAL(47, motor.pwmPercent());
}

void test_supervisor_stop_failsafe_latches_safe_hold() {
  Settings settings;
  settings.reset();
  settings.set("AnchorEnabled", 1.0f);

  StepperControl stepper(1, 2, 3, 4);
  stepper.attachSettings(&settings);
  stepper.loadFromSettings();

  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  DriftMonitor driftMonitor;
  RuntimeMotion motion(settings, stepper, motor, driftMonitor);

  ManualControl manualControl;
  manualControl.apply(ManualControlSource::BLE_PHONE, 1, 50, 500, 1000);

  AnchorSupervisor::Decision decision;
  decision.action = AnchorSupervisor::SafeAction::STOP;
  decision.reason = AnchorSupervisor::Reason::SENSOR_TIMEOUT;
  motion.applySupervisorDecision(decision, manualControl, 4000);

  TEST_ASSERT_FALSE(manualControl.active());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorEnabled"));
  TEST_ASSERT_EQUAL((int)FailsafeReason::SENSOR_TIMEOUT, (int)motion.lastFailsafeReason());
  TEST_ASSERT_TRUE(motion.safeHoldActive());
}

void test_supervisor_containment_breach_exits_anchor() {
  Settings settings;
  settings.reset();
  settings.set("AnchorEnabled", 1.0f);

  StepperControl stepper(1, 2, 3, 4);
  stepper.attachSettings(&settings);
  stepper.loadFromSettings();

  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  DriftMonitor driftMonitor;
  RuntimeMotion motion(settings, stepper, motor, driftMonitor);

  ManualControl manualControl;
  manualControl.apply(ManualControlSource::BLE_PHONE, 1, 50, 500, 1000);

  AnchorSupervisor::Decision decision;
  decision.action = AnchorSupervisor::SafeAction::EXIT_ANCHOR;
  decision.reason = AnchorSupervisor::Reason::CONTAINMENT_BREACH;
  motion.applySupervisorDecision(decision, manualControl, 4000);

  TEST_ASSERT_FALSE(manualControl.active());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorEnabled"));
  TEST_ASSERT_EQUAL((int)FailsafeReason::CONTAINMENT_BREACH, (int)motion.lastFailsafeReason());
  TEST_ASSERT_EQUAL_STRING("CONTAINMENT_BREACH", motion.activeSafetyReason(4000).c_str());
  TEST_ASSERT_TRUE(motion.safeHoldActive());
}

void test_clear_safe_hold_returns_to_idle_state() {
  Settings settings;
  settings.reset();
  StepperControl stepper(1, 2, 3, 4);
  MotorControl motor;
  DriftMonitor driftMonitor;
  RuntimeMotion motion(settings, stepper, motor, driftMonitor);

  motion.enterSafeHold();
  TEST_ASSERT_TRUE(motion.safeHoldActive());

  motion.clearSafeHold();
  TEST_ASSERT_FALSE(motion.safeHoldActive());
}

void test_safety_reason_expires() {
  Settings settings;
  settings.reset();
  StepperControl stepper(1, 2, 3, 4);
  MotorControl motor;
  DriftMonitor driftMonitor;
  RuntimeMotion motion(settings, stepper, motor, driftMonitor);

  motion.setSafetyReason("NUDGE_OK", 1000);
  TEST_ASSERT_EQUAL_STRING("NUDGE_OK", motion.activeSafetyReason(2000).c_str());
  TEST_ASSERT_EQUAL_STRING("", motion.activeSafetyReason(20000).c_str());
}

void test_anchor_without_control_gps_clears_auto_thrust_state() {
  Settings settings;
  settings.reset();
  settings.set("AnchorEnabled", 1.0f);

  StepperControl stepper(1, 2, 3, 4);
  stepper.attachSettings(&settings);
  stepper.loadFromSettings();

  MotorControl motor;
  motor.setupPWM(7, 0, 5000, 8);
  motor.setDirPins(5, 10);

  DriftMonitor driftMonitor;
  RuntimeMotion motion(settings, stepper, motor, driftMonitor);

  mockSetMillis(1000);
  motor.driveAnchorAuto(20.0f, 2.0f, true);
  TEST_ASSERT_TRUE(motor.autoThrustActive);

  RuntimeControlInput input;
  input.nowMs = 1500;
  input.mode = CoreMode::ANCHOR;
  input.controlGpsAvailable = false;
  input.hasHeading = true;
  input.hasBearing = true;
  input.headingDeg = 0.0f;
  input.bearingDeg = 0.0f;
  input.diffDeg = 0.0f;
  input.distanceM = 20.0f;
  motion.applyControl(input);

  TEST_ASSERT_FALSE(motor.autoThrustActive);
  TEST_ASSERT_FALSE(motor.filteredAnchorDistanceValid);
  TEST_ASSERT_EQUAL(0, motor.pwmPercent());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_stop_all_motion_disarms_and_clears_manual);
  RUN_TEST(test_apply_control_in_manual_mode_runs_manual_outputs);
  RUN_TEST(test_supervisor_stop_failsafe_latches_safe_hold);
  RUN_TEST(test_supervisor_containment_breach_exits_anchor);
  RUN_TEST(test_clear_safe_hold_returns_to_idle_state);
  RUN_TEST(test_safety_reason_expires);
  RUN_TEST(test_anchor_without_control_gps_clears_auto_thrust_state);
  return UNITY_END();
}
