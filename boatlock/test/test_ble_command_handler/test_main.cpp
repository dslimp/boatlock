#include "../mocks/BleCommandHandler.h"
#include "ManualControl.h"
#include <unity.h>
#include <cstring>
#include <cstdlib>

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
AnchorDeniedReason currentAnchorEnableDeniedReason() { return anchorEnableDeniedReasonStub; }
AnchorDeniedReason currentGnssDeniedReason() { return gnssDeniedReasonStub; }
void setLastAnchorDeniedReason(AnchorDeniedReason reason) { lastDeniedReason = reason; }
void setLastFailsafeReason(FailsafeReason reason) { lastFailsafeReason = reason; }
void clearSafeHold() { ++clearSafeHoldCalls; }
bool setManualControlFromBle(int steer, int throttlePct, unsigned long ttlMs) {
  const bool wasActive = manualControl.active();
  if (!manualControl.apply(ManualControlSource::BLE_PHONE, steer, throttlePct, ttlMs, 1000)) {
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
  motor.stop();
}
bool preprocessSecureCommand(const std::string& incoming, std::string* effective) {
  if (!effective) return false;
  if (securityForceReject) return false;
  if (!securityRequireWrapper) {
    *effective = incoming;
    return true;
  }
  if (incoming.rfind("SEC_CMD:", 0) != 0) {
    return false;
  }
  size_t p1 = incoming.find(':', 8);
  size_t p2 = (p1 == std::string::npos) ? std::string::npos : incoming.find(':', p1 + 1);
  if (p1 == std::string::npos || p2 == std::string::npos || p2 + 1 >= incoming.size()) {
    return false;
  }
  const std::string counterHex = incoming.substr(8, p1 - 8);
  uint32_t counter = (uint32_t)strtoul(counterHex.c_str(), nullptr, 16);
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

void test_set_hold_heading() {
  handleBleCommand("SET_HOLD_HEADING:1");
  TEST_ASSERT_EQUAL_FLOAT(1.0f, settings.get("HoldHeading"));
}

void test_ota_begin_enters_safe_state_and_starts_update() {
  settings.set("AnchorEnabled", 1.0f);
  manualControl.apply(ManualControlSource::BLE_PHONE, 1, 50, 500, 1000);

  handleBleCommand("OTA_BEGIN:4096,00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff");

  TEST_ASSERT_TRUE(lastFailsafeReason == FailsafeReason::STOP_CMD);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorEnabled"));
  TEST_ASSERT_EQUAL(1, manualStopCalls);
  TEST_ASSERT_TRUE(stepperControl.cancelCalled);
  TEST_ASSERT_TRUE(motor.stopCalled);
  TEST_ASSERT_TRUE(stopAllMotionCalled);
  TEST_ASSERT_EQUAL(1, bleOta.beginCalls);
  TEST_ASSERT_EQUAL_UINT32(4096, bleOta.lastImageSize);
  TEST_ASSERT_EQUAL_STRING("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                           bleOta.lastSha256Hex);
}

void test_ota_active_blocks_runtime_commands_until_finish_or_abort() {
  bleOta.setActive(true);
  handleBleCommand("MANUAL_SET:1,40,500");
  TEST_ASSERT_FALSE(manualControl.active());

  handleBleCommand("OTA_FINISH");
  TEST_ASSERT_EQUAL(1, bleOta.finishCalls);
}

void test_manual_set_is_atomic_and_enters_manual_mode() {
  handleBleCommand("MANUAL_SET:-1,42,500");
  TEST_ASSERT_TRUE(manualControl.active());
  TEST_ASSERT_EQUAL(-1, manualControl.steer());
  TEST_ASSERT_EQUAL(42, manualControl.throttlePct());
  TEST_ASSERT_EQUAL(0, manualControl.stepperDir());
  TEST_ASSERT_EQUAL(107, manualControl.motorPwm());
  TEST_ASSERT_TRUE(stepperControl.cancelCalled);
  TEST_ASSERT_EQUAL(1, clearSafeHoldCalls);
}

void test_manual_set_disarms_anchor_without_reenable_path() {
  settings.set("AnchorEnabled", 1.0f);
  handleBleCommand("MANUAL_SET:1,-30,500");
  TEST_ASSERT_TRUE(manualControl.active());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorEnabled"));
}

void test_manual_off_stops_manual_outputs() {
  manualControl.apply(ManualControlSource::BLE_PHONE, 1, 50, 500, 1000);
  handleBleCommand("MANUAL_OFF");
  TEST_ASSERT_FALSE(manualControl.active());
  TEST_ASSERT_TRUE(stepperControl.stopManualCalled);
  TEST_ASSERT_TRUE(motor.stopCalled);
  TEST_ASSERT_EQUAL(1, manualStopCalls);
}

void test_removed_split_manual_commands_do_not_change_state() {
  handleBleCommand("MANUAL:1");
  handleBleCommand("MANUAL_DIR:1");
  handleBleCommand("MANUAL_SPEED:-123");
  TEST_ASSERT_FALSE(manualControl.active());
  TEST_ASSERT_EQUAL(0, manualControl.throttlePct());
}

void test_manual_set_rejects_out_of_range_payloads() {
  handleBleCommand("MANUAL_SET:2,10,500");
  handleBleCommand("MANUAL_SET:0,101,500");
  handleBleCommand("MANUAL_SET:0,10,50");
  TEST_ASSERT_FALSE(manualControl.active());
}

void test_set_stepper_bow_calls_capture() {
  handleBleCommand("SET_STEPPER_BOW");
  TEST_ASSERT_TRUE(stepperBowCaptured);
}

void test_set_anchor_saves_point_without_enabling_mode() {
  settings.set("AnchorEnabled", 1.0f);
  handleBleCommand("SET_ANCHOR:59.9386,30.3141");
  TEST_ASSERT_EQUAL_FLOAT(59.9386f, anchor.anchorLat);
  TEST_ASSERT_EQUAL_FLOAT(30.3141f, anchor.anchorLon);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, anchor.anchorHeading);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorEnabled"));
}

void test_set_anchor_rejects_unconfigured_zero_point() {
  anchor.saveAnchor(10.0f, 20.0f, 30.0f, false);
  handleBleCommand("SET_ANCHOR:0.0,0.0");
  TEST_ASSERT_EQUAL_FLOAT(10.0f, anchor.anchorLat);
  TEST_ASSERT_EQUAL_FLOAT(20.0f, anchor.anchorLon);
  TEST_ASSERT_EQUAL_FLOAT(30.0f, anchor.anchorHeading);
}

void test_set_anchor_uses_only_fresh_heading() {
  compass.az = 123.4f;
  headingAvailableStub = false;
  handleBleCommand("SET_ANCHOR:59.9386,30.3141");
  TEST_ASSERT_EQUAL_FLOAT(0.0f, anchor.anchorHeading);

  headingAvailableStub = true;
  handleBleCommand("SET_ANCHOR:59.9387,30.3142");
  TEST_ASSERT_EQUAL_FLOAT(123.4f, anchor.anchorHeading);
}

void test_anchor_on_rejected_without_anchor_point() {
  anchorPointPresent = false;
  anchorEnableDeniedReasonStub = AnchorDeniedReason::NO_ANCHOR_POINT;
  settings.set("AnchorEnabled", 0.0f);
  handleBleCommand("ANCHOR_ON");
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorEnabled"));
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::NO_ANCHOR_POINT, (int)lastDeniedReason);
  TEST_ASSERT_EQUAL(0, clearSafeHoldCalls);
}

void test_anchor_on_rejected_on_bad_gnss() {
  anchorPointPresent = true;
  anchorEnableAllowed = false;
  anchorEnableDeniedReasonStub = AnchorDeniedReason::GPS_HDOP_TOO_HIGH;
  settings.set("AnchorEnabled", 0.0f);
  handleBleCommand("ANCHOR_ON");
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorEnabled"));
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::GPS_HDOP_TOO_HIGH, (int)lastDeniedReason);
}

