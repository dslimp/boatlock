#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <EEPROM.h>
#include <AccelStepper.h>
#include <math.h>
#include <strings.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Settings.h"
#include "Logger.h"
#include "AnchorControl.h"
#include "MotorControl.h"
#include "StepperControl.h"
#include "display.h"
#include "BNO08xCompass.h"
#include "DriftMonitor.h"
#include "AnchorSupervisor.h"
#include "AnchorDiagnostics.h"
#include "GnssQualityGate.h"
#include "AnchorReasons.h"
#include "RuntimeButtons.h"
#include "RuntimeBleParams.h"
#include "RuntimeAnchorNudge.h"
#include "RuntimeAnchorGate.h"
#include "RuntimeCompassHealth.h"
#include "RuntimeCompassRetry.h"
#include "ManualControl.h"
#include "RuntimeControl.h"
#include "RuntimeControlInputBuilder.h"
#include "RuntimeGnss.h"
#include "RuntimeGpsUart.h"
#include "RuntimeMotion.h"
#include "RuntimeSimCommand.h"
#include "RuntimeSimExecution.h"
#include "RuntimeSimLog.h"
#include "RuntimeSimBadge.h"
#include "RuntimeStatus.h"
#include "RuntimeSupervisorPolicy.h"
#include "RuntimeTelemetryCadence.h"
#include "RuntimeUiSnapshot.h"
#include "BleOtaUpdate.h"
#include "HilSimRunner.h"
#include "BleCommandHandler.h"
#include "BLEBoatLock.h"
#include "SdCardLogger.h"

Settings settings;
constexpr size_t EEPROM_SIZE =
    Settings::EEPROM_ADDR + Settings::STORED_BYTES;

BLEBoatLock bleBoatLock;
BleOtaUpdate bleOta;
bool bleStarted = false;

namespace cfg {
constexpr char kFirmwareVersion[] = "0.2.7";

constexpr int kStepperStepPin = 6;
constexpr int kStepperDirPin = 16;
constexpr int kGpsRxPin = 17;
constexpr int kGpsTxPin = 18;
constexpr int kCompassRxPin = 12;
constexpr int kCompassTxPin = 11;
constexpr int kCompassResetPin = 13;
#if BOATLOCK_COMPASS_SH2_UART
constexpr uint32_t kCompassBaud = 3000000;
#else
constexpr uint32_t kCompassBaud = 115200;
#endif
constexpr int kMotorPwmPin = 7;
constexpr int kMotorDirPin1 = 8;
constexpr int kMotorDirPin2 = 10;
constexpr int kBootPin = 0;
constexpr int kStopButtonPin = 15;

constexpr int kPwmFreq = 5000;
constexpr int kPwmResolution = 8;
constexpr int kPwmChannel = 0;

constexpr int kMaxGpsFilterWindow = 20;
constexpr unsigned long kBleNotifyIntervalMs = 1000;
constexpr unsigned long kUiRefreshIntervalMs = 120;
constexpr unsigned long kCompassReadIntervalMs = 50;
constexpr unsigned long kCompassFirstEventTimeoutMs = 3000;
constexpr unsigned long kCompassStaleEventMs = 750;
constexpr unsigned long kBootSaveHoldMs = 1500;
constexpr unsigned long kCompassRetryIntervalMs = 5000;
constexpr unsigned long kGpsNoDataWarnMs = 6000;
constexpr unsigned long kGpsUartStaleRestartMs = 6000;
constexpr unsigned long kGpsUartRestartCooldownMs = 4000;
constexpr unsigned long kSimResultBannerMs = 15000;
constexpr unsigned long kSdTelemetryIntervalMs = 200;
} // namespace cfg

HardwareSerial gpsSerial(1);
HardwareSerial compassSerial(2);
TinyGPSPlus gps;
StepperControl stepperControl(cfg::kStepperStepPin, cfg::kStepperDirPin);

BNO08xCompass compass;
AnchorControl anchor;
MotorControl motor;
DriftMonitor driftMonitor;
AnchorSupervisor anchorSupervisor;
AnchorDiagnostics anchorDiagnostics;

bool compassReady = false;
unsigned long compassReadySinceMs = 0;
bool compassHeadingEventsReadyLogged = false;
float fallbackHeading = 0.0f;
float fallbackBearing = 0.0f;
enum class FixTypeSource : uint8_t {
  NONE = 0,
  NMEA_GSA = 1,
  UBX = 2,
};
FixTypeSource fixTypeSource = FixTypeSource::NONE;
bool minFixTypeIgnoredLogged = false;
unsigned long lastLoopMs = 0;
unsigned long lastSdTelemetryMs = 0;
ManualControl manualControl;
hilsim::HilSimManager hilSim;
unsigned long simLastWallMs = 0;
RuntimeGnss runtimeGnss;
RuntimeMotion runtimeMotion(settings, stepperControl, motor, driftMonitor);

