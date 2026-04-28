#include "../mocks/BleCommandHandler.h"
#include "ManualControl.h"

#include <cstdlib>
#include <cstring>
#include <unity.h>

AnchorControl anchor;
StepperControl stepperControl;
MotorControl motor;
Settings settings;
BNO08xCompass compass;
BleOtaUpdate bleOta;
bool headingAvailableStub = false;
ManualControl manualControl;
bool phoneGpsFixSet = false;
float phoneGpsLat = 0.0f;
float phoneGpsLon = 0.0f;
float phoneGpsSpeed = 0.0f;
int phoneGpsSatellites = 0;
bool stepperBowCaptured = false;
bool anchorPointPresent = true;
bool anchorEnableAllowed = true;
bool stopAllMotionCalled = false;
int controlActivityNotes = 0;
bool nudgeCardinalCalled = false;
bool nudgeBearingCalled = false;
char nudgeDirLast[16] = {0};
float nudgeBearingLast = 0.0f;
bool nudgeCardinalResult = true;
bool nudgeBearingResult = true;
const char* gnssReasonStub = "GPS_HDOP_TOO_HIGH";
AnchorDeniedReason gnssDeniedReasonStub = AnchorDeniedReason::GPS_HDOP_TOO_HIGH;
AnchorDeniedReason anchorEnableDeniedReasonStub = AnchorDeniedReason::NONE;
AnchorDeniedReason lastDeniedReason = AnchorDeniedReason::NONE;
FailsafeReason lastFailsafeReason = FailsafeReason::NONE;
int clearSafeHoldCalls = 0;
int manualStopCalls = 0;
bool simHandlerConsumes = false;
int simHandlerCalls = 0;
std::string simLastCommand;

void setPhoneGpsFix(float lat, float lon, float speedKmh, int satellites) {
  phoneGpsFixSet = true;
  phoneGpsLat = lat;
  phoneGpsLon = lon;
  phoneGpsSpeed = speedKmh;
  phoneGpsSatellites = satellites;
}

void captureStepperBowZero() { stepperBowCaptured = true; }
bool canEnableAnchorNow() { return anchorEnableAllowed; }
bool hasAnchorPoint() { return anchorPointPresent; }
void stopAllMotionNow() { stopAllMotionCalled = true; }
void noteControlActivityNow() { ++controlActivityNotes; }

bool nudgeAnchorCardinal(const char* dir) {
  nudgeCardinalCalled = true;
  strncpy(nudgeDirLast, dir ? dir : "", sizeof(nudgeDirLast) - 1);
  nudgeDirLast[sizeof(nudgeDirLast) - 1] = '\0';
  return nudgeCardinalResult;
}

bool nudgeAnchorBearing(float bearingDeg) {
  nudgeBearingCalled = true;
  nudgeBearingLast = bearingDeg;
  return nudgeBearingResult;
}

const char* currentGnssFailReason() { return gnssReasonStub; }
AnchorDeniedReason currentAnchorEnableDeniedReason() {
  return anchorEnableDeniedReasonStub;
}
AnchorDeniedReason currentGnssDeniedReason() { return gnssDeniedReasonStub; }
void setLastAnchorDeniedReason(AnchorDeniedReason reason) {
  lastDeniedReason = reason;
}
void setLastFailsafeReason(FailsafeReason reason) { lastFailsafeReason = reason; }
void clearSafeHold() { ++clearSafeHoldCalls; }

bool setManualControlFromBle(float angleDeg, int throttlePct, unsigned long ttlMs) {
  const bool wasActive = manualControl.active();
  if (!manualControl.apply(ManualControlSource::BLE_PHONE,
                           angleDeg,
                           throttlePct,
                           ttlMs,
                           1000)) {
    return false;
  }
  if (!wasActive) {
    setLastAnchorDeniedReason(AnchorDeniedReason::NONE);
    setLastFailsafeReason(FailsafeReason::NONE);
    clearSafeHold();
    stepperControl.cancelMove();
    if (settings.get("AnchorEnabled") == 1.0f) {
      settings.set("AnchorEnabled", 0.0f);
    }
  }
  return true;
}

void stopManualControlFromBle() {
  ++manualStopCalls;
  manualControl.stop();
  stepperControl.stopManual();
  stepperControl.cancelMove();
  motor.stop();
}

bool preprocessBleCommand(const std::string& incoming, std::string* effective) {
  if (!effective) {
    return false;
  }
  *effective = incoming;
  return true;
}

bool handleSimCommand(const std::string& command) {
  ++simHandlerCalls;
  simLastCommand = command;
  return simHandlerConsumes;
}

bool headingAvailable() { return headingAvailableStub; }

