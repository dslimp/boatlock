#include "RuntimeMotion.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <unity.h>

static char gLogBuffer[1024];

static void resetLogBuffer() {
  gLogBuffer[0] = '\0';
}

void logMessage(const char* fmt, ...) {
  if (!fmt) {
    return;
  }
  const size_t used = std::strlen(gLogBuffer);
  if (used >= sizeof(gLogBuffer) - 1) {
    return;
  }
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(gLogBuffer + used, sizeof(gLogBuffer) - used, fmt, args);
  va_end(args);
}

void setUp() { resetLogBuffer(); }
void tearDown() {}

struct RuntimeMotionFixture {
  Settings settings;
  StepperControl stepper;
  MotorControl motor;
  DriftMonitor driftMonitor;
  RuntimeMotion motion;

  RuntimeMotionFixture()
      : settings(),
        stepper(1, 2, 3, 4),
        motor(),
        driftMonitor(),
        motion(settings, stepper, motor, driftMonitor) {
    settings.reset();
    stepper.attachSettings(&settings);
    stepper.loadFromSettings();
    motor.setupPWM(7, 0, 5000, 8);
    motor.setDirPins(5, 10);
  }

  RuntimeControlInput manualInput(unsigned long nowMs, int dir, int speed) const {
    RuntimeControlInput input;
    input.nowMs = nowMs;
    input.mode = CoreMode::MANUAL;
    input.manualDir = dir;
    input.manualSpeed = speed;
    return input;
  }

  RuntimeControlInput holdInput(unsigned long nowMs) const {
    RuntimeControlInput input;
    input.nowMs = nowMs;
    input.mode = CoreMode::SAFE_HOLD;
    return input;
  }

  RuntimeControlInput anchorInput(unsigned long nowMs, float diffDeg, float distanceM) const {
    RuntimeControlInput input;
    input.nowMs = nowMs;
    input.mode = CoreMode::ANCHOR;
    input.controlGpsAvailable = true;
    input.hasHeading = true;
    input.hasBearing = true;
    input.headingDeg = 0.0f;
    input.bearingDeg = diffDeg;
    input.diffDeg = diffDeg;
    input.distanceM = distanceM;
    return input;
  }
};

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