bool headingAvailable() {
  return compassReady && compass.lastHeadingEventAgeMs(millis()) <= cfg::kCompassStaleEventMs;
}

void setPhoneGpsFix(float lat, float lon, float speedKmh, int satellites);
void captureStepperBowZero();
bool canEnableAnchorNow();
bool hasAnchorPoint();
void stopAllMotionNow();
void noteControlActivityNow();
bool nudgeAnchorCardinal(const char* dir);
bool nudgeAnchorBearing(float bearingDeg);
const char* currentGnssFailReason();
AnchorDeniedReason currentAnchorEnableDeniedReason();
void applySupervisorDecision(const AnchorSupervisor::Decision& decision);
void setLastAnchorDeniedReason(AnchorDeniedReason reason);
void setLastFailsafeReason(FailsafeReason reason);
void clearSafeHold();
const char* currentAnchorDeniedReason();
const char* currentFailsafeReason();
AnchorDeniedReason currentGnssDeniedReason();
bool preprocessBleCommand(const std::string& incoming, std::string* effective);
bool handleSimCommand(const std::string& command);

namespace {
TaskHandle_t stepperTaskHandle = nullptr;
bool controlGpsAvailable();

unsigned long lastGpsDebugMs = 0;
unsigned long lastGpsJumpRejectLogMs = 0;
RuntimeButtons runtimeButtons;
RuntimeTelemetryCadence telemetryCadence;
RuntimeGpsUart gpsUart;
RuntimeCompassRetry compassRetry;
unsigned long bootMs = 0;
RuntimeSimBadge runtimeSimBadge;

const char* fixTypeSourceString(FixTypeSource src) {
  switch (src) {
    case FixTypeSource::NMEA_GSA:
      return "NMEA_GSA";
    case FixTypeSource::UBX:
      return "UBX";
    case FixTypeSource::NONE:
    default:
      return "NONE";
  }
}

bool anchorPointConfigured() {
  return AnchorControl::validAnchorPoint(anchor.anchorLat, anchor.anchorLon);
}

bool gpsQualityGoodForAnchorOn() {
  return runtimeGnss.gpsQualityGoodForAnchorOn(settings);
}

int gnssQualityLevel() {
  return runtimeGnss.gnssQualityLevel(settings);
}

void setSafetyReason(const char* reason) {
  runtimeMotion.setSafetyReason(reason, millis());
}

std::string activeSafetyReason(unsigned long now) {
  return runtimeMotion.activeSafetyReason(now);
}

RuntimeStatusInput currentRuntimeStatusInput(unsigned long now, std::string* safetyReason) {
  RuntimeStatusInput input;
  if (hilSim.isRunning()) {
    hilsim::HilScenarioRunner::LiveTelemetry live;
    const bool liveOk = hilSim.liveTelemetry(&live);
    input.gpsUnavailable = !(liveOk && live.gnss.valid);
    input.gnssWeak = false;
    input.gnssWeakReason = nullptr;
    input.headingAvailable = liveOk && live.heading.valid;
    input.driftFail = false;
    input.driftAlert = false;
    input.anchorActive = false;
    input.anchorDeniedReason = nullptr;
    input.failsafeReason = nullptr;
    input.holdMode = false;
    if (safetyReason) {
      safetyReason->clear();
      input.safetyReason = safetyReason->c_str();
    }
    return input;
  }
  input.gpsUnavailable = !gps.location.isValid() && !runtimeGnss.fix();
  input.gnssWeak = !input.gpsUnavailable && !gpsQualityGoodForAnchorOn();
  input.gnssWeakReason = input.gnssWeak ? currentGnssFailReason() : nullptr;
  input.headingAvailable = headingAvailable();
  input.driftFail = runtimeMotion.driftFailActive();
  input.driftAlert = runtimeMotion.driftAlertActive();
  input.anchorActive = settings.get("AnchorEnabled") == 1;
  input.anchorDeniedReason =
      (!input.anchorActive && runtimeMotion.lastAnchorDeniedReason() != AnchorDeniedReason::NONE)
          ? currentAnchorDeniedReason()
          : nullptr;
  input.failsafeReason =
      (runtimeMotion.lastFailsafeReason() != FailsafeReason::NONE) ? currentFailsafeReason()
                                                                   : nullptr;
  if (safetyReason) {
    *safetyReason = activeSafetyReason(now);
    input.safetyReason = safetyReason->c_str();
  }
  input.holdMode = runtimeMotion.safeHoldActive();
  return input;
}

std::string buildCurrentStatusReasons(unsigned long now) {
  std::string safetyReason;
  const RuntimeStatusInput input = currentRuntimeStatusInput(now, &safetyReason);
  return buildRuntimeStatusReasons(input);
}

RuntimeStatusSnapshot buildCurrentStatusSnapshot(unsigned long now) {
  std::string safetyReason;
  const RuntimeStatusInput input = currentRuntimeStatusInput(now, &safetyReason);
  return buildRuntimeStatusSnapshot(input);
}

float normalizeDiffDeg(float targetDeg, float currentDeg) {
  return RuntimeGnss::normalizeDiffDeg(targetDeg, currentDeg);
}

float normalize360Deg(float deg) {
  return RuntimeGnss::normalize360Deg(deg);
}

float normalize180Deg(float deg) {
  return RuntimeGnss::normalize180Deg(deg);
}

CoreMode currentCoreMode();

const char* currentModeLabel() {
  return coreModeLabel(currentCoreMode());
}

CoreMode currentCoreMode() {
  return resolveCoreMode(hilSim.isRunning(),
                         manualControl.active(),
                         settings.get("AnchorEnabled") == 1,
                         runtimeMotion.safeHoldActive());
}

float currentHeadingValue() {
  return runtimeGnss.currentHeadingValue(headingAvailable(), compass.getAzimuth(), millis());
}

bool controlGpsAvailable() {
  return runtimeGnss.controlGpsAvailable();
}

void stepperTask(void*) {
  for (;;) {
    stepperControl.run();
    vTaskDelay(1);
  }
}

void registerBleParams() {
  RuntimeBleParamContext context{
      bleBoatLock,
      settings,
      runtimeGnss,
      runtimeMotion,
      anchor,
      motor,
      driftMonitor,
      compass,
      hilSim,
      handleBleCommand,
      []() { return headingAvailable(); },
      []() { return currentHeadingValue(); },
      []() { return currentModeLabel(); },
      []() { return gnssQualityLevel(); },
      []() { return buildCurrentStatusSnapshot(millis()); },
      []() { return fixTypeSourceString(fixTypeSource); },
  };
  registerRuntimeBleParams(context);
}

void startBleRuntime() {
  if (bleStarted) {
    return;
  }
  bleBoatLock.begin();
  bleStarted = true;
}

void serviceBleRuntime() {
  if (bleStarted) {
    bleBoatLock.loop();
  }
}

void updateGpsFromSerial() {
  size_t bytesRead = 0;
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
    ++bytesRead;
  }