void test_anchor_on_rejected_without_heading() {
  anchorPointPresent = true;
  anchorEnableAllowed = false;
  anchorEnableDeniedReasonStub = AnchorDeniedReason::NO_HEADING;
  settings.set("AnchorEnabled", 0.0f);
  handleBleCommand("ANCHOR_ON");
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorEnabled"));
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::NO_HEADING, (int)lastDeniedReason);
}

void test_anchor_on_accepts_and_exits_manual_mode() {
  manualControl.apply(ManualControlSource::BLE_PHONE, 1, 90, 500, 1000);
  anchorPointPresent = true;
  anchorEnableAllowed = true;
  anchorEnableDeniedReasonStub = AnchorDeniedReason::NONE;
  settings.set("AnchorEnabled", 0.0f);
  handleBleCommand("ANCHOR_ON");
  TEST_ASSERT_FALSE(manualControl.active());
  TEST_ASSERT_EQUAL(1, manualStopCalls);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, settings.get("AnchorEnabled"));
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::NONE, (int)lastDeniedReason);
  TEST_ASSERT_EQUAL(1, clearSafeHoldCalls);
}

void test_anchor_off_always_disables_anchor_and_stops_drive() {
  settings.set("AnchorEnabled", 1.0f);
  manualControl.apply(ManualControlSource::BLE_PHONE, 1, 25, 500, 1000);
  handleBleCommand("ANCHOR_OFF");
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorEnabled"));
  TEST_ASSERT_FALSE(manualControl.active());
  TEST_ASSERT_TRUE(stepperControl.cancelCalled);
  TEST_ASSERT_TRUE(motor.stopCalled);
  TEST_ASSERT_EQUAL(1, manualStopCalls);
  TEST_ASSERT_EQUAL((int)FailsafeReason::NONE, (int)lastFailsafeReason);
  TEST_ASSERT_EQUAL(1, clearSafeHoldCalls);
}

