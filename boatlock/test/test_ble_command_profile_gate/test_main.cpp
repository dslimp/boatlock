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
bool securityForceReject = false;
bool securityRequireWrapper = false;
bool securitySessionActive = false;
uint32_t securityLastCounter = 0;
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

bool preprocessSecureCommand(const std::string& incoming, std::string* effective) {
  if (!effective) {
    return false;
  }
  if (securityForceReject) {
    return false;
  }
  if (!securityRequireWrapper) {
    *effective = incoming;
    return true;
  }
  if (incoming.rfind("SEC_CMD:", 0) != 0) {
    return false;
  }
  const size_t p1 = incoming.find(':', 8);
  const size_t p2 =
      (p1 == std::string::npos) ? std::string::npos : incoming.find(':', p1 + 1);
  if (p1 == std::string::npos || p2 == std::string::npos ||
      p2 + 1 >= incoming.size()) {
    return false;
  }
  const std::string counterHex = incoming.substr(8, p1 - 8);
  const uint32_t counter =
      static_cast<uint32_t>(strtoul(counterHex.c_str(), nullptr, 16));
  if (!securitySessionActive || counter <= securityLastCounter) {
    return false;
  }
  securityLastCounter = counter;
  *effective = incoming.substr(p2 + 1);
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
  securityForceReject = false;
  securityRequireWrapper = false;
  securitySessionActive = false;
  securityLastCounter = 0;
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

void test_default_release_rejects_ota_before_safe_state_side_effects() {
  settings.set("AnchorEnabled", 1.0f);
  manualControl.apply(ManualControlSource::BLE_PHONE, 1, 50, 500, 1000);

  handleBleCommand("OTA_BEGIN:4096,00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff");

  TEST_ASSERT_EQUAL(0, controlActivityNotes);
  TEST_ASSERT_EQUAL((int)FailsafeReason::NONE, (int)lastFailsafeReason);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, settings.get("AnchorEnabled"));
  TEST_ASSERT_TRUE(manualControl.active());
  TEST_ASSERT_EQUAL(0, manualStopCalls);
  TEST_ASSERT_FALSE(stepperControl.cancelCalled);
  TEST_ASSERT_FALSE(motor.stopCalled);
  TEST_ASSERT_FALSE(stopAllMotionCalled);
  TEST_ASSERT_EQUAL(0, bleOta.beginCalls);
}

void test_default_release_rejects_service_tuning_before_side_effects() {
  const float compassOffsetBefore = compass.getHeadingOffsetDeg();
  const float stepMaxBefore = settings.get("StepMaxSpd");

  handleBleCommand("SET_COMPASS_OFFSET:12.5");
  handleBleCommand("SET_STEP_MAXSPD:1234");
  handleBleCommand("SET_STEPPER_BOW");

  TEST_ASSERT_EQUAL(0, controlActivityNotes);
  TEST_ASSERT_EQUAL_FLOAT(compassOffsetBefore, compass.getHeadingOffsetDeg());
  TEST_ASSERT_EQUAL_FLOAT(stepMaxBefore, settings.get("StepMaxSpd"));
  TEST_ASSERT_FALSE(stepperControl.loadCalled);
  TEST_ASSERT_FALSE(stepperBowCaptured);
}

void test_default_release_rejects_external_sensor_injection_before_side_effects() {
  handleBleCommand("SET_PHONE_GPS:59.9386,30.3141,12.7,9");

  TEST_ASSERT_EQUAL(0, controlActivityNotes);
  TEST_ASSERT_EQUAL(0, simHandlerCalls);
  TEST_ASSERT_TRUE(simLastCommand.empty());
  TEST_ASSERT_FALSE(phoneGpsFixSet);
}

void test_default_release_gates_unwrapped_secure_payload_scope() {
  securityRequireWrapper = true;
  securitySessionActive = true;

  handleBleCommand("SEC_CMD:1:deadbeef:SET_COMPASS_OFFSET:12.5");

  TEST_ASSERT_EQUAL_UINT32(1, securityLastCounter);
  TEST_ASSERT_EQUAL(0, controlActivityNotes);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, compass.getHeadingOffsetDeg());

  handleBleCommand("SEC_CMD:2:deadbeef:SET_HOLD_HEADING:1");

  TEST_ASSERT_EQUAL_UINT32(2, securityLastCounter);
  TEST_ASSERT_EQUAL(1, controlActivityNotes);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, settings.get("HoldHeading"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_default_release_allows_release_command_path);
  RUN_TEST(test_default_release_allows_sim_command_path);
  RUN_TEST(test_default_release_rejects_ota_before_safe_state_side_effects);
  RUN_TEST(test_default_release_rejects_service_tuning_before_side_effects);
  RUN_TEST(test_default_release_rejects_external_sensor_injection_before_side_effects);
  RUN_TEST(test_default_release_gates_unwrapped_secure_payload_scope);
  return UNITY_END();
}