  const unsigned long now = millis();
  RuntimeGpsUartConfig uartConfig;
  uartConfig.noDataWarnMs = cfg::kGpsNoDataWarnMs;
  uartConfig.staleRestartMs = cfg::kGpsUartStaleRestartMs;
  uartConfig.restartCooldownMs = cfg::kGpsUartRestartCooldownMs;
  const RuntimeGpsUartActions uartActions = gpsUart.update(bytesRead, now, uartConfig);

  if (uartActions.firstDataSeen) {
    logMessage("[GPS] UART data detected RX=%d TX=%d baud=9600\n",
               cfg::kGpsRxPin,
               cfg::kGpsTxPin);
  }

  if (uartActions.warnNoData) {
    logMessage("[GPS] no UART bytes on RX=%d (check wiring/baud/power)\n", cfg::kGpsRxPin);
  }

  if (uartActions.warnStale) {
    logMessage("[GPS] UART stream stale (%lu ms), restarting serial\n", uartActions.staleAgeMs);
  }

  if (uartActions.restartSerial) {
    gpsSerial.end();
    delay(20);
    gpsSerial.begin(9600, SERIAL_8N1, cfg::kGpsRxPin, cfg::kGpsTxPin);
    gps = TinyGPSPlus();
    logMessage("[GPS] UART restarted RX=%d TX=%d baud=9600\n", cfg::kGpsRxPin, cfg::kGpsTxPin);
  }

  if (settings.get("DebugGps") == 1 && millis() - lastGpsDebugMs >= 1000) {
    Serial.printf("[GPS] bytes=%lu valid=%d fix=%d sats=%d hdop=%.2f age=%lu\n",
                  gps.charsProcessed(),
                  gps.location.isValid(),
                  runtimeGnss.fix(),
                  gps.satellites.value(),
                  gps.hdop.value() * 0.01f,
                  gps.location.age());
    lastGpsDebugMs = millis();
  }

  const unsigned long maxGpsAgeMs =
      static_cast<unsigned long>(constrain(settings.get("MaxGpsAgeMs"), 300.0f, 20000.0f));
  const bool hardwareFix = gps.location.isValid() && gps.location.age() < maxGpsAgeMs;
  if (hardwareFix) {
    RuntimeGnss::HardwareFixInput fix;
    fix.lat = gps.location.lat();
    fix.lon = gps.location.lng();
    fix.speedKmh = gps.speed.kmph();
    fix.satellites = max(0, (int)gps.satellites.value());
    fix.hdop = gps.hdop.isValid() ? (gps.hdop.value() * 0.01f) : NAN;
    fix.ageMs = gps.location.age();
    const bool headingOk = headingAvailable();
    const RuntimeGnss::ApplyResult result = runtimeGnss.applyHardwareFix(
        fix, settings, headingOk, compass.getAzimuth(), now);
    if (result == RuntimeGnss::ApplyResult::JUMP_REJECTED &&
        now - lastGpsJumpRejectLogMs >= 1000UL) {
      logMessage("[EVENT] GPS_JUMP_REJECTED jump=%.2f max=%.2f\n",
                 runtimeGnss.currentPosJumpM(),
                 constrain(settings.get("MaxPosJumpM"), 1.0f, 200.0f));
      lastGpsJumpRejectLogMs = now;
    }
    return;
  }

