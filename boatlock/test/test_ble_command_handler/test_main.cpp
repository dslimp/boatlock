#include "../mocks/BleCommandHandler.h"
#include <unity.h>

AnchorControl anchor;
PathControl pathControl;
StepperControl stepperControl;
MotorControl motor;
Settings settings;
HMC5883Compass compass;
float emuHeading = 0;
bool compassReady = false;
bool manualMode = false;
int manualDir = -1;
int manualSpeed = 0;
bool exportLogCalled = false;
bool clearLogCalled = false;
void startCompassCalibration() {}
void exportRouteLog() { exportLogCalled = true; }
void clearRouteLog() { clearLogCalled = true; }

void setUp() {
  settings.reset();
  anchor.attachSettings(&settings);
  stepperControl.cancelCalled = false;
  stepperControl.stopManualCalled = false;
  stepperControl.loadCalled = false;
  motor.stopCalled = false;
  pathControl.startCalled = false;
  pathControl.stopCalled = false;
  pathControl.numPoints = 0;
  manualMode = false;
  manualDir = -1;
  manualSpeed = 0;
  exportLogCalled = false;
  clearLogCalled = false;
}

void tearDown() {}

void test_set_hold_heading() {
  handleBleCommand("SET_HOLD_HEADING:1");
  TEST_ASSERT_EQUAL_FLOAT(1.0f, settings.get("HoldHeading"));
}

void test_set_step_spr() {
  handleBleCommand("SET_STEP_SPR:400");
  TEST_ASSERT_EQUAL_FLOAT(400.0f, settings.get("StepSpr"));
  TEST_ASSERT_TRUE(stepperControl.loadCalled);
}

void test_set_route_parses_points() {
  handleBleCommand("SET_ROUTE:59.100,30.200;59.300,30.400;59.500,30.600");
  TEST_ASSERT_EQUAL(3, pathControl.numPoints);
}

void test_start_route_disables_anchor() {
  settings.set("AnchorEnabled", 1);
  handleBleCommand("START_ROUTE");
  TEST_ASSERT_TRUE(pathControl.startCalled);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorEnabled"));
}

void test_stop_route_calls_stop() {
  handleBleCommand("STOP_ROUTE");
  TEST_ASSERT_TRUE(pathControl.stopCalled);
}

void test_manual_on_cancels_stepper_move() {
  handleBleCommand("MANUAL:1");
  TEST_ASSERT_TRUE(manualMode);
  TEST_ASSERT_TRUE(stepperControl.cancelCalled);
}

void test_manual_off() {
  manualMode = true;
  manualDir = 2;
  manualSpeed = 100;
  handleBleCommand("MANUAL:0");
  TEST_ASSERT_FALSE(manualMode);
  TEST_ASSERT_TRUE(stepperControl.stopManualCalled);
  TEST_ASSERT_TRUE(motor.stopCalled);
  TEST_ASSERT_EQUAL(0, manualSpeed);
  TEST_ASSERT_EQUAL(-1, manualDir);
}

void test_manual_dir_and_speed() {
  handleBleCommand("MANUAL_DIR:1");
  handleBleCommand("MANUAL_SPEED:-123");
  TEST_ASSERT_EQUAL(1, manualDir);
  TEST_ASSERT_EQUAL(-123, manualSpeed);
}

void test_export_and_clear_log_commands() {
  handleBleCommand("EXPORT_LOG");
  handleBleCommand("CLEAR_LOG");
  TEST_ASSERT_TRUE(exportLogCalled);
  TEST_ASSERT_TRUE(clearLogCalled);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_set_hold_heading);
  RUN_TEST(test_set_step_spr);
  RUN_TEST(test_set_route_parses_points);
  RUN_TEST(test_start_route_disables_anchor);
  RUN_TEST(test_stop_route_calls_stop);
  RUN_TEST(test_manual_on_cancels_stepper_move);
  RUN_TEST(test_manual_off);
  RUN_TEST(test_manual_dir_and_speed);
  RUN_TEST(test_export_and_clear_log_commands);
  return UNITY_END();
}
