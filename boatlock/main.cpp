#include <Arduino.h>
#include <Wire.h>
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
#include "AnchorProfiles.h"
#include "AnchorReasons.h"
#include "RuntimeButtons.h"
#include "RuntimeBleParams.h"
#include "RuntimeAnchorNudge.h"
#include "RuntimeAnchorGate.h"
#include "RuntimeCompassRetry.h"
#include "RuntimeCompassRecovery.h"
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
#include "BleSecurity.h"
#include "HilSimRunner.h"
#include "BleCommandHandler.h"
#include "BLEBoatLock.h"

Settings settings;
constexpr size_t EEPROM_SIZE =
    (Settings::EEPROM_ADDR + Settings::STORED_BYTES >
     BleSecurity::EEPROM_ADDR + BleSecurity::STORED_BYTES)
        ? (Settings::EEPROM_ADDR + Settings::STORED_BYTES)
        : (BleSecurity::EEPROM_ADDR + BleSecurity::STORED_BYTES);

BLEBoatLock bleBoatLock;

namespace cfg {
constexpr char kFirmwareVersion[] = "0.2.0";

#if defined(BOATLOCK_BOARD_JC4832W535)
constexpr int kStepIn1Pin = 11;
constexpr int kStepIn2Pin = 12;
constexpr int kStepIn3Pin = 13;
constexpr int kStepIn4Pin = 14;
// JC4832W535 display uses GPIO47/48 for LCD QSPI, so I2C must be moved.
constexpr int kI2cSdaPin = 4;
constexpr int kI2cSclPin = 8;
#else
constexpr int kStepIn1Pin = 2;
constexpr int kStepIn2Pin = 4;
constexpr int kStepIn3Pin = 6;
constexpr int kStepIn4Pin = 16;
constexpr int kI2cSdaPin = 47;
constexpr int kI2cSclPin = 48;
#endif
constexpr int kGpsRxPin = 17;
constexpr int kGpsTxPin = 18;
constexpr int kMotorPwmPin = 7;
constexpr int kMotorDirPin1 = 5;
constexpr int kMotorDirPin2 = 10;
constexpr int kBootPin = 0;
constexpr int kStopButtonPin = 15;

constexpr int kPwmFreq = 5000;
constexpr int kPwmResolution = 8;
constexpr int kPwmChannel = 0;

constexpr int kMaxGpsFilterWindow = 20;
constexpr unsigned long kBleNotifyIntervalMs = 1000;
constexpr unsigned long kUiRefreshIntervalMs = 120;
constexpr unsigned long kBootSaveHoldMs = 1500;
constexpr unsigned long kStopPairingHoldMs = 3000;
constexpr unsigned long kCompassRetryIntervalMs = 5000;
constexpr unsigned long kGpsNoDataWarnMs = 6000;
constexpr unsigned long kGpsUartStaleRestartMs = 6000;
constexpr unsigned long kGpsUartRestartCooldownMs = 4000;
constexpr unsigned long kSimResultBannerMs = 15000;
} // namespace cfg

HardwareSerial gpsSerial(1);
TinyGPSPlus gps;
StepperControl stepperControl(cfg::kStepIn1Pin,
                              cfg::kStepIn2Pin,
                              cfg::kStepIn3Pin,
                              cfg::kStepIn4Pin);

BNO08xCompass compass;
AnchorControl anchor;
MotorControl motor;
DriftMonitor driftMonitor;
AnchorSupervisor anchorSupervisor;
AnchorDiagnostics anchorDiagnostics;

bool compassReady = false;
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
BleSecurity bleSecurity;

bool manualMode = false;
int manualDir = -1;
int manualSpeed = 0;
hilsim::HilSimManager hilSim;
unsigned long simLastWallMs = 0;
RuntimeGnss runtimeGnss;
RuntimeMotion runtimeMotion(settings, stepperControl, motor, driftMonitor);