  if (runtimeGnss.applyPhoneFallback(headingAvailable(), compass.getAzimuth(), now)) {
    return;
  }

  runtimeGnss.clearFix();
}

void handleBootButton() {
  const bool pressed = digitalRead(cfg::kBootPin) == LOW;
  const unsigned long now = millis();
  const RuntimeButtonAction action = runtimeButtons.updateBoot(
      pressed, now, cfg::kBootSaveHoldMs, controlGpsAvailable(), currentGnssDeniedReason());
  if (action.type == RuntimeButtonActionType::NONE) {
    return;
  }

  if (action.type == RuntimeButtonActionType::SAVE_ANCHOR_POINT) {
    if (!anchor.saveAnchor(
            runtimeGnss.lastLat(), runtimeGnss.lastLon(), currentHeadingValue(), false)) {
      setLastAnchorDeniedReason(AnchorDeniedReason::INTERNAL_ERROR);
      logMessage("[EVENT] ANCHOR_DENIED reason=GPS_INVALID source=BOOT_BUTTON\n");
      return;
    }
    setLastAnchorDeniedReason(AnchorDeniedReason::NONE);
    setLastFailsafeReason(FailsafeReason::NONE);
    logMessage("[EVENT] ANCHOR_POINT_SAVED source=BOOT_BUTTON\n");
    logMessage("[ANCHOR] point saved from BOOT button; anchor remains OFF\n");
    return;
  }

  if (action.type == RuntimeButtonActionType::DENY_ANCHOR_POINT) {
    setLastAnchorDeniedReason(action.deniedReason);
    logMessage("[EVENT] ANCHOR_DENIED reason=%s source=BOOT_BUTTON\n",
               currentGnssFailReason());
    logMessage("[ANCHOR] BOOT save rejected (control GNSS unavailable)\n");
  }
}

void handleStopButton() {
  const bool pressed = digitalRead(cfg::kStopButtonPin) == LOW;
  const unsigned long now = millis();
  const RuntimeButtonAction action =
      runtimeButtons.updateStop(pressed, now);
  if (action.type == RuntimeButtonActionType::EMERGENCY_STOP) {
    setLastFailsafeReason(FailsafeReason::STOP_CMD);
    stopAllMotionNow();
    logMessage("[STOP] hardware stop button pressed (GPIO%d)\n", cfg::kStopButtonPin);
    return;
  }
}

void updateDistanceAndBearing() {
  runtimeGnss.updateDistanceAndBearing(manualControl.active(),
                                       settings.get("AnchorEnabled") == 1,
                                       settings.get("HoldHeading") == 1,
                                       anchor.anchorLat,
                                       anchor.anchorLon,
                                       anchor.anchorHeading,
                                       millis());
}

void updateDriftState(unsigned long now) {
  runtimeMotion.updateDriftState(now, currentCoreMode(), runtimeGnss.fix(), runtimeGnss.distanceM());
}

void applyMotionControl(const RuntimeControlInput& input) {
  runtimeMotion.applyControl(input);
}

void publishBleAndUi(unsigned long now,
                     bool hasHeading,
                     bool hasBearing,
                     float heading,
                     float diffDeg,
                     const char* stickyBadge) {
  const char* mode = currentModeLabel();
  RuntimeUiSnapshot ui = buildRuntimeUiSnapshot(runtimeGnss.fix(),
                                                runtimeGnss.currentSatellites(),
                                                runtimeGnss.gpsSourcePhone(),
                                                runtimeGnss.currentGpsHdop(),
                                                runtimeGnss.currentSpeedKmh(),
                                                hasHeading ? heading : fallbackHeading,
                                                hasHeading,
                                                false,
                                                hasHeading ? compass.getHeadingQuality() : 0,
                                                hasBearing ? runtimeGnss.bearingDeg() : fallbackBearing,
                                                runtimeGnss.distanceM(),
                                                diffDeg,
                                                motor.pwmPercent());

  if (hilSim.isRunning()) {
    hilsim::HilScenarioRunner::LiveTelemetry simLive;
    if (hilSim.liveTelemetry(&simLive)) {
      applyHilSimUiSnapshot(&ui, simLive, fallbackHeading);
    }
  }

  if (telemetryCadence.shouldRefreshUi(now, cfg::kUiRefreshIntervalMs)) {
    display_draw_ui(ui.gpsFix,
                    ui.satellites,
                    ui.gpsFromPhone,
                    ui.gpsHdop,
                    ui.speedKmh,
                    ui.heading,
                    ui.headingValid,
                    ui.headingFromPhone,
                    ui.compassQuality,
                    ui.bearing,
                    ui.distanceM,
                    ui.diffDeg,
                    mode,
                    stickyBadge,
                    ui.motorPwmPercent);
  }

  if (telemetryCadence.shouldNotifyBle(now, cfg::kBleNotifyIntervalMs)) {
    std::string safetyReason;
    const RuntimeStatusInput statusInput = currentRuntimeStatusInput(now, &safetyReason);
    notifyRuntimeBleStatus(bleBoatLock, statusInput);
  }
}