void test_stop_all_motion_quiets_running_manual_actuators() {
  RuntimeMotionFixture f;
  f.settings.set("AnchorEnabled", 1.0f);

  mockSetMillis(1000);
  f.motion.applyControl(f.manualInput(1000, 1, 160));
  TEST_ASSERT_TRUE(f.stepper.manual);
  TEST_ASSERT_TRUE(f.motor.pwmPercent() > 0);

  ManualControl manualControl;
  manualControl.apply(ManualControlSource::BLE_PHONE, 1, 63, 500, 1000);
  f.motion.stopAllMotionNow(manualControl);

  TEST_ASSERT_FALSE(manualControl.active());
  TEST_ASSERT_FALSE(f.stepper.manual);
  TEST_ASSERT_TRUE(f.stepper.idleTimerActive);
  TEST_ASSERT_EQUAL(0, f.stepper.stepper.distanceToGo());
  TEST_ASSERT_EQUAL(0, f.motor.pwmPercent());
  TEST_ASSERT_FALSE(f.motor.autoThrustActive);
  TEST_ASSERT_TRUE(f.motion.safeHoldActive());
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

void test_manual_to_hold_quiets_shared_actuator_path() {
  RuntimeMotionFixture f;

  mockSetMillis(1000);
  f.motion.applyControl(f.manualInput(1000, 0, -180));
  TEST_ASSERT_TRUE(f.stepper.manual);
  TEST_ASSERT_TRUE(f.motor.pwmPercent() > 0);

  mockSetMillis(1100);
  f.motion.applyControl(f.holdInput(1100));

  TEST_ASSERT_FALSE(f.stepper.manual);
  TEST_ASSERT_TRUE(f.stepper.idleTimerActive);
  TEST_ASSERT_EQUAL(0, f.stepper.stepper.distanceToGo());
  TEST_ASSERT_EQUAL(0, f.motor.pwmPercent());
  TEST_ASSERT_FALSE(f.motor.autoThrustActive);
}

void test_hold_cancels_anchor_stepper_tracking() {
  RuntimeMotionFixture f;
  f.settings.set("AnchorEnabled", 1.0f);

  mockSetMillis(1000);
  f.motion.applyControl(f.anchorInput(1000, 45.0f, 20.0f));
  TEST_ASSERT_TRUE(f.stepper.stepper.distanceToGo() != 0);

  mockSetMillis(1100);
  f.motion.applyControl(f.holdInput(1100));

  TEST_ASSERT_FALSE(f.stepper.manual);
  TEST_ASSERT_EQUAL(0, f.stepper.stepper.distanceToGo());
  TEST_ASSERT_TRUE(f.stepper.idleTimerActive);
  TEST_ASSERT_EQUAL(0, f.motor.pwmPercent());
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
  resetLogBuffer();
  motion.applySupervisorDecision(decision, manualControl, 4000);

  TEST_ASSERT_FALSE(manualControl.active());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorEnabled"));
  TEST_ASSERT_EQUAL((int)FailsafeReason::SENSOR_TIMEOUT, (int)motion.lastFailsafeReason());
  TEST_ASSERT_TRUE(motion.safeHoldActive());
  TEST_ASSERT_NOT_NULL(std::strstr(gLogBuffer, "[EVENT] ANCHOR_OFF reason=STOP"));
  TEST_ASSERT_NOT_NULL(std::strstr(gLogBuffer, "[EVENT] FAILSAFE_TRIGGERED reason=SENSOR_TIMEOUT action=STOP"));
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

void test_drift_fail_quiets_anchor_auto_thrust() {
  RuntimeMotionFixture f;
  f.settings.set("AnchorEnabled", 1.0f);
  f.settings.set("DriftAlert", 8.0f);
  f.settings.set("DriftFail", 12.0f);

  mockSetMillis(1000);
  f.motion.applyControl(f.anchorInput(1000, 0.0f, 20.0f));
  TEST_ASSERT_TRUE(f.motor.autoThrustActive);
  TEST_ASSERT_TRUE(f.motor.pwmRaw > 0);

  f.motion.updateDriftState(1200, CoreMode::ANCHOR, true, 20.0f);
  TEST_ASSERT_TRUE(f.motion.driftFailActive());

  mockSetMillis(1200);
  f.motion.applyControl(f.anchorInput(1200, 0.0f, 20.0f));

  TEST_ASSERT_FALSE(f.motor.autoThrustActive);
  TEST_ASSERT_FALSE(f.motor.filteredAnchorDistanceValid);
  TEST_ASSERT_EQUAL(0, f.motor.pwmPercent());
  TEST_ASSERT_EQUAL(0, f.stepper.stepper.distanceToGo());
}

void test_anchor_with_nonfinite_runtime_tuning_quiets_outputs() {
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

  RuntimeControlInput input;
  input.nowMs = 1000;
  input.mode = CoreMode::ANCHOR;
  input.controlGpsAvailable = true;
  input.hasHeading = true;
  input.hasBearing = true;
  input.headingDeg = 0.0f;
  input.bearingDeg = 0.0f;
  input.diffDeg = 0.0f;
  input.distanceM = 20.0f;

  mockSetMillis(1000);
  motion.applyControl(input);
  TEST_ASSERT_TRUE(motor.autoThrustActive);

  settings.entries[settings.idxByKey("AngTol")].value = NAN;
  input.nowMs = 1300;
  mockSetMillis(1300);
  motion.applyControl(input);

  TEST_ASSERT_FALSE(motor.autoThrustActive);
  TEST_ASSERT_FALSE(motor.filteredAnchorDistanceValid);
  TEST_ASSERT_EQUAL(0, motor.pwmPercent());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_stop_all_motion_disarms_and_clears_manual);
  RUN_TEST(test_stop_all_motion_quiets_running_manual_actuators);
  RUN_TEST(test_apply_control_in_manual_mode_runs_manual_outputs);
  RUN_TEST(test_manual_to_hold_quiets_shared_actuator_path);
  RUN_TEST(test_hold_cancels_anchor_stepper_tracking);
  RUN_TEST(test_supervisor_stop_failsafe_latches_safe_hold);
  RUN_TEST(test_supervisor_containment_breach_exits_anchor);
  RUN_TEST(test_clear_safe_hold_returns_to_idle_state);
  RUN_TEST(test_safety_reason_expires);
  RUN_TEST(test_anchor_without_control_gps_clears_auto_thrust_state);
  RUN_TEST(test_drift_fail_quiets_anchor_auto_thrust);
  RUN_TEST(test_anchor_with_nonfinite_runtime_tuning_quiets_outputs);
  return UNITY_END();
}
