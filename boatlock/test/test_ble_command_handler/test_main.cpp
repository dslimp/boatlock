#include "../mocks/BleCommandHandler.h"
#include <unity.h>

AnchorControl anchor;
StepperControl stepperControl;
MotorControl motor;
Settings settings;
BNO08xCompass compass;
bool compassReady = false;
bool manualMode = false;
int manualDir = -1;
int manualSpeed = 0;
bool phoneGpsFixSet = false;
float phoneGpsLat = 0.0f;
float phoneGpsLon = 0.0f;
float phoneGpsSpeed = 0.0f;
int phoneGpsSatellites = 0;
bool stepperBowCaptured = false;
void setPhoneGpsFix(float lat, float lon, float speedKmh, int satellites) {
  phoneGpsFixSet = true;
  phoneGpsLat = lat;
  phoneGpsLon = lon;
  phoneGpsSpeed = speedKmh;
  phoneGpsSatellites = satellites;
}
void captureStepperBowZero() { stepperBowCaptured = true; }

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
  phoneGpsFixSet = false;
  phoneGpsLat = 0.0f;
  phoneGpsLon = 0.0f;
  phoneGpsSpeed = 0.0f;
  phoneGpsSatellites = 0;
  stepperBowCaptured = false;
}

void tearDown() {}

void test_set_hold_heading() {
  handleBleCommand("SET_HOLD_HEADING:1");
  TEST_ASSERT_EQUAL_FLOAT(1.0f, settings.get("HoldHeading"));
}

void test_set_step_spr() {
  handleBleCommand("SET_STEP_SPR:400");
  TEST_ASSERT_EQUAL_FLOAT(4096.0f, settings.get("StepSpr"));
  TEST_ASSERT_TRUE(stepperControl.loadCalled);
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

void test_set_stepper_bow_calls_capture() {
  handleBleCommand("SET_STEPPER_BOW");
  TEST_ASSERT_TRUE(stepperBowCaptured);
}

void test_set_phone_gps_with_speed() {
  handleBleCommand("SET_PHONE_GPS:59.9386,30.3141,12.7");
  TEST_ASSERT_TRUE(phoneGpsFixSet);
  TEST_ASSERT_EQUAL_FLOAT(59.9386f, phoneGpsLat);
  TEST_ASSERT_EQUAL_FLOAT(30.3141f, phoneGpsLon);
  TEST_ASSERT_EQUAL_FLOAT(12.7f, phoneGpsSpeed);
}

void test_set_phone_gps_without_speed() {
  handleBleCommand("SET_PHONE_GPS:59.9386,30.3141");
  TEST_ASSERT_TRUE(phoneGpsFixSet);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, phoneGpsSpeed);
  TEST_ASSERT_EQUAL(0, phoneGpsSatellites);
}

void test_set_phone_gps_with_satellites() {
  handleBleCommand("SET_PHONE_GPS:59.9386,30.3141,12.7,9");
  TEST_ASSERT_TRUE(phoneGpsFixSet);
  TEST_ASSERT_EQUAL_FLOAT(12.7f, phoneGpsSpeed);
  TEST_ASSERT_EQUAL(9, phoneGpsSatellites);
}

void test_removed_route_commands_do_not_change_state() {
  settings.set("AnchorEnabled", 1.0f);
  handleBleCommand("SET_ROUTE:59.100,30.200;59.300,30.400");
  handleBleCommand("START_ROUTE");
  handleBleCommand("STOP_ROUTE");
  TEST_ASSERT_EQUAL_FLOAT(1.0f, settings.get("AnchorEnabled"));
  TEST_ASSERT_FALSE(manualMode);
  TEST_ASSERT_EQUAL(-1, manualDir);
  TEST_ASSERT_EQUAL(0, manualSpeed);
}

void test_removed_compass_and_log_commands_do_not_change_state() {
  settings.set("HoldHeading", 0.0f);
  const float offsetBefore = compass.getHeadingOffsetDeg();

  handleBleCommand("CALIB_COMPASS");
  handleBleCommand("SET_HEADING:123.4");
  handleBleCommand("EMU_COMPASS:1");
  handleBleCommand("EXPORT_LOG");
  handleBleCommand("CLEAR_LOG");

  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("HoldHeading"));
  TEST_ASSERT_EQUAL_FLOAT(offsetBefore, compass.getHeadingOffsetDeg());
  TEST_ASSERT_FALSE(phoneGpsFixSet);
}

void test_set_compass_offset() {
  handleBleCommand("SET_COMPASS_OFFSET:12.5");
  TEST_ASSERT_EQUAL_FLOAT(12.5f, compass.getHeadingOffsetDeg());
  TEST_ASSERT_EQUAL_FLOAT(12.5f, settings.get("MagOffX"));
  handleBleCommand("RESET_COMPASS_OFFSET");
  TEST_ASSERT_EQUAL_FLOAT(0.0f, compass.getHeadingOffsetDeg());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("MagOffX"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_set_hold_heading);
  RUN_TEST(test_set_step_spr);
  RUN_TEST(test_manual_on_cancels_stepper_move);
  RUN_TEST(test_manual_off);
  RUN_TEST(test_manual_dir_and_speed);
  RUN_TEST(test_set_stepper_bow_calls_capture);
  RUN_TEST(test_set_phone_gps_with_speed);
  RUN_TEST(test_set_phone_gps_without_speed);
  RUN_TEST(test_set_phone_gps_with_satellites);
  RUN_TEST(test_removed_route_commands_do_not_change_state);
  RUN_TEST(test_removed_compass_and_log_commands_do_not_change_state);
  RUN_TEST(test_set_compass_offset);
  return UNITY_END();
}