void enqueueSdTelemetry(unsigned long now, const RuntimeControlState& controlState) {
  if (now - lastSdTelemetryMs < cfg::kSdTelemetryIntervalMs) {
    return;
  }
  lastSdTelemetryMs = now;

  const RuntimeStatusSnapshot status = buildCurrentStatusSnapshot(now);
  SdLogTelemetrySnapshot snapshot;
  snapshot.ms = now;
  snapshot.mode = currentModeLabel();
  snapshot.status = status.summary.c_str();
  snapshot.reasons = status.reasons.c_str();
  snapshot.gpsFix = runtimeGnss.fix();
  snapshot.gpsSourcePhone = runtimeGnss.gpsSourcePhone();
  snapshot.lat = runtimeGnss.lastLat();
  snapshot.lon = runtimeGnss.lastLon();
  snapshot.gpsAgeMs = runtimeGnss.currentGpsAgeMs();
  snapshot.satellites = runtimeGnss.currentSatellites();
  snapshot.hdop = runtimeGnss.currentGpsHdop();
  snapshot.speedKmh = runtimeGnss.currentSpeedKmh();
  snapshot.accelValid = runtimeGnss.currentAccelValid();
  snapshot.accelMps2 = runtimeGnss.currentAccelMps2();
  snapshot.headingValid = controlState.hasHeading;
  snapshot.headingDeg = controlState.headingDeg;
  snapshot.headingRawDeg = compassReady ? compass.getRawAzimuth() : NAN;
  snapshot.headingCorrDeg = runtimeGnss.headingCorrectionDeg(now);
  snapshot.pitchDeg = compassReady ? compass.getPitchDeg() : NAN;
  snapshot.rollDeg = compassReady ? compass.getRollDeg() : NAN;
  snapshot.rvAccuracyDeg = compassReady ? compass.getRvAccuracyDeg() : NAN;
  snapshot.compassQuality = compassReady ? compass.getHeadingQuality() : 0;
  snapshot.anchorConfigured = anchorPointConfigured();
  snapshot.anchorEnabled = settings.get("AnchorEnabled") == 1;
  snapshot.anchorLat = anchor.anchorLat;
  snapshot.anchorLon = anchor.anchorLon;
  snapshot.distanceM = runtimeGnss.distanceM();
  snapshot.bearingValid = controlState.hasBearing;
  snapshot.bearingDeg = runtimeGnss.bearingDeg();
  snapshot.diffDeg = controlState.diffDeg;
  snapshot.motorPwmPct = motor.pwmPercent();
  snapshot.stepperPosition = stepperControl.stepper.currentPosition();
  snapshot.stepperTarget = stepperControl.stepper.targetPosition();
  snapshot.bleConnected = bleBoatLock.bleStatus == BLEBoatLock::CONNECTED;
  sdCardLogger.enqueueTelemetry(snapshot);
}

} // namespace

void setPhoneGpsFix(float lat, float lon, float speedKmh, int satellites) {
  runtimeGnss.setPhoneFix(lat, lon, speedKmh, satellites, millis());
}

void captureStepperBowZero() {
  stepperControl.captureBowZero();
}

bool hasAnchorPoint() {
  return anchorPointConfigured();
}

bool canEnableAnchorNow() {
  return currentAnchorEnableDeniedReason() == AnchorDeniedReason::NONE;
}

const char* currentGnssFailReason() {
  return runtimeGnss.currentFailReasonString();
}

AnchorDeniedReason currentAnchorEnableDeniedReason() {
  return resolveRuntimeAnchorEnableDeniedReason(
      hasAnchorPoint(),
      headingAvailable(),
      gpsQualityGoodForAnchorOn() ? AnchorDeniedReason::NONE : currentGnssDeniedReason());
}

AnchorDeniedReason currentGnssDeniedReason() {
  return runtimeGnss.currentDeniedReason();
}

bool preprocessBleCommand(const std::string& incoming, std::string* effective) {
  if (!effective) {
    return false;
  }
  *effective = incoming;
  return true;
}

bool handleSimCommand(const std::string& command) {
  const RuntimeSimExecutionOutcome outcome =
      executeRuntimeSimCommand(parseRuntimeSimCommand(command), command, hilSim);
  if (!outcome.handled) {
    return false;
  }

  if (outcome.shouldStopMotion) {
    stopAllMotionNow();
  }
  if (outcome.shouldClearFailsafe) {
    setLastFailsafeReason(FailsafeReason::NONE);
  }
  if (outcome.shouldClearAnchorDenied) {
    setLastAnchorDeniedReason(AnchorDeniedReason::NONE);
  }
  if (outcome.shouldResetSimWallClock) {
    simLastWallMs = millis();
  }

  for (const std::string& line : buildRuntimeSimLogLines(outcome)) {
    logMessage("%s\n", line.c_str());
  }
  return true;
}