void test_manual_on_clears_safe_hold_and_failsafe_latch() {
  lastDeniedReason = AnchorDeniedReason::GPS_HDOP_TOO_HIGH;
  lastFailsafeReason = FailsafeReason::COMM_TIMEOUT;
  handleBleCommand("MANUAL_SET:0,0,500");
  TEST_ASSERT_TRUE(manualControl.active());
  TEST_ASSERT_EQUAL((int)AnchorDeniedReason::NONE, (int)lastDeniedReason);
  TEST_ASSERT_EQUAL((int)FailsafeReason::NONE, (int)lastFailsafeReason);
  TEST_ASSERT_EQUAL(1, clearSafeHoldCalls);
}

void test_stop_always_has_highest_priority() {
  stopAllMotionCalled = false;
  handleBleCommand("STOP");
  TEST_ASSERT_TRUE(stopAllMotionCalled);
  TEST_ASSERT_EQUAL((int)FailsafeReason::STOP_CMD, (int)lastFailsafeReason);
}

void test_heartbeat_marks_control_activity_without_state_change() {
  settings.set("AnchorEnabled", 1.0f);
  handleBleCommand("HEARTBEAT");
  TEST_ASSERT_EQUAL(1, controlActivityNotes);
  TEST_ASSERT_EQUAL_FLOAT(1.0f, settings.get("AnchorEnabled"));
  TEST_ASSERT_FALSE(stopAllMotionCalled);
}

void test_nudge_dir_routes_to_cardinal_handler() {
  handleBleCommand("NUDGE_DIR:LEFT");
  TEST_ASSERT_TRUE(nudgeCardinalCalled);
  TEST_ASSERT_EQUAL_STRING("LEFT", nudgeDirLast);
  TEST_ASSERT_FALSE(nudgeBearingCalled);

  nudgeCardinalCalled = false;
  handleBleCommand("NUDGE_DIR:LEFT,2.5");
  TEST_ASSERT_FALSE(nudgeCardinalCalled);
}