void setPhoneGpsFix(float lat, float lon, float speedKmh, int satellites);
void captureStepperBowZero();
bool canEnableAnchorNow();
bool hasAnchorPoint();
void stopAllMotionNow();
void noteControlActivityNow();
bool nudgeAnchorCardinal(const char* dir, float meters);
bool nudgeAnchorBearing(float bearingDeg, float meters);
const char* currentGnssFailReason();
AnchorDeniedReason currentAnchorEnableDeniedReason();
void applySupervisorDecision(const AnchorSupervisor::Decision& decision);
void setLastAnchorDeniedReason(AnchorDeniedReason reason);
void setLastFailsafeReason(FailsafeReason reason);
void clearSafeHold();
const char* currentAnchorDeniedReason();
const char* currentFailsafeReason();
AnchorDeniedReason currentGnssDeniedReason();
bool preprocessSecureCommand(const std::string& incoming, std::string* effective);
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
  return RuntimeGnss::anchorPointConfigured(anchor.anchorLat, anchor.anchorLon);
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
  input.gpsUnavailable = !gps.location.isValid() && !runtimeGnss.fix();
  input.gnssWeak = !input.gpsUnavailable && !gpsQualityGoodForAnchorOn();
  input.gnssWeakReason = input.gnssWeak ? currentGnssFailReason() : nullptr;
  input.headingAvailable = compassReady;
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

void logI2cInventory() {
  uint8_t found[32] = {0};
  size_t count = 0;

  for (uint8_t addr = 1; addr < 127 && count < sizeof(found); ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      found[count++] = addr;
    }
  }

  logMessage("[I2C] SDA=%d SCL=%d devices=%u",
             cfg::kI2cSdaPin,
             cfg::kI2cSclPin,
             static_cast<unsigned>(count));
  if (count == 0) {
    logMessage(": none\n");
    return;
  }

  logMessage(":");
  for (size_t i = 0; i < count; ++i) {
    const uint8_t addr = found[i];
    logMessage(" 0x%02X", addr);
    if (addr == 0x4A || addr == 0x4B) {
      logMessage("(BNO08x)");
    } else if (addr == 0x6B || addr == 0x7E) {
      logMessage("(imu)");
    } else if (addr == 0x41) {
      logMessage("(display/other)");
    }
  }
  logMessage("\n");
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
                         manualMode,
                         settings.get("AnchorEnabled") == 1,
                         runtimeMotion.safeHoldActive());
}

float currentHeadingValue() {
  return runtimeGnss.currentHeadingValue(compassReady, compass.getAzimuth(), millis());
}

bool headingAvailable() {
  return compassReady;
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
      bleSecurity,
      handleBleCommand,
      []() { return compassReady; },
      []() { return currentHeadingValue(); },
      []() { return currentModeLabel(); },
      []() { return gnssQualityLevel(); },
      []() { return buildCurrentStatusReasons(millis()); },
      []() { return fixTypeSourceString(fixTypeSource); },
  };
  registerRuntimeBleParams(context);
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
    const RuntimeGnss::ApplyResult result = runtimeGnss.applyHardwareFix(
        fix, settings, compassReady, compass.getAzimuth(), now);
    if (result == RuntimeGnss::ApplyResult::JUMP_REJECTED &&
        now - lastGpsJumpRejectLogMs >= 1000UL) {
      logMessage("[EVENT] GPS_JUMP_REJECTED jump=%.2f max=%.2f\n",
                 runtimeGnss.currentPosJumpM(),
                 constrain(settings.get("MaxPosJumpM"), 1.0f, 200.0f));
      lastGpsJumpRejectLogMs = now;
    }
    return;
  }

  if (runtimeGnss.applyPhoneFallback(compassReady, compass.getAzimuth(), now)) {
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
    anchor.saveAnchor(
        runtimeGnss.lastLat(), runtimeGnss.lastLon(), currentHeadingValue(), false);
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
      runtimeButtons.updateStop(pressed, now, cfg::kStopPairingHoldMs);
  if (action.type == RuntimeButtonActionType::EMERGENCY_STOP) {
    setLastFailsafeReason(FailsafeReason::STOP_CMD);
    stopAllMotionNow();
    logMessage("[STOP] hardware stop button pressed (GPIO%d)\n", cfg::kStopButtonPin);
    return;
  }
  if (action.type == RuntimeButtonActionType::OPEN_PAIRING_WINDOW) {
    bleSecurity.openPairingWindow(now);
    logMessage("[EVENT] PAIRING_WINDOW_OPEN source=STOP_BUTTON timeout_ms=%lu\n",
               (unsigned long)BleSecurity::kPairingWindowMs);
  }
}