void setLastAnchorDeniedReason(AnchorDeniedReason reason) {
  runtimeMotion.setLastAnchorDeniedReason(reason);
}

void setLastFailsafeReason(FailsafeReason reason) {
  runtimeMotion.setLastFailsafeReason(reason);
}

void clearSafeHold() {
  runtimeMotion.clearSafeHold();
}

const char* currentAnchorDeniedReason() {
  return runtimeMotion.currentAnchorDeniedReason();
}

const char* currentFailsafeReason() {
  return runtimeMotion.currentFailsafeReason();
}

void stopAllMotionNow() {
  runtimeMotion.stopAllMotionNow(manualControl);
}

void noteControlActivityNow() {
  anchorSupervisor.noteControlActivity(millis());
}

void applySupervisorDecision(const AnchorSupervisor::Decision& decision) {
  runtimeMotion.applySupervisorDecision(decision, manualControl, millis());
}

bool setManualControlFromBle(float angleDeg, int throttlePct, unsigned long ttlMs) {
  const unsigned long now = millis();
  const bool wasActive = manualControl.active();
  if (!manualControl.apply(ManualControlSource::BLE_PHONE, angleDeg, throttlePct, ttlMs, now)) {
    return false;
  }
  if (!wasActive) {
    setLastAnchorDeniedReason(AnchorDeniedReason::NONE);
    setLastFailsafeReason(FailsafeReason::NONE);
    runtimeMotion.clearSafeHold();
    if (settings.get("AnchorEnabled") == 1) {
      settings.set("AnchorEnabled", 0);
      settings.save();
      logMessage("[EVENT] ANCHOR_OFF reason=MANUAL\n");
    }
    stepperControl.cancelMove();
  }
  return true;
}

void stopManualControlFromBle() {
  manualControl.stop();
  stepperControl.stopManual();
  stepperControl.cancelMove();
  motor.stop();
}

bool nudgeAnchorBearing(float bearingDeg) {
  if (settings.get("AnchorEnabled") != 1) {
    logMessage("[EVENT] NUDGE_DENIED reason=ANCHOR_INACTIVE\n");
    return false;
  }
  const AnchorDeniedReason enableDenyReason = currentAnchorEnableDeniedReason();
  if (enableDenyReason != AnchorDeniedReason::NONE) {
    logMessage("[EVENT] NUDGE_DENIED reason=%s\n", anchorDeniedReasonString(enableDenyReason));
    return false;
  }
  if (!isfinite(bearingDeg)) {
    logMessage("[EVENT] NUDGE_DENIED reason=RANGE\n");
    return false;
  }
  if (!runtimeAnchorNudgePointValid(anchor.anchorLat, anchor.anchorLon)) {
    logMessage("[EVENT] NUDGE_DENIED reason=NO_ANCHOR_POINT\n");
    return false;
  }

  RuntimeAnchorNudgeTarget target;
  if (!projectRuntimeAnchorNudge(anchor.anchorLat, anchor.anchorLon, bearingDeg, &target)) {
    logMessage("[EVENT] NUDGE_DENIED reason=RANGE\n");
    return false;
  }

  if (!anchor.saveAnchor(target.lat, target.lon, anchor.anchorHeading, true)) {
    logMessage("[EVENT] NUDGE_DENIED reason=RANGE\n");
    return false;
  }
  setSafetyReason("NUDGE_OK");
  logMessage("[EVENT] NUDGE_APPLIED bearing=%.1f meters=%.2f lat=%.6f lon=%.6f\n",
             normalize360Deg(bearingDeg),
             kRuntimeAnchorNudgeMeters,
             target.lat,
             target.lon);
  return true;
}

bool nudgeAnchorCardinal(const char* dir) {
  if (!dir) {
    return false;
  }
  if (!headingAvailable()) {
    logMessage("[EVENT] NUDGE_DENIED reason=NO_COMPASS\n");
    return false;
  }

  float bearing = 0.0f;
  if (!resolveRuntimeCardinalNudgeBearing(dir, currentHeadingValue(), &bearing)) {
    logMessage("[EVENT] NUDGE_DENIED reason=DIR\n");
    return false;
  }
  return nudgeAnchorBearing(bearing);
}

