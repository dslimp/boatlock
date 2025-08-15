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
void startCompassCalibration() {}

void setUp() {
  settings.reset();
  anchor.attachSettings(&settings);
  stepperControl.cancelCalled = false;
  stepperControl.stopManualCalled = false;
  stepperControl.loadCalled = false;
  motor.stopCalled = false;
  manualMode = false;
  manualDir = -1;
  manualSpeed = 0;
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

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_set_hold_heading);
  RUN_TEST(test_set_step_spr);
  RUN_TEST(test_manual_off);
  return UNITY_END();
}