void updateDistanceAndBearing() {
  runtimeGnss.updateDistanceAndBearing(manualMode,
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
                                                compassReady ? compass.getHeadingQuality() : 0,
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

bool preprocessSecureCommand(const std::string& incoming, std::string* effective) {
  if (!effective) {
    return false;
  }
  *effective = incoming;
  const unsigned long now = millis();

  if (incoming == "AUTH_HELLO") {
    const uint64_t nonce = bleSecurity.startAuth(now);
    if (nonce == 0) {
      logMessage("[EVENT] AUTH_DENIED reason=%s\n", bleSecurity.lastRejectString());
    } else {
      logMessage("[EVENT] AUTH_CHALLENGE issued=1\n");
    }
    return false;
  }

  if (incoming.rfind("AUTH_PROVE:", 0) == 0) {
    const char* proof = incoming.c_str() + 11;
    if (bleSecurity.proveAuthHex(proof)) {
      logMessage("[EVENT] AUTH_OK role=OWNER\n");
    } else {
      logMessage("[EVENT] AUTH_DENIED reason=%s\n", bleSecurity.lastRejectString());
    }
    return false;
  }

  if (incoming.rfind("PAIR_SET:", 0) == 0) {
    const char* secretHex = incoming.c_str() + 9;
    if (bleSecurity.setOwnerSecretHex(secretHex, now)) {
      logMessage("[EVENT] PAIRING_COMPLETED secret=SET\n");
    } else {
      logMessage("[EVENT] PAIRING_DENIED reason=%s\n", bleSecurity.lastRejectString());
    }
    return false;
  }

  if (incoming == "PAIR_CLEAR") {
    if (bleSecurity.pairingWindowOpen(now) || !bleSecurity.isPaired()) {
      bleSecurity.clearPairing();
      logMessage("[EVENT] PAIRING_CLEARED source=PAIR_WINDOW\n");
    } else {
      logMessage("[EVENT] AUTH_REQUIRED command=PAIR_CLEAR\n");
    }
    return false;
  }

  if (incoming.rfind("SEC_CMD:", 0) == 0) {
    std::string payload;
    if (!bleSecurity.unwrapSecureCommand(incoming, &payload, now)) {
      logMessage("[EVENT] AUTH_REJECT reason=%s\n", bleSecurity.lastRejectString());
      return false;
    }
    if (payload == "PAIR_CLEAR") {
      bleSecurity.clearPairing();
      logMessage("[EVENT] PAIRING_CLEARED source=OWNER_SESSION\n");
      return false;
    }
    *effective = payload;
    return true;
  }

  if (bleSecurity.commandNeedsAuth(incoming)) {
    logMessage("[EVENT] AUTH_REQUIRED command=%s\n", incoming.c_str());
    return false;
  }

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
  runtimeMotion.stopAllMotionNow(manualMode, manualDir, manualSpeed);
}

void noteControlActivityNow() {
  anchorSupervisor.noteControlActivity(millis());
}

void applySupervisorDecision(const AnchorSupervisor::Decision& decision) {
  runtimeMotion.applySupervisorDecision(decision, manualMode, manualDir, manualSpeed, millis());
}

bool nudgeAnchorBearing(float bearingDeg, float meters) {
  if (settings.get("AnchorEnabled") != 1) {
    logMessage("[EVENT] NUDGE_DENIED reason=ANCHOR_INACTIVE\n");
    return false;
  }
  const AnchorDeniedReason enableDenyReason = currentAnchorEnableDeniedReason();
  if (enableDenyReason != AnchorDeniedReason::NONE) {
    logMessage("[EVENT] NUDGE_DENIED reason=%s\n", anchorDeniedReasonString(enableDenyReason));
    return false;
  }
  if (!runtimeAnchorNudgeRangeValid(meters) || !isfinite(bearingDeg)) {
    logMessage("[EVENT] NUDGE_DENIED reason=RANGE\n");
    return false;
  }
  if (!isfinite(anchor.anchorLat) || !isfinite(anchor.anchorLon)) {
    logMessage("[EVENT] NUDGE_DENIED reason=NO_ANCHOR_POINT\n");
    return false;
  }

  RuntimeAnchorNudgeTarget target;
  if (!projectRuntimeAnchorNudge(anchor.anchorLat, anchor.anchorLon, bearingDeg, meters, &target)) {
    logMessage("[EVENT] NUDGE_DENIED reason=RANGE\n");
    return false;
  }

  anchor.saveAnchor(target.lat, target.lon, anchor.anchorHeading, true);
  setSafetyReason("NUDGE_OK");
  logMessage("[EVENT] NUDGE_APPLIED bearing=%.1f meters=%.2f lat=%.6f lon=%.6f\n",
             normalize360Deg(bearingDeg),
             meters,
             target.lat,
             target.lon);
  return true;
}

bool nudgeAnchorCardinal(const char* dir, float meters) {
  if (!dir) {
    return false;
  }
  if (!compassReady) {
    logMessage("[EVENT] NUDGE_DENIED reason=NO_COMPASS\n");
    return false;
  }

  float bearing = 0.0f;
  if (!resolveRuntimeCardinalNudgeBearing(dir, currentHeadingValue(), &bearing)) {
    logMessage("[EVENT] NUDGE_DENIED reason=DIR\n");
    return false;
  }
  return nudgeAnchorBearing(bearing, meters);
}

void setup() {
  Serial.begin(115200);
  logMessage("\n[BoatLock] ESP32 start, firmware: %s\n", cfg::kFirmwareVersion);
  bootMs = millis();

  pinMode(cfg::kBootPin, INPUT_PULLUP);
  pinMode(cfg::kStopButtonPin, INPUT_PULLUP);

  Wire.begin(cfg::kI2cSdaPin, cfg::kI2cSclPin);
  Wire.setTimeOut(20);
  delay(250);
  logI2cInventory();
  compassReady = compass.init();
  logMessage("[COMPASS] ready=%d source=%s", (int)compassReady, compass.sourceName());
  if (compassReady) {
    logMessage(" addr=0x%02X", compass.bnoI2cAddress());
  }
  logMessage("\n");

  const bool displayReady = display_init();
  logMessage("[DISPLAY] ready=%d\n", (int)displayReady);

  randomSeed(micros());
  fallbackHeading = random(0, 360);
  fallbackBearing = random(0, 360);

  gpsSerial.begin(9600, SERIAL_8N1, cfg::kGpsRxPin, cfg::kGpsTxPin);
  gpsUart.reset(bootMs);

  EEPROM.begin(EEPROM_SIZE);
  settings.load();
  bleSecurity.load();
  if (settings.get("AnchorEnabled") == 1) {
    settings.set("AnchorEnabled", 0);
    settings.save();
    logMessage("[EVENT] ANCHOR_DISARM_ON_BOOT prev=1\n");
  }
  logMessage("[SEC] paired=%d\n", bleSecurity.isPaired() ? 1 : 0);
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
  bleBoatLock.begin();

  motor.setupPWM(cfg::kMotorPwmPin, cfg::kPwmChannel, cfg::kPwmFreq, cfg::kPwmResolution);
  motor.setDirPins(cfg::kMotorDirPin1, cfg::kMotorDirPin2);
  motor.loadPIDfromSettings();

  stepperControl.attachSettings(&settings);
  stepperControl.loadFromSettings();
  logMessage("[STEP] pins IN1=%d IN2=%d IN3=%d IN4=%d\n",
             cfg::kStepIn1Pin,
             cfg::kStepIn2Pin,
             cfg::kStepIn3Pin,
             cfg::kStepIn4Pin);
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
  bleBoatLock.loop();

  updateGpsFromSerial();
  handleBootButton();
  handleStopButton();

  const RuntimeCompassRecoveryOutcome compassRecovery = attemptRuntimeCompassRecovery(
      compassRetry.shouldRetry(compassReady, millis(), cfg::kCompassRetryIntervalMs),
      []() {
        Wire.begin(cfg::kI2cSdaPin, cfg::kI2cSclPin);
        Wire.setTimeOut(20);
        delay(120);
      },
      [&]() { return compass.init(); });
  if (compassRecovery.attempted) {
    compassReady = compassRecovery.ready;
    logMessage("%s\n",
               buildRuntimeCompassRecoveryStatusLine(
                   compassReady, compass.sourceName(), compass.bnoI2cAddress())
                   .c_str());
    if (compassRecovery.shouldReloadCalibration) {
      compass.loadCalibrationFromSettings();
      logMessage("%s\n", buildRuntimeCompassOffsetLine(compass.getHeadingOffsetDeg()).c_str());
    } else if (compassRecovery.shouldLogInventory) {
      logI2cInventory();
    }
  }

  if (compassReady) {
    compass.read();
  }

  updateDistanceAndBearing();
  const unsigned long now = millis();
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
  const RuntimeControlState controlState = buildRuntimeControlState(
      now,
      controlMode,
      manualDir,
      manualSpeed,
      controlGpsAvailable(),
      headingAvailable(),
      heading,
      anchorBearingAvailable,
      runtimeGnss.bearingDeg(),
      compassReady,
      compassReady ? compass.getRvAccuracyDeg() : NAN,
      compassReady ? compass.getHeadingQuality() : 0,
      runtimeGnss.distanceM());

  const bool simRunningNow = hilSim.isRunning();
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

  bleBoatLock.loop();
}