void setup() {
  Serial.begin(115200);
  logMessage("\n[BoatLock] ESP32 start, firmware: %s\n", cfg::kFirmwareVersion);
  bootMs = millis();

  pinMode(cfg::kBootPin, INPUT_PULLUP);
  pinMode(cfg::kStopButtonPin, INPUT_PULLUP);

  compass.attachResetPin(cfg::kCompassResetPin);
  const bool compassHardwareReset = compass.hardwareReset();
  logMessage("[COMPASS] reset pin=%d pulse=%d\n",
             cfg::kCompassResetPin,
             compassHardwareReset ? 1 : 0);

  compassReady =
      compass.begin(compassSerial, cfg::kCompassRxPin, cfg::kCompassTxPin, cfg::kCompassBaud);
  compassReadySinceMs = compassReady ? millis() : 0;
  compassHeadingEventsReadyLogged = false;
  logMessage("[COMPASS] ready=%d source=%s rx=%d tx=%d baud=%lu\n",
             (int)compassReady,
             compass.sourceName(),
             compass.rxPin(),
             compass.txPin(),
             (unsigned long)compass.baud());

  const bool displayReady = display_init();
  logMessage("[DISPLAY] ready=%d\n", (int)displayReady);
  sdCardLogger.begin(cfg::kFirmwareVersion, bootMs);
  logMessage("[SD] logger ready=%d path=%s total_mb=%llu free_mb=%llu\n",
             sdCardLogger.ready() ? 1 : 0,
             sdCardLogger.currentPath(),
             static_cast<unsigned long long>(sdCardLogger.totalBytes() / (1024ULL * 1024ULL)),
             static_cast<unsigned long long>(sdCardLogger.freeBytes() / (1024ULL * 1024ULL)));

  randomSeed(micros());
  fallbackHeading = random(0, 360);
  fallbackBearing = random(0, 360);

  gpsSerial.begin(9600, SERIAL_8N1, cfg::kGpsRxPin, cfg::kGpsTxPin);
  gpsUart.reset(bootMs);

  EEPROM.begin(EEPROM_SIZE);
  settings.load();
  if (settings.get("AnchorEnabled") == 1) {
    settings.set("AnchorEnabled", 0);
    settings.save();
    logMessage("[EVENT] ANCHOR_DISARM_ON_BOOT prev=1\n");
  }
  logMessage("[CFG] HoldHeading=%d\n", (int)settings.get("HoldHeading"));
  if (!minFixTypeIgnoredLogged && fixTypeSource == FixTypeSource::NONE) {
    minFixTypeIgnoredLogged = true;
    logMessage("[EVENT] CONFIG_FIELD_IGNORED field=min_fix_type reason=fix_type_unavailable\n");
  }

  runtimeGnss.reset((int)settings.get("GpsFWin"));

  compass.attachSettings(&settings);
  if (compassReady) {
    compass.loadCalibrationFromSettings();
    logMessage("[COMPASS] heading offset=%.1f\n", compass.getHeadingOffsetDeg());
  }

  anchor.attachSettings(&settings);
  anchor.loadAnchor();

  registerBleParams();
  startBleRuntime();

  motor.setupPWM(cfg::kMotorPwmPin, cfg::kPwmChannel, cfg::kPwmFreq, cfg::kPwmResolution);
  motor.setDirPins(cfg::kMotorDirPin1, cfg::kMotorDirPin2);

  stepperControl.attachSettings(&settings);
  stepperControl.loadFromSettings();
  logMessage("[STEP] driver=DRV8825 mode=STEP_DIR step=%d dir=%d minPulseUs=%u\n",
             cfg::kStepperStepPin,
             cfg::kStepperDirPin,
             StepperControl::DRV8825_MIN_PULSE_WIDTH_US);
  logMessage("[STOP] button pin=%d active=LOW\n", cfg::kStopButtonPin);
  simLastWallMs = millis();

  xTaskCreatePinnedToCore(stepperTask, "stepper", 4096, nullptr, 1, &stepperTaskHandle, 1);
}