void test_nudge_bearing_routes_to_bearing_handler() {
  handleBleCommand("NUDGE_BRG:123.4");
  TEST_ASSERT_TRUE(nudgeBearingCalled);
  TEST_ASSERT_EQUAL_FLOAT(123.4f, nudgeBearingLast);
  TEST_ASSERT_FALSE(nudgeCardinalCalled);

  nudgeBearingCalled = false;
  handleBleCommand("NUDGE_BRG:123.4,3.0");
  TEST_ASSERT_FALSE(nudgeBearingCalled);
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
  TEST_ASSERT_FALSE(manualControl.active());
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

void test_compass_calibration_commands_are_routed_to_compass() {
  handleBleCommand("COMPASS_CAL_START");
  handleBleCommand("COMPASS_DCD_SAVE");
  handleBleCommand("COMPASS_DCD_AUTOSAVE_ON");
  TEST_ASSERT_TRUE(compass.lastDcdAutoSave);
  handleBleCommand("COMPASS_DCD_AUTOSAVE_OFF");
  TEST_ASSERT_FALSE(compass.lastDcdAutoSave);
  handleBleCommand("COMPASS_TARE_Z");
  handleBleCommand("COMPASS_TARE_SAVE");
  handleBleCommand("COMPASS_TARE_CLEAR");

  TEST_ASSERT_EQUAL(1, compass.startDynamicCalibrationCalls);
  TEST_ASSERT_EQUAL(1, compass.saveDynamicCalibrationCalls);
  TEST_ASSERT_EQUAL(2, compass.setDcdAutoSaveCalls);
  TEST_ASSERT_EQUAL(1, compass.tareHeadingNowCalls);
  TEST_ASSERT_EQUAL(1, compass.saveTareCalls);
  TEST_ASSERT_EQUAL(1, compass.clearTareCalls);
}

void test_set_anchor_profile_applies_bundle() {
  handleBleCommand("SET_ANCHOR_PROFILE:quiet");
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorProf"));
  TEST_ASSERT_EQUAL_FLOAT(3.0f, settings.get("HoldRadius"));
  TEST_ASSERT_EQUAL_FLOAT(2.2f, settings.get("DeadbandM"));
  TEST_ASSERT_EQUAL_FLOAT(45.0f, settings.get("MaxThrustA"));
  TEST_ASSERT_EQUAL_FLOAT(20.0f, settings.get("ThrRampA"));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("ReacqStrat"));
}

void test_set_anchor_profile_rejects_invalid_payload() {
  const float holdBefore = settings.get("HoldRadius");
  const float profileBefore = settings.get("AnchorProf");
  handleBleCommand("SET_ANCHOR_PROFILE:storm");
  TEST_ASSERT_EQUAL_FLOAT(profileBefore, settings.get("AnchorProf"));
  TEST_ASSERT_EQUAL_FLOAT(holdBefore, settings.get("HoldRadius"));
}

void test_security_rejects_plain_control_command_when_wrapper_required() {
  securityRequireWrapper = true;
  securitySessionActive = true;
  settings.set("AnchorEnabled", 0.0f);
  handleBleCommand("ANCHOR_ON");
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorEnabled"));
}

void test_security_accepts_wrapped_command_with_increasing_counter() {
  securityRequireWrapper = true;
  securitySessionActive = true;
  anchorPointPresent = true;
  anchorEnableAllowed = true;

  handleBleCommand("SEC_CMD:1:deadbeef:ANCHOR_ON");
  TEST_ASSERT_EQUAL_FLOAT(1.0f, settings.get("AnchorEnabled"));

  settings.set("AnchorEnabled", 0.0f);
  handleBleCommand("SEC_CMD:1:deadbeef:ANCHOR_ON");
  TEST_ASSERT_EQUAL_FLOAT(0.0f, settings.get("AnchorEnabled"));
}