void setUp() {
  settings.reset();
  anchor.attachSettings(&settings);
  stepperControl.cancelCalled = false;
  stepperControl.stopManualCalled = false;
  stepperControl.loadCalled = false;
  motor.stopCalled = false;
  headingAvailableStub = false;
  compass.az = 0.0f;
  compass.startDynamicCalibrationCalls = 0;
  compass.saveDynamicCalibrationCalls = 0;
  compass.setDcdAutoSaveCalls = 0;
  compass.tareHeadingNowCalls = 0;
  compass.saveTareCalls = 0;
  compass.clearTareCalls = 0;
  compass.lastDcdAutoSave = false;
  manualControl.stop();
  phoneGpsFixSet = false;
  phoneGpsLat = 0.0f;
  phoneGpsLon = 0.0f;
  phoneGpsSpeed = 0.0f;
  phoneGpsSatellites = 0;
  stepperBowCaptured = false;
  anchorPointPresent = true;
  anchorEnableAllowed = true;
  stopAllMotionCalled = false;
  controlActivityNotes = 0;
  nudgeCardinalCalled = false;
  nudgeBearingCalled = false;
  nudgeDirLast[0] = '\0';
  nudgeBearingLast = 0.0f;
  nudgeCardinalResult = true;
  nudgeBearingResult = true;
  gnssReasonStub = "GPS_HDOP_TOO_HIGH";
  gnssDeniedReasonStub = AnchorDeniedReason::GPS_HDOP_TOO_HIGH;
  anchorEnableDeniedReasonStub = AnchorDeniedReason::NONE;
  lastDeniedReason = AnchorDeniedReason::NONE;
  lastFailsafeReason = FailsafeReason::NONE;
  clearSafeHoldCalls = 0;
  manualStopCalls = 0;
  simHandlerConsumes = false;
  simHandlerCalls = 0;
  simLastCommand.clear();
  bleOta.reset();
}

void tearDown() {}

void test_default_release_allows_release_command_path() {
  handleBleCommand("SET_HOLD_HEADING:1");

  TEST_ASSERT_EQUAL(1, controlActivityNotes);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, settings.get("HoldHeading"));
}

void test_default_release_allows_sim_command_path() {
  simHandlerConsumes = true;

  handleBleCommand("SIM_STATUS");

  TEST_ASSERT_EQUAL(1, controlActivityNotes);
  TEST_ASSERT_EQUAL(1, simHandlerCalls);
  TEST_ASSERT_EQUAL_STRING("SIM_STATUS", simLastCommand.c_str());
}

void test_default_release_accepts_ota_after_safe_state_side_effects() {
  settings.set("AnchorEnabled", 1.0f);
  manualControl.apply(ManualControlSource::BLE_PHONE, 1, 50, 500, 1000);

  handleBleCommand("OTA_BEGIN:4096,00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff");

  TEST_ASSERT_EQUAL(1, controlActivityNotes);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorEnabled"));
  TEST_ASSERT_FALSE(manualControl.active());
  TEST_ASSERT_EQUAL(1, manualStopCalls);
  TEST_ASSERT_TRUE(stepperControl.cancelCalled);
  TEST_ASSERT_TRUE(motor.stopCalled);
  TEST_ASSERT_TRUE(stopAllMotionCalled);
  TEST_ASSERT_EQUAL(1, bleOta.beginCalls);
}

void test_default_release_accepts_setup_commands() {
  handleBleCommand("SET_COMPASS_OFFSET:12.5");
  handleBleCommand("SET_STEP_MAXSPD:1234");
  handleBleCommand("SET_STEP_SPR:400");
  handleBleCommand("SET_STEP_GEAR:36");
  handleBleCommand("SET_STEPPER_BOW");

  TEST_ASSERT_EQUAL(5, controlActivityNotes);
  TEST_ASSERT_EQUAL_FLOAT(12.5f, compass.getHeadingOffsetDeg());
  TEST_ASSERT_EQUAL_FLOAT(1234.0f, settings.get("StepMaxSpd"));
  TEST_ASSERT_EQUAL_FLOAT(400.0f, settings.get("StepSpr"));
  TEST_ASSERT_EQUAL_FLOAT(36.0f, settings.get("StepGear"));
  TEST_ASSERT_TRUE(stepperControl.loadCalled);
  TEST_ASSERT_TRUE(stepperBowCaptured);
}

void test_default_release_rejects_external_sensor_injection_before_side_effects() {
  handleBleCommand("SET_PHONE_GPS:59.9386,30.3141,12.7,9");

  TEST_ASSERT_EQUAL(0, controlActivityNotes);
  TEST_ASSERT_EQUAL(0, simHandlerCalls);
  TEST_ASSERT_TRUE(simLastCommand.empty());
  TEST_ASSERT_FALSE(phoneGpsFixSet);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_default_release_allows_release_command_path);
  RUN_TEST(test_default_release_allows_sim_command_path);
  RUN_TEST(test_default_release_accepts_ota_after_safe_state_side_effects);
  RUN_TEST(test_default_release_accepts_setup_commands);
  RUN_TEST(test_default_release_rejects_external_sensor_injection_before_side_effects);
  return UNITY_END();
}