void loop() {
  const unsigned long wallNowMs = millis();
  const unsigned long wallDtMs =
      (simLastWallMs == 0 || wallNowMs < simLastWallMs) ? 0 : (wallNowMs - simLastWallMs);
  simLastWallMs = wallNowMs;
  hilSim.loop(wallDtMs);
  serviceBleRuntime();
  bleOta.loop();

  const bool simRunningNow = hilSim.isRunning();
  if (!simRunningNow) {
    updateGpsFromSerial();
    handleBootButton();
  }
  handleStopButton();

  const unsigned long now = millis();
  if (!simRunningNow &&
      compassRetry.shouldRetry(compassReady, now, cfg::kCompassRetryIntervalMs)) {
    const bool resetPulse = compass.hardwareReset();
    compassReady =
        compass.begin(compassSerial, cfg::kCompassRxPin, cfg::kCompassTxPin, cfg::kCompassBaud);
    compassReadySinceMs = compassReady ? now : 0;
    compassHeadingEventsReadyLogged = false;
    logMessage("[COMPASS] retry ready=%d source=%s rx=%d tx=%d baud=%lu reset=%d\n",
               (int)compassReady,
               compass.sourceName(),
               compass.rxPin(),
               compass.txPin(),
               (unsigned long)compass.baud(),
               resetPulse ? 1 : 0);
    if (compassReady) {
      compass.loadCalibrationFromSettings();
      logMessage("[COMPASS] heading offset=%.1f\n", compass.getHeadingOffsetDeg());
    }
  }

  if (!simRunningNow && compassReady &&
      telemetryCadence.shouldPollCompass(now, cfg::kCompassReadIntervalMs)) {
    const BNO08xCompassReadResult compassRead = compass.read();
    if (compassRead.headingEvent && !compassHeadingEventsReadyLogged) {
      compassHeadingEventsReadyLogged = true;
      logMessage("[COMPASS] heading events ready age=%lu\n", compass.lastHeadingEventAgeMs(millis()));
    }
  }

  if (!simRunningNow && compassReady) {
    const unsigned long compassHealthNow = millis();
    RuntimeCompassHealthInput compassHealth;
    compassHealth.compassReady = compassReady;
    compassHealth.nowMs = compassHealthNow;
    compassHealth.readySinceMs = compassReadySinceMs;
    compassHealth.lastHeadingEventAgeMs = compass.lastHeadingEventAgeMs(compassHealthNow);
    compassHealth.firstEventTimeoutMs = cfg::kCompassFirstEventTimeoutMs;
    compassHealth.staleEventMs = cfg::kCompassStaleEventMs;
    const RuntimeCompassLossReason lossReason = runtimeCompassLossReason(compassHealth);
    if (lossReason != RuntimeCompassLossReason::NONE) {
      logMessage("[COMPASS] lost reason=%s age=%lu\n",
                 runtimeCompassLossReasonString(lossReason),
                 compassHealth.lastHeadingEventAgeMs);
      compassReady = false;
      compassReadySinceMs = 0;
      compassHeadingEventsReadyLogged = false;
    }
  }

  manualControl.update(now);
  updateDistanceAndBearing();
  const unsigned long loopDtMs = (lastLoopMs == 0 || now < lastLoopMs) ? 0 : (now - lastLoopMs);
  lastLoopMs = now;
  updateDriftState(now);

  const CoreMode mode = currentCoreMode();
  const bool anchorActive = coreModeUsesAnchorControl(mode);
  const bool gpsQualityOk = gpsQualityGoodForAnchorOn();
  const bool bleConnected = (bleBoatLock.bleStatus == BLEBoatLock::CONNECTED);
  const AnchorSupervisor::Config supCfg = buildRuntimeSupervisorConfig(settings);
  const AnchorSupervisor::Input supIn = buildRuntimeSupervisorInput(
      now,
      anchorActive,
      gpsQualityOk,
      bleConnected,
      controlGpsAvailable() && headingAvailable(),
      (!isfinite(runtimeGnss.distanceM()) || !isfinite(runtimeGnss.bearingDeg())),
      runtimeMotion.driftFailActive(),
      loopDtMs,
      motor.pwmPercent());
  applySupervisorDecision(anchorSupervisor.update(supCfg, supIn));

  const CoreMode controlMode = currentCoreMode();
  const float heading = currentHeadingValue();
  const bool anchorBearingAvailable =
      runtimeGnss.anchorBearingAvailable(coreModeUsesAnchorControl(controlMode),
                                         settings.get("HoldHeading") == 1,
                                         now);
  const bool headingOk = headingAvailable();
  const RuntimeControlState controlState = buildRuntimeControlState(
      now,
      controlMode,
      manualControl.angleDeg(),
      manualControl.motorPwm(),
      controlGpsAvailable(),
      headingOk,
      heading,
      anchorBearingAvailable,
      runtimeGnss.bearingDeg(),
      headingOk,
      headingOk ? compass.getRvAccuracyDeg() : NAN,
      headingOk ? compass.getHeadingQuality() : 0,
      runtimeGnss.distanceM());

  const char* stickyBadge = runtimeSimBadge.update(
      simRunningNow, hilSim.currentScenarioId(), hilSim.lastPass(), now, cfg::kSimResultBannerMs);

  applyMotionControl(controlState.input);
  const AnchorDiagnostics::Events diag = anchorDiagnostics.update(
      now,
      controlState.autoControlActive,
      runtimeGnss.fix(),
      controlState.hasHeading,
      runtimeMotion.driftAlertActive(),
      motor.pwmPercent(),
      MotorControl::AUTO_MAX_PWM_PCT);
  if (diag.driftAlert) {
    logMessage("[EVENT] DRIFT_ALERT dist=%.2f\n", runtimeGnss.distanceM());
  }
  if (diag.controlSaturated) {
    logMessage("[EVENT] CONTROL_SATURATED pwm=%d\n", motor.pwmPercent());
  }
  if (diag.sensorTimeout) {
    logMessage("[EVENT] SENSOR_TIMEOUT\n");
  }
  publishBleAndUi(
      now, controlState.hasHeading, controlState.hasBearing, controlState.headingDeg, controlState.diffDeg, stickyBadge);
  enqueueSdTelemetry(now, controlState);
  sdCardLogger.service(now);

  serviceBleRuntime();
}