void test_command_parser_fuzz_does_not_break_safe_state() {
  srand(42);
  settings.set("AnchorEnabled", 0.0f);
  for (int i = 0; i < 500; ++i) {
    char cmd[96];
    int len = 1 + (rand() % 80);
    for (int j = 0; j < len; ++j) {
      const int r = 32 + (rand() % 95);  // printable ASCII
      cmd[j] = static_cast<char>(r);
    }
    cmd[len] = '\0';
    handleBleCommand(cmd);
  }
  TEST_ASSERT_TRUE(settings.get("AnchorEnabled") == 0.0f || settings.get("AnchorEnabled") == 1.0f);
  TEST_ASSERT_FALSE(manualControl.active());
}

void test_sim_command_is_forwarded_and_consumed() {
  simHandlerConsumes = true;
  handleBleCommand("SIM_STATUS");
  TEST_ASSERT_EQUAL(1, simHandlerCalls);
  TEST_ASSERT_EQUAL_STRING("SIM_STATUS", simLastCommand.c_str());
  TEST_ASSERT_FALSE(stopAllMotionCalled);
}

void test_non_sim_command_still_runs_when_sim_handler_declines() {
  simHandlerConsumes = false;
  handleBleCommand("STOP");
  TEST_ASSERT_EQUAL(1, simHandlerCalls);
  TEST_ASSERT_EQUAL_STRING("STOP", simLastCommand.c_str());
  TEST_ASSERT_TRUE(stopAllMotionCalled);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_set_hold_heading);
  RUN_TEST(test_ota_begin_enters_safe_state_and_starts_update);
  RUN_TEST(test_ota_active_blocks_runtime_commands_until_finish_or_abort);
  RUN_TEST(test_manual_set_is_atomic_and_enters_manual_mode);
  RUN_TEST(test_manual_set_disarms_anchor_without_reenable_path);
  RUN_TEST(test_manual_off_stops_manual_outputs);
  RUN_TEST(test_removed_split_manual_commands_do_not_change_state);
  RUN_TEST(test_manual_set_rejects_out_of_range_payloads);
  RUN_TEST(test_set_stepper_bow_calls_capture);
  RUN_TEST(test_set_anchor_saves_point_without_enabling_mode);
  RUN_TEST(test_set_anchor_rejects_unconfigured_zero_point);
  RUN_TEST(test_set_anchor_uses_only_fresh_heading);
  RUN_TEST(test_anchor_on_rejected_without_anchor_point);
  RUN_TEST(test_anchor_on_rejected_on_bad_gnss);
  RUN_TEST(test_anchor_on_rejected_without_heading);
  RUN_TEST(test_anchor_on_accepts_and_exits_manual_mode);
  RUN_TEST(test_anchor_off_always_disables_anchor_and_stops_drive);
  RUN_TEST(test_manual_on_clears_safe_hold_and_failsafe_latch);
  RUN_TEST(test_stop_always_has_highest_priority);
  RUN_TEST(test_heartbeat_marks_control_activity_without_state_change);
  RUN_TEST(test_nudge_dir_routes_to_cardinal_handler);
  RUN_TEST(test_nudge_bearing_routes_to_bearing_handler);
  RUN_TEST(test_set_phone_gps_with_speed);
  RUN_TEST(test_set_phone_gps_without_speed);
  RUN_TEST(test_set_phone_gps_with_satellites);
  RUN_TEST(test_removed_route_commands_do_not_change_state);
  RUN_TEST(test_removed_compass_and_log_commands_do_not_change_state);
  RUN_TEST(test_set_compass_offset);
  RUN_TEST(test_compass_calibration_commands_are_routed_to_compass);
  RUN_TEST(test_set_anchor_profile_applies_bundle);
  RUN_TEST(test_set_anchor_profile_rejects_invalid_payload);
  RUN_TEST(test_security_rejects_plain_control_command_when_wrapper_required);
  RUN_TEST(test_security_accepts_wrapped_command_with_increasing_counter);
  RUN_TEST(test_command_parser_fuzz_does_not_break_safe_state);
  RUN_TEST(test_sim_command_is_forwarded_and_consumed);
  RUN_TEST(test_non_sim_command_still_runs_when_sim_handler_declines);
  return UNITY_END();
}
