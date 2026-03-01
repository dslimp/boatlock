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

#include "ParamHelpers.h"
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
constexpr char kFirmwareVersion[] = "0.1.0";

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
constexpr unsigned long kStepperCheckIntervalMs = 250;
constexpr unsigned long kPhoneGpsTimeoutMs = 5000;
constexpr unsigned long kCompassRetryIntervalMs = 5000;
constexpr float kGpsCorrMinSpeedKmh = 3.0f;
constexpr float kGpsCorrMinMoveM = 4.0f;
constexpr float kGpsCorrAlpha = 0.18f;
constexpr float kGpsCorrMaxAbsDeg = 90.0f;
constexpr unsigned long kGpsCorrMaxAgeMs = 180000;
constexpr unsigned long kAnchorBearingCacheMaxAgeMs = 120000;
constexpr float kStepperDeadbandBaseDeg = 2.2f;
constexpr float kStepperDeadbandMaxDeg = 9.0f;
constexpr float kStepperDeadbandHystDeg = 0.9f;
constexpr float kMotorHeadingHystDeg = 2.0f;
constexpr unsigned long kSafetyReasonHoldMs = 15000;
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
float dist = 0.0f;
float bearing = 0.0f;
float lastLat = 0.0f;
float lastLon = 0.0f;
bool gpsFix = false;
enum class FixTypeSource : uint8_t {
  NONE = 0,
  NMEA_GSA = 1,
  UBX = 2,
};
FixTypeSource fixTypeSource = FixTypeSource::NONE;
float currentSpeedKmh = 0.0f;
int currentSatellites = 0;
float currentGpsHdop = NAN;
float phoneLat = 0.0f;
float phoneLon = 0.0f;
float phoneSpeedKmh = 0.0f;
int phoneSatellites = 0;
unsigned long phoneGpsUpdatedMs = 0;
bool phoneGpsValid = false;
bool gpsSourcePhone = false;
float gpsHeadingCorrDeg = 0.0f;
unsigned long gpsHeadingCorrUpdatedMs = 0;
bool gpsHeadingCorrValid = false;
float gpsCorrRefLat = 0.0f;
float gpsCorrRefLon = 0.0f;
bool gpsCorrRefValid = false;
float anchorBearingCacheDeg = 0.0f;
unsigned long anchorBearingCacheUpdatedMs = 0;
bool anchorBearingCacheValid = false;
bool driftAlertActive = false;
bool driftFailActive = false;
std::string safetyReason;
unsigned long safetyReasonMs = 0;
unsigned long currentGpsAgeMs = 999999;
float currentPosJumpM = 0.0f;
bool currentPosJumpRejected = false;
float currentSpeedMps = 0.0f;
float currentAccelMps2 = 0.0f;
bool currentAccelValid = false;
unsigned long speedSampleMs = 0;
float prevSpeedMps = NAN;
unsigned long hwFixSamplesCount = 0;
GnssQualityFailReason lastGnssFailReason = GnssQualityFailReason::NO_FIX;
bool minFixTypeIgnoredLogged = false;
unsigned long lastLoopMs = 0;
AnchorDeniedReason lastAnchorDeniedReason = AnchorDeniedReason::NONE;
FailsafeReason lastFailsafeReason = FailsafeReason::NONE;
BleSecurity bleSecurity;

bool manualMode = false;
int manualDir = -1;
int manualSpeed = 0;
hilsim::HilSimManager hilSim;
unsigned long simLastWallMs = 0;

void setPhoneGpsFix(float lat, float lon, float speedKmh, int satellites);
void captureStepperBowZero();
bool canEnableAnchorNow();
bool hasAnchorPoint();
void stopAllMotionNow();
void noteControlActivityNow();
bool nudgeAnchorCardinal(const char* dir, float meters);
bool nudgeAnchorBearing(float bearingDeg, float meters);
const char* currentGnssFailReason();
void applySupervisorDecision(const AnchorSupervisor::Decision& decision);
void setLastAnchorDeniedReason(AnchorDeniedReason reason);
void setLastFailsafeReason(FailsafeReason reason);
const char* currentAnchorDeniedReason();
const char* currentFailsafeReason();
AnchorDeniedReason currentGnssDeniedReason();
bool preprocessSecureCommand(const std::string& incoming, std::string* effective);
bool handleSimCommand(const std::string& command);

namespace {
TaskHandle_t stepperTaskHandle = nullptr;

unsigned long lastGpsDebugMs = 0;
unsigned long lastBleNotifyMs = 0;
unsigned long lastUiRefreshMs = 0;
unsigned long lastStepperCheckMs = 0;
unsigned long lastCompassRetryMs = 0;
bool stepperTrackingActive = false;
bool motorHeadingAligned = false;
bool lastBootButton = HIGH;
bool lastStopButton = HIGH;
unsigned long stopButtonDownSinceMs = 0;
bool stopPairingActionDone = false;
bool gpsUartSeen = false;
bool gpsNoDataWarned = false;
unsigned long bootMs = 0;

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
  return isfinite(anchor.anchorLat) &&
         isfinite(anchor.anchorLon) &&
         !(anchor.anchorLat == 0.0f && anchor.anchorLon == 0.0f);
}

bool gpsQualityGoodForAnchorOn() {
  GnssQualityConfig cfg;
  cfg.maxGpsAgeMs =
      static_cast<unsigned long>(constrain(settings.get("MaxGpsAgeMs"), 300.0f, 20000.0f));
  cfg.minSats = constrain((int)settings.get("MinSats"), 3, 20);
  cfg.maxHdop = constrain(settings.get("MaxHdop"), 0.5f, 10.0f);
  cfg.maxPositionJumpM = constrain(settings.get("MaxPosJumpM"), 1.0f, 200.0f);
  cfg.enableSpeedAccelSanity = ((int)settings.get("SpdSanity") == 1);
  cfg.maxSpeedMps = constrain(settings.get("MaxSpdMps"), 1.0f, 60.0f);
  cfg.maxAccelMps2 = constrain(settings.get("MaxAccMps2"), 0.5f, 20.0f);
  cfg.requiredSentences = constrain((int)settings.get("ReqSent"), 0, 20);

  GnssQualitySample s;
  s.fix = gpsFix;
  s.ageMs = currentGpsAgeMs;
  s.sats = currentSatellites;
  s.hasHdop = !gpsSourcePhone && isfinite(currentGpsHdop) && currentGpsHdop > 0.0f;
  s.hdop = currentGpsHdop;
  s.jumpRejected = currentPosJumpRejected;
  s.jumpM = currentPosJumpM;
  s.hasSpeed = isfinite(currentSpeedMps);
  s.speedMps = currentSpeedMps;
  s.hasAccel = currentAccelValid && isfinite(currentAccelMps2);
  s.accelMps2 = currentAccelMps2;
  s.hasSentences = !gpsSourcePhone;
  s.sentences = (int)hwFixSamplesCount;

  lastGnssFailReason = evaluateGnssQuality(cfg, s);
  return lastGnssFailReason == GnssQualityFailReason::NONE;
}

int gnssQualityLevel() {
  if (!gpsFix) {
    return 0;
  }
  if (lastGnssFailReason == GnssQualityFailReason::NONE || gpsQualityGoodForAnchorOn()) {
    return 2;
  }
  return 1;
}

void setSafetyReason(const char* reason) {
  safetyReason = reason ? reason : "";
  safetyReasonMs = millis();
}

std::string activeSafetyReason(unsigned long now) {
  if (safetyReason.empty()) {
    return "";
  }
  if (now - safetyReasonMs > cfg::kSafetyReasonHoldMs) {
    return "";
  }
  return safetyReason;
}

struct GpsFilterState {
  float latBuf[cfg::kMaxGpsFilterWindow] = {0};
  float lonBuf[cfg::kMaxGpsFilterWindow] = {0};
  int count = 0;
  int index = 0;
  int window = 5;
  float latSum = 0.0f;
  float lonSum = 0.0f;

  void reset(int newWindow) {
    count = 0;
    index = 0;
    latSum = 0.0f;
    lonSum = 0.0f;
    window = constrain(newWindow, 1, cfg::kMaxGpsFilterWindow);
  }

  void push(float lat, float lon) {
    if (count < window) {
      latBuf[index] = lat;
      lonBuf[index] = lon;
      latSum += lat;
      lonSum += lon;
      ++count;
    } else {
      latSum -= latBuf[index];
      lonSum -= lonBuf[index];
      latBuf[index] = lat;
      lonBuf[index] = lon;
      latSum += lat;
      lonSum += lon;
    }
    index = (index + 1) % window;
  }

  float avgLat() const {
    return (count > 0) ? (latSum / count) : 0.0f;
  }

  float avgLon() const {
    return (count > 0) ? (lonSum / count) : 0.0f;
  }
};

GpsFilterState gpsFilter;

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
  float diff = targetDeg - currentDeg;
  if (diff > 180.0f) diff -= 360.0f;
  if (diff < -180.0f) diff += 360.0f;
  return diff;
}

float normalize360Deg(float deg) {
  while (deg < 0.0f) deg += 360.0f;
  while (deg >= 360.0f) deg -= 360.0f;
  return deg;
}

float normalize180Deg(float deg) {
  while (deg > 180.0f) deg -= 360.0f;
  while (deg < -180.0f) deg += 360.0f;
  return deg;
}

void maybeUpdateGpsHeadingCorrection(float lat, float lon, float speedKmh) {
  if (!compassReady) {
    gpsCorrRefValid = false;
    return;
  }
  if (!isfinite(lat) || !isfinite(lon) || !isfinite(speedKmh) ||
      speedKmh < cfg::kGpsCorrMinSpeedKmh) {
    return;
  }

  if (!gpsCorrRefValid) {
    gpsCorrRefLat = lat;
    gpsCorrRefLon = lon;
    gpsCorrRefValid = true;
    return;
  }

  const float movedM =
      TinyGPSPlus::distanceBetween(gpsCorrRefLat, gpsCorrRefLon, lat, lon);
  if (!isfinite(movedM) || movedM < cfg::kGpsCorrMinMoveM) {
    return;
  }

  const float gpsCourse =
      TinyGPSPlus::courseTo(gpsCorrRefLat, gpsCorrRefLon, lat, lon);
  gpsCorrRefLat = lat;
  gpsCorrRefLon = lon;

  const float compassHeading = compass.getAzimuth();
  float targetCorr = normalize180Deg(gpsCourse - compassHeading);
  targetCorr = constrain(targetCorr, -cfg::kGpsCorrMaxAbsDeg, cfg::kGpsCorrMaxAbsDeg);

  if (!gpsHeadingCorrValid) {
    gpsHeadingCorrDeg = targetCorr;
  } else {
    const float delta = normalize180Deg(targetCorr - gpsHeadingCorrDeg);
    gpsHeadingCorrDeg = normalize180Deg(gpsHeadingCorrDeg + delta * cfg::kGpsCorrAlpha);
  }

  gpsHeadingCorrDeg =
      constrain(gpsHeadingCorrDeg, -cfg::kGpsCorrMaxAbsDeg, cfg::kGpsCorrMaxAbsDeg);
  gpsHeadingCorrUpdatedMs = millis();
  gpsHeadingCorrValid = true;
}

float correctedCompassHeading(float headingDeg) {
  if (!gpsHeadingCorrValid) {
    return normalize360Deg(headingDeg);
  }
  if (millis() - gpsHeadingCorrUpdatedMs > cfg::kGpsCorrMaxAgeMs) {
    return normalize360Deg(headingDeg);
  }
  return normalize360Deg(headingDeg + gpsHeadingCorrDeg);
}

const char* currentModeLabel() {
  if (manualMode) return "MANUAL";
  if (settings.get("AnchorEnabled") == 1) return "ANCHOR";
  return "IDLE";
}

float currentHeadingValue() {
  if (compassReady) {
    return correctedCompassHeading(compass.getAzimuth());
  }
  return 0.0f;
}

bool headingAvailable() {
  return compassReady;
}

float currentStepperDeadbandDeg() {
  float deadband = cfg::kStepperDeadbandBaseDeg;
  if (compassReady) {
    const float rvAcc = compass.getRvAccuracyDeg();
    if (isfinite(rvAcc) && rvAcc > 0.0f) {
      deadband = max(deadband, 1.2f + rvAcc * 1.3f);
    }
    const int rvQ = compass.getHeadingQuality();
    if (rvQ <= 1) {
      deadband += 0.8f;
    }
    if (rvQ == 0) {
      deadband += 1.0f;
    }
  }

  return constrain(deadband, cfg::kStepperDeadbandBaseDeg, cfg::kStepperDeadbandMaxDeg);
}

void stepperTask(void*) {
  for (;;) {
    stepperControl.run();
    vTaskDelay(1);
  }
}

void registerBleParams() {
  bleBoatLock.setCommandHandler(handleBleCommand);

  bleBoatLock.registerParam("lat", makeFloatParam([&]() {
    return gpsFix ? lastLat : 0.0f;
  }, "%.6f"));

  bleBoatLock.registerParam("lon", makeFloatParam([&]() {
    return gpsFix ? lastLon : 0.0f;
  }, "%.6f"));

  bleBoatLock.registerParam("heading", makeFloatParam([&]() {
    return currentHeadingValue();
  }, "%.1f"));

  bleBoatLock.registerParam("headingRaw", makeFloatParam([&]() {
    return compassReady ? compass.getRawAzimuth() : 0.0f;
  }, "%.1f"));

  bleBoatLock.registerParam("compassOffset", makeFloatParam([&]() {
    return compassReady ? compass.getHeadingOffsetDeg() : settings.get("MagOffX");
  }, "%.1f"));

  bleBoatLock.registerParam("gpsHdgCorr", makeFloatParam([&]() {
    if (!gpsHeadingCorrValid) {
      return 0.0f;
    }
    if (millis() - gpsHeadingCorrUpdatedMs > cfg::kGpsCorrMaxAgeMs) {
      return 0.0f;
    }
    return gpsHeadingCorrDeg;
  }, "%.1f"));

  bleBoatLock.registerParam("compassQ", makeFloatParam([&]() {
    return compassReady ? (float)compass.getHeadingQuality() : 0.0f;
  }, "%.0f"));

  bleBoatLock.registerParam("rvAcc", makeFloatParam([&]() {
    return compassReady ? compass.getRvAccuracyDeg() : 0.0f;
  }, "%.2f"));

  bleBoatLock.registerParam("magNorm", makeFloatParam([&]() {
    return compassReady ? compass.getMagNormUT() : 0.0f;
  }, "%.2f"));

  bleBoatLock.registerParam("gyroNorm", makeFloatParam([&]() {
    return compassReady ? compass.getGyroNormDps() : 0.0f;
  }, "%.2f"));

  bleBoatLock.registerParam("magQ", makeFloatParam([&]() {
    return compassReady ? (float)compass.getMagQuality() : 0.0f;
  }, "%.0f"));

  bleBoatLock.registerParam("gyroQ", makeFloatParam([&]() {
    return compassReady ? (float)compass.getGyroQuality() : 0.0f;
  }, "%.0f"));

  bleBoatLock.registerParam("pitch", makeFloatParam([&]() {
    return compassReady ? compass.getPitchDeg() : 0.0f;
  }, "%.2f"));

  bleBoatLock.registerParam("roll", makeFloatParam([&]() {
    return compassReady ? compass.getRollDeg() : 0.0f;
  }, "%.2f"));

  bleBoatLock.registerParam("anchorLat", makeFloatParam([&]() {
    return isnan(anchor.anchorLat) ? 0.0 : anchor.anchorLat;
  }, "%.6f"));

  bleBoatLock.registerParam("anchorLon", makeFloatParam([&]() {
    return isnan(anchor.anchorLon) ? 0.0 : anchor.anchorLon;
  }, "%.6f"));

  bleBoatLock.registerParam("anchorHead", makeFloatParam([&]() {
    return anchor.anchorHeading;
  }, "%.1f"));

  bleBoatLock.registerParam("holdHeading", makeFloatParam([&]() {
    return settings.get("HoldHeading");
  }, "%.0f"));

  bleBoatLock.registerParam("stepSpr", makeFloatParam([&]() {
    return settings.get("StepSpr");
  }, "%.0f"));

  bleBoatLock.registerParam("stepMaxSpd", makeFloatParam([&]() {
    return settings.get("StepMaxSpd");
  }, "%.0f"));

  bleBoatLock.registerParam("stepAccel", makeFloatParam([&]() {
    return settings.get("StepAccel");
  }, "%.0f"));

  bleBoatLock.registerParam("distance", makeFloatParam([&]() {
    return dist;
  }, "%.2f"));

  bleBoatLock.registerParam("driftSpeed", makeFloatParam([&]() {
    return driftMonitor.driftSpeedMps();
  }, "%.2f"));

  bleBoatLock.registerParam("driftAlert", makeFloatParam([&]() {
    return driftAlertActive ? 1.0f : 0.0f;
  }, "%.0f"));

  bleBoatLock.registerParam("driftFail", makeFloatParam([&]() {
    return driftFailActive ? 1.0f : 0.0f;
  }, "%.0f"));

  bleBoatLock.registerParam("gnssQ", makeFloatParam([&]() {
    return (float)gnssQualityLevel();
  }, "%.0f"));

  bleBoatLock.registerParam("motorPwm", makeFloatParam([&]() {
    return (float)motor.pwmPercent();
  }, "%.0f"));

  bleBoatLock.registerParam("mode", [&]() {
    return std::string(currentModeLabel());
  });

  bleBoatLock.registerParam("simState", [&]() {
    return std::string(hilSim.stateLabel());
  });

  bleBoatLock.registerParam("simScenario", [&]() {
    return hilSim.currentScenarioId();
  });

  bleBoatLock.registerParam("simProgress", makeFloatParam([&]() {
    return hilSim.progressPct();
  }, "%.1f"));

  bleBoatLock.registerParam("simPass", makeFloatParam([&]() {
    return (float)hilSim.lastPass();
  }, "%.0f"));

  bleBoatLock.registerParam("cfgVer", makeFloatParam([&]() {
    return (float)Settings::VERSION;
  }, "%.0f"));

  bleBoatLock.registerParam("anchorProfile", makeFloatParam([&]() {
    return settings.get("AnchorProf");
  }, "%.0f"));

  bleBoatLock.registerParam("anchorProfileName", [&]() {
    const int raw = constrain((int)settings.get("AnchorProf"), 0, 2);
    return std::string(anchorProfileName((AnchorProfileId)raw));
  });

  bleBoatLock.registerParam("fixType", [&]() {
    return std::string("UNKNOWN");
  });

  bleBoatLock.registerParam("fixTypeSource", [&]() {
    return std::string(fixTypeSourceString(fixTypeSource));
  });

  bleBoatLock.registerParam("anchorDeniedReason", [&]() {
    return std::string(currentAnchorDeniedReason());
  });

  bleBoatLock.registerParam("failsafeReason", [&]() {
    return std::string(currentFailsafeReason());
  });

  bleBoatLock.registerParam("secPaired", makeFloatParam([&]() {
    return bleSecurity.isPaired() ? 1.0f : 0.0f;
  }, "%.0f"));

  bleBoatLock.registerParam("secAuth", makeFloatParam([&]() {
    return bleSecurity.sessionActive() ? 1.0f : 0.0f;
  }, "%.0f"));

  bleBoatLock.registerParam("secPairWin", makeFloatParam([&]() {
    return bleSecurity.pairingWindowOpen(millis()) ? 1.0f : 0.0f;
  }, "%.0f"));

  bleBoatLock.registerParam("secNonce", makeFloatParam([&]() {
    return (float)bleSecurity.sessionNonce();
  }, "%.0f"));

  bleBoatLock.registerParam("secReject", [&]() {
    return std::string(bleSecurity.lastRejectString());
  });
}

void updateGpsFromSerial() {
  size_t bytesRead = 0;
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
    ++bytesRead;
  }

  if (bytesRead > 0 && !gpsUartSeen) {
    gpsUartSeen = true;
    logMessage("[GPS] UART data detected RX=%d TX=%d baud=9600\n",
               cfg::kGpsRxPin,
               cfg::kGpsTxPin);
  }

  if (!gpsUartSeen && !gpsNoDataWarned && millis() - bootMs > 6000) {
    gpsNoDataWarned = true;
    logMessage("[GPS] no UART bytes on RX=%d (check wiring/baud/power)\n", cfg::kGpsRxPin);
  }

  if (settings.get("DebugGps") == 1 && millis() - lastGpsDebugMs >= 1000) {
    Serial.printf("[GPS] bytes=%lu valid=%d fix=%d sats=%d hdop=%.2f age=%lu\n",
                  gps.charsProcessed(),
                  gps.location.isValid(),
                  gpsFix,
                  gps.satellites.value(),
                  gps.hdop.value() * 0.01f,
                  gps.location.age());
    lastGpsDebugMs = millis();
  }

  const unsigned long maxGpsAgeMs =
      static_cast<unsigned long>(constrain(settings.get("MaxGpsAgeMs"), 300.0f, 20000.0f));
  const bool hardwareFix = gps.location.isValid() && gps.location.age() < maxGpsAgeMs;
  if (hardwareFix) {
    int requestedWindow = constrain((int)settings.get("GpsFWin"),
                                    1,
                                    cfg::kMaxGpsFilterWindow);
    if (requestedWindow != gpsFilter.window) {
      gpsFilter.reset(requestedWindow);
    }

    const float rawLat = gps.location.lat();
    const float rawLon = gps.location.lng();
    const float maxPosJumpM = constrain(settings.get("MaxPosJumpM"), 1.0f, 200.0f);
    const bool hasPrev = gpsFilter.count > 0;
    if (hasPrev) {
      const float jumpM =
          TinyGPSPlus::distanceBetween(lastLat, lastLon, rawLat, rawLon);
      currentPosJumpM = isfinite(jumpM) ? jumpM : 0.0f;
      if (isfinite(jumpM) && jumpM > maxPosJumpM) {
        currentPosJumpRejected = true;
        currentGpsAgeMs = gps.location.age();
        logMessage("[EVENT] GPS_JUMP_REJECTED jump=%.2f max=%.2f\n", jumpM, maxPosJumpM);
        return;
      }
    }

    gpsFilter.push(rawLat, rawLon);
    lastLat = gpsFilter.avgLat();
    lastLon = gpsFilter.avgLon();
    const float hwSpeed = gps.speed.kmph();
    currentSpeedKmh = (isfinite(hwSpeed) && hwSpeed > 0.0f) ? hwSpeed : 0.0f;
    currentSpeedMps = currentSpeedKmh / 3.6f;
    if (speedSampleMs > 0 && isfinite(prevSpeedMps)) {
      const float dt = (millis() - speedSampleMs) / 1000.0f;
      if (isfinite(dt) && dt > 0.05f && dt < 5.0f) {
        currentAccelMps2 = (currentSpeedMps - prevSpeedMps) / dt;
        currentAccelValid = true;
      } else {
        currentAccelValid = false;
      }
    } else {
      currentAccelValid = false;
    }
    speedSampleMs = millis();
    prevSpeedMps = currentSpeedMps;
    currentSatellites = max(0, (int)gps.satellites.value());
    currentGpsHdop = gps.hdop.isValid() ? (gps.hdop.value() * 0.01f) : NAN;
    currentGpsAgeMs = gps.location.age();
    currentPosJumpRejected = false;
    ++hwFixSamplesCount;
    gpsSourcePhone = false;
    gpsFix = true;
    maybeUpdateGpsHeadingCorrection(lastLat, lastLon, currentSpeedKmh);
    return;
  }

  if (phoneGpsValid && (millis() - phoneGpsUpdatedMs <= cfg::kPhoneGpsTimeoutMs)) {
    lastLat = phoneLat;
    lastLon = phoneLon;
    currentSpeedKmh =
        (isfinite(phoneSpeedKmh) && phoneSpeedKmh > 0.0f) ? phoneSpeedKmh : 0.0f;
    currentSpeedMps = currentSpeedKmh / 3.6f;
    currentAccelValid = false;
    prevSpeedMps = currentSpeedMps;
    currentSatellites = max(0, phoneSatellites);
    currentGpsHdop = NAN;
    currentGpsAgeMs = millis() - phoneGpsUpdatedMs;
    currentPosJumpRejected = false;
    currentPosJumpM = 0.0f;
    gpsSourcePhone = true;
    gpsFix = true;
    maybeUpdateGpsHeadingCorrection(lastLat, lastLon, currentSpeedKmh);
    return;
  }

  currentSpeedKmh = 0.0f;
  currentSpeedMps = 0.0f;
  currentAccelMps2 = 0.0f;
  currentAccelValid = false;
  prevSpeedMps = NAN;
  currentSatellites = 0;
  currentGpsHdop = NAN;
  currentGpsAgeMs = 999999;
  currentPosJumpRejected = false;
  currentPosJumpM = 0.0f;
  gpsSourcePhone = false;
  gpsFix = false;
  gpsCorrRefValid = false;
}

void handleBootButton() {
  const bool nowButton = digitalRead(cfg::kBootPin);
  if (lastBootButton == HIGH && nowButton == LOW && (gps.location.isValid() || gpsFix)) {
    anchor.saveAnchor(lastLat,
                      lastLon,
                      currentHeadingValue());
    if (canEnableAnchorNow()) {
      setLastAnchorDeniedReason(AnchorDeniedReason::NONE);
      setLastFailsafeReason(FailsafeReason::NONE);
      settings.set("AnchorEnabled", 1);
      settings.save();
      logMessage("[EVENT] ANCHOR_ON source=BOOT_BUTTON\n");
      logMessage("[ANCHOR] enabled from BOOT button\n");
    } else {
      setLastAnchorDeniedReason(currentGnssDeniedReason());
      settings.set("AnchorEnabled", 0);
      settings.save();
      logMessage("[EVENT] ANCHOR_DENIED reason=%s source=BOOT_BUTTON\n", currentGnssFailReason());
      logMessage("[ANCHOR] BOOT set point, enable rejected (GNSS quality)\n");
    }
    logMessage("Anchor point set!\n");
  }
  lastBootButton = nowButton;
}

void handleStopButton() {
  const bool nowButton = digitalRead(cfg::kStopButtonPin);
  const unsigned long now = millis();
  if (lastStopButton == HIGH && nowButton == LOW) {
    stopButtonDownSinceMs = now;
    stopPairingActionDone = false;
    setLastFailsafeReason(FailsafeReason::STOP_CMD);
    stopAllMotionNow();
    logMessage("[STOP] hardware stop button pressed (GPIO%d)\n", cfg::kStopButtonPin);
  } else if (nowButton == LOW) {
    if (!stopPairingActionDone && stopButtonDownSinceMs > 0 && now - stopButtonDownSinceMs >= 3000) {
      stopPairingActionDone = true;
      bleSecurity.openPairingWindow(now);
      logMessage("[EVENT] PAIRING_WINDOW_OPEN source=STOP_BUTTON timeout_ms=%lu\n",
                 (unsigned long)BleSecurity::kPairingWindowMs);
    }
  } else if (lastStopButton == LOW && nowButton == HIGH) {
    stopButtonDownSinceMs = 0;
    stopPairingActionDone = false;
  }
  lastStopButton = nowButton;
}

void updateDistanceAndBearing() {
  if (!manualMode && settings.get("AnchorEnabled") == 1) {
    if (settings.get("HoldHeading") == 1) {
      bearing = anchor.anchorHeading;
      dist = gpsFix
                 ? TinyGPSPlus::distanceBetween(lastLat, lastLon, anchor.anchorLat, anchor.anchorLon)
                 : 0.0f;
      return;
    }

    if (gpsFix) {
      dist = TinyGPSPlus::distanceBetween(lastLat, lastLon, anchor.anchorLat, anchor.anchorLon);
      const float computedBearing =
          TinyGPSPlus::courseTo(lastLat, lastLon, anchor.anchorLat, anchor.anchorLon);
      if (isfinite(computedBearing)) {
        bearing = computedBearing;
        anchorBearingCacheDeg = computedBearing;
        anchorBearingCacheUpdatedMs = millis();
        anchorBearingCacheValid = true;
      } else if (anchorBearingCacheValid &&
                 millis() - anchorBearingCacheUpdatedMs <= cfg::kAnchorBearingCacheMaxAgeMs) {
        bearing = anchorBearingCacheDeg;
      } else {
        bearing = 0.0f;
      }
      return;
    }

    if (anchorBearingCacheValid &&
        millis() - anchorBearingCacheUpdatedMs <= cfg::kAnchorBearingCacheMaxAgeMs) {
      bearing = anchorBearingCacheDeg;
      dist = 0.0f;
      return;
    }
  }

  dist = 0.0f;
  bearing = 0.0f;
}

void updateDriftState(unsigned long now) {
  const bool anchorActive = !manualMode && (settings.get("AnchorEnabled") == 1);
  const float driftAlertM = settings.get("DriftAlert");
  const float driftFailM = settings.get("DriftFail");
  driftMonitor.update(now, anchorActive, gpsFix, dist, driftAlertM, driftFailM);
  driftAlertActive = driftMonitor.alertActive();
  driftFailActive = driftMonitor.failActive();
}

void applyMotionControl(unsigned long now,
                        bool autoControlActive,
                        bool hasHeading,
                        bool hasBearing,
                        float heading,
                        float diff) {
  if (manualMode) {
    stepperTrackingActive = false;
    motorHeadingAligned = false;
    if (manualDir == 0) {
      stepperControl.startManual(-1);
    } else if (manualDir == 1) {
      stepperControl.startManual(1);
    } else {
      stepperControl.stopManual();
    }
    motor.driveManual(manualSpeed);
    return;
  }

  stepperControl.stopManual();

  bool motorCanRun = false;
  if (autoControlActive) {
    if (hasHeading && hasBearing) {
      const float absDiff = fabsf(diff);
      const float deadband = currentStepperDeadbandDeg();
      const float engageBand = deadband + cfg::kStepperDeadbandHystDeg;

      if (stepperTrackingActive) {
        if (absDiff <= deadband) {
          stepperTrackingActive = false;
        }
      } else if (absDiff >= engageBand) {
        stepperTrackingActive = true;
      }

      if (stepperTrackingActive &&
          now - lastStepperCheckMs >= cfg::kStepperCheckIntervalMs) {
        stepperControl.pointToBearing(bearing, heading);
        lastStepperCheckMs = now;
      } else if (!stepperTrackingActive) {
        stepperControl.cancelMove();
      }

      const float motorTol = max(2.0f, settings.get("AngTol"));
      const float motorReleaseTol = motorTol + cfg::kMotorHeadingHystDeg;
      if (motorHeadingAligned) {
        if (absDiff > motorReleaseTol) {
          motorHeadingAligned = false;
        }
      } else if (absDiff <= motorTol) {
        motorHeadingAligned = true;
      }
      motorCanRun = motorHeadingAligned;
    } else {
      stepperTrackingActive = false;
      motorHeadingAligned = false;
      stepperControl.cancelMove();
    }
  } else {
    stepperTrackingActive = false;
    motorHeadingAligned = false;
    stepperControl.cancelMove();
  }

  const float holdRadiusMeters = max(0.5f, settings.get("HoldRadius"));
  const float deadbandMeters = constrain(settings.get("DeadbandM"), 0.2f, 10.0f);
  const int maxThrustPctAnchor = constrain((int)settings.get("MaxThrustA"), 10, 100);
  const float thrustRampPctPerS = constrain(settings.get("ThrRampA"), 1.0f, 100.0f);
  const bool allowThrust = autoControlActive && motorCanRun && !driftFailActive;
  motor.driveAnchorAuto(dist,
                        holdRadiusMeters,
                        allowThrust,
                        deadbandMeters,
                        maxThrustPctAnchor,
                        thrustRampPctPerS);
}

void publishBleAndUi(unsigned long now,
                     bool hasHeading,
                     bool hasBearing,
                     float heading,
                     float diffDeg) {
  const char* mode = currentModeLabel();
  const float displayHeading = hasHeading ? heading : fallbackHeading;
  const float displayBearing = hasBearing ? bearing : fallbackBearing;
  const int compassQuality = compassReady ? compass.getHeadingQuality() : 0;
  const int motorPwmPercent = motor.pwmPercent();
  const bool headingFromPhone = false;

  if (now - lastUiRefreshMs >= cfg::kUiRefreshIntervalMs) {
    display_draw_ui(gpsFix,
                    currentSatellites,
                    gpsSourcePhone,
                    currentGpsHdop,
                    currentSpeedKmh,
                    displayHeading,
                    hasHeading,
                    headingFromPhone,
                    compassQuality,
                    displayBearing,
                    dist,
                    diffDeg,
                    mode,
                    motorPwmPercent);
    lastUiRefreshMs = now;
  }

  if (now - lastBleNotifyMs >= cfg::kBleNotifyIntervalMs) {
    std::string err;
    if (!gps.location.isValid() && !gpsFix) {
      err = "NO_GPS";
    } else if (!gpsQualityGoodForAnchorOn()) {
      err = currentGnssFailReason();
    }
    if (!hasHeading) {
      if (!err.empty()) {
        err += ",";
      }
      err += "NO_COMPASS";
    }
    if (driftFailActive) {
      if (!err.empty()) {
        err += ",";
      }
      err += "DRIFT_FAIL";
    } else if (driftAlertActive) {
      if (!err.empty()) {
        err += ",";
      }
      err += "DRIFT_ALERT";
    }
    if (settings.get("AnchorEnabled") != 1 && lastAnchorDeniedReason != AnchorDeniedReason::NONE) {
      if (!err.empty()) {
        err += ",";
      }
      err += currentAnchorDeniedReason();
    }
    if (lastFailsafeReason != FailsafeReason::NONE) {
      if (!err.empty()) {
        err += ",";
      }
      err += currentFailsafeReason();
    }
    const std::string safety = activeSafetyReason(now);
    if (!safety.empty()) {
      if (!err.empty()) {
        err += ",";
      }
      err += safety;
    }

    std::string status = std::string(bleBoatLock.statusString()) + ":" + mode;
    if (!err.empty()) {
      status += ":" + err;
    }

    bleBoatLock.setStatus(status);
    bleBoatLock.notifyAll();
    lastBleNotifyMs = now;
  }
}

std::string trimAscii(const std::string& in) {
  size_t b = 0;
  while (b < in.size() && (in[b] == ' ' || in[b] == '\t' || in[b] == '\r' || in[b] == '\n')) {
    ++b;
  }
  size_t e = in.size();
  while (e > b &&
         (in[e - 1] == ' ' || in[e - 1] == '\t' || in[e - 1] == '\r' || in[e - 1] == '\n')) {
    --e;
  }
  return in.substr(b, e - b);
}

bool parseSimRunCommand(const std::string& command, std::string* id, int* speedup) {
  if (!id || !speedup) {
    return false;
  }
  *id = "";
  *speedup = 0;
  std::string payload;
  const size_t colon = command.find(':');
  if (colon != std::string::npos && colon + 1 < command.size()) {
    payload = command.substr(colon + 1);
  } else {
    const size_t space = command.find(' ');
    if (space != std::string::npos && space + 1 < command.size()) {
      payload = command.substr(space + 1);
    }
  }
  payload = trimAscii(payload);
  if (payload.empty()) {
    return false;
  }

  if (!payload.empty() && payload.front() == '{') {
    // Accept: SIM_RUN {id=S1_current_0p4_good,speedup=0}
    const size_t idPos = payload.find("id");
    if (idPos != std::string::npos) {
      size_t sep = payload.find_first_of(":=", idPos);
      if (sep != std::string::npos) {
        size_t start = sep + 1;
        while (start < payload.size() && (payload[start] == ' ' || payload[start] == '"' || payload[start] == '\'')) {
          ++start;
        }
        size_t end = start;
        while (end < payload.size() && payload[end] != ',' && payload[end] != '}' &&
               payload[end] != '"' && payload[end] != '\'') {
          ++end;
        }
        *id = trimAscii(payload.substr(start, end - start));
      }
    }
    const size_t spPos = payload.find("speedup");
    if (spPos != std::string::npos) {
      size_t sep = payload.find_first_of(":=", spPos);
      if (sep != std::string::npos) {
        *speedup = atoi(payload.c_str() + sep + 1);
      }
    }
    return !id->empty();
  }

  // Accept: SIM_RUN:S1_current_0p4_good,0
  const size_t comma = payload.find(',');
  if (comma == std::string::npos) {
    *id = trimAscii(payload);
    return !id->empty();
  }
  *id = trimAscii(payload.substr(0, comma));
  *speedup = atoi(payload.c_str() + comma + 1);
  return !id->empty();
}

} // namespace

void setPhoneGpsFix(float lat, float lon, float speedKmh, int satellites) {
  phoneLat = lat;
  phoneLon = lon;
  phoneSpeedKmh = speedKmh;
  phoneSatellites = max(0, satellites);
  phoneGpsUpdatedMs = millis();
  phoneGpsValid = true;
}

void captureStepperBowZero() {
  stepperControl.captureBowZero();
}

bool hasAnchorPoint() {
  return anchorPointConfigured();
}

bool canEnableAnchorNow() {
  return hasAnchorPoint() && gpsQualityGoodForAnchorOn();
}

const char* currentGnssFailReason() {
  return gnssFailReasonString(lastGnssFailReason);
}

AnchorDeniedReason currentGnssDeniedReason() {
  return deniedReasonFromGnss(lastGnssFailReason);
}

bool preprocessSecureCommand(const std::string& incoming, std::string* effective) {
  if (!effective) {
    return false;
  }
  *effective = incoming;
  const unsigned long now = millis();

  if (incoming == "AUTH_HELLO") {
    const uint32_t nonce = bleSecurity.startAuth(now);
    if (nonce == 0) {
      logMessage("[EVENT] AUTH_DENIED reason=%s\n", bleSecurity.lastRejectString());
    } else {
      logMessage("[EVENT] AUTH_CHALLENGE nonce=%08lX\n", (unsigned long)nonce);
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
    const unsigned long code = strtoul(incoming.c_str() + 9, nullptr, 10);
    if (bleSecurity.setPairCode((uint32_t)code, now)) {
      logMessage("[EVENT] PAIRING_COMPLETED code=%06lu\n", code);
    } else {
      logMessage("[EVENT] PAIRING_DENIED reason=%s\n", bleSecurity.lastRejectString());
    }
    return false;
  }

  if (incoming == "PAIR_CLEAR") {
    bleSecurity.clearPairing();
    logMessage("[EVENT] PAIRING_CLEARED\n");
    return false;
  }

  if (incoming.rfind("SEC_CMD:", 0) == 0) {
    std::string payload;
    if (!bleSecurity.unwrapSecureCommand(incoming, &payload, now)) {
      logMessage("[EVENT] AUTH_REJECT reason=%s\n", bleSecurity.lastRejectString());
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
  if (command.rfind("SIM_", 0) != 0) {
    return false;
  }

  if (command == "SIM_LIST") {
    logMessage("[SIM] LIST %s\n", hilSim.listCsv().c_str());
    return true;
  }

  if (command == "SIM_STATUS") {
    logMessage("[SIM] STATUS %s\n", hilSim.statusJson().c_str());
    return true;
  }

  if (command.rfind("SIM_RUN", 0) == 0) {
    std::string scenarioId;
    int speedup = 0;
    if (!parseSimRunCommand(command, &scenarioId, &speedup)) {
      logMessage("[SIM] RUN rejected: invalid payload\n");
      return true;
    }
    stopAllMotionNow();
    setLastFailsafeReason(FailsafeReason::NONE);
    setLastAnchorDeniedReason(AnchorDeniedReason::NONE);
    std::string err;
    if (!hilSim.startRun(scenarioId, speedup, &err)) {
      logMessage("[SIM] RUN failed id=%s err=%s\n", scenarioId.c_str(), err.c_str());
      return true;
    }
    simLastWallMs = millis();
    logMessage("[SIM] RUN started id=%s speedup=%d\n", scenarioId.c_str(), speedup);
    return true;
  }

  if (command == "SIM_ABORT") {
    hilSim.abortRun();
    logMessage("[SIM] ABORTED\n");
    return true;
  }

  if (command.rfind("SIM_REPORT", 0) == 0) {
    const std::string json = hilSim.reportJson();
    if (json.empty()) {
      logMessage("[SIM] REPORT unavailable\n");
      return true;
    }
    const size_t chunkSize = 220;
    for (size_t i = 0; i < json.size(); i += chunkSize) {
      const std::string chunk = json.substr(i, chunkSize);
      logMessage("[SIM] REPORT %s\n", chunk.c_str());
    }
    return true;
  }

  logMessage("[SIM] unknown command: %s\n", command.c_str());
  return true;
}

void setLastAnchorDeniedReason(AnchorDeniedReason reason) {
  lastAnchorDeniedReason = reason;
}

void setLastFailsafeReason(FailsafeReason reason) {
  lastFailsafeReason = reason;
}

const char* currentAnchorDeniedReason() {
  return anchorDeniedReasonString(lastAnchorDeniedReason);
}

const char* currentFailsafeReason() {
  return failsafeReasonString(lastFailsafeReason);
}

void stopAllMotionNow() {
  const bool wasAnchorOn = settings.get("AnchorEnabled") == 1;
  settings.set("AnchorEnabled", 0);
  settings.save();
  manualMode = false;
  manualDir = -1;
  manualSpeed = 0;
  stepperTrackingActive = false;
  motorHeadingAligned = false;
  stepperControl.stopManual();
  stepperControl.cancelMove();
  motor.stop();
  if (wasAnchorOn) {
    logMessage("[EVENT] ANCHOR_OFF reason=STOP\n");
  }
}

void noteControlActivityNow() {
  anchorSupervisor.noteControlActivity(millis());
}

void disableAnchorAndStop(const char* reason) {
  settings.set("AnchorEnabled", 0);
  settings.save();
  stepperTrackingActive = false;
  motorHeadingAligned = false;
  stepperControl.cancelMove();
  motor.stop();
  setSafetyReason(reason);
  logMessage("[EVENT] ANCHOR_OFF reason=%s\n", reason ? reason : "UNKNOWN");
}

void applySupervisorDecision(const AnchorSupervisor::Decision& decision) {
  if (decision.action == AnchorSupervisor::SafeAction::NONE) {
    return;
  }
  FailsafeReason fsReason = FailsafeReason::NONE;
  switch (decision.reason) {
    case AnchorSupervisor::Reason::GPS_WEAK:
      fsReason = FailsafeReason::GPS_WEAK;
      break;
    case AnchorSupervisor::Reason::COMM_TIMEOUT:
      fsReason = FailsafeReason::COMM_TIMEOUT;
      break;
    case AnchorSupervisor::Reason::CONTROL_LOOP_TIMEOUT:
      fsReason = FailsafeReason::CONTROL_LOOP_TIMEOUT;
      break;
    case AnchorSupervisor::Reason::SENSOR_TIMEOUT:
      fsReason = FailsafeReason::SENSOR_TIMEOUT;
      break;
    case AnchorSupervisor::Reason::INTERNAL_ERROR_NAN:
      fsReason = FailsafeReason::INTERNAL_ERROR_NAN;
      break;
    case AnchorSupervisor::Reason::COMMAND_OUT_OF_RANGE:
      fsReason = FailsafeReason::COMMAND_OUT_OF_RANGE;
      break;
    case AnchorSupervisor::Reason::NONE:
    default:
      fsReason = FailsafeReason::NONE;
      break;
  }
  const char* reason = failsafeReasonString(fsReason);
  setLastFailsafeReason(fsReason);
  if (decision.action == AnchorSupervisor::SafeAction::EXIT_ANCHOR) {
    disableAnchorAndStop(reason);
    return;
  }

  settings.set("AnchorEnabled", 0);
  settings.save();

  if (decision.action == AnchorSupervisor::SafeAction::MANUAL) {
    manualMode = true;
    manualDir = -1;
    manualSpeed = 0;
    stepperTrackingActive = false;
    motorHeadingAligned = false;
    stepperControl.stopManual();
    stepperControl.cancelMove();
    motor.stop();
    setSafetyReason(reason);
    logMessage("[EVENT] FAILSAFE_TRIGGERED reason=%s action=MANUAL\n", reason);
    return;
  }

  stopAllMotionNow();
  setSafetyReason(reason);
  logMessage("[EVENT] FAILSAFE_TRIGGERED reason=%s action=STOP\n", reason);
}

bool nudgeAnchorBearing(float bearingDeg, float meters) {
  if (settings.get("AnchorEnabled") != 1) {
    logMessage("[EVENT] NUDGE_DENIED reason=ANCHOR_INACTIVE\n");
    return false;
  }
  if (!canEnableAnchorNow()) {
    logMessage("[EVENT] NUDGE_DENIED reason=GNSS_QUALITY\n");
    return false;
  }
  if (!isfinite(bearingDeg) || !isfinite(meters) || meters < 1.0f || meters > 5.0f) {
    logMessage("[EVENT] NUDGE_DENIED reason=RANGE\n");
    return false;
  }
  if (!isfinite(anchor.anchorLat) || !isfinite(anchor.anchorLon)) {
    logMessage("[EVENT] NUDGE_DENIED reason=NO_ANCHOR_POINT\n");
    return false;
  }

  const double lat1 = anchor.anchorLat * (M_PI / 180.0);
  const double lon1 = anchor.anchorLon * (M_PI / 180.0);
  const double brg = normalize360Deg(bearingDeg) * (M_PI / 180.0);
  const double distRatio = meters / 6378137.0;

  const double sinLat2 =
      sin(lat1) * cos(distRatio) + cos(lat1) * sin(distRatio) * cos(brg);
  const double lat2 = asin(sinLat2);
  const double lon2 = lon1 + atan2(sin(brg) * sin(distRatio) * cos(lat1),
                                   cos(distRatio) - sin(lat1) * sin(lat2));

  const float newLat = (float)(lat2 * 180.0 / M_PI);
  const float newLon = (float)(lon2 * 180.0 / M_PI);
  anchor.saveAnchor(newLat, newLon, anchor.anchorHeading, true);
  setSafetyReason("NUDGE_OK");
  logMessage("[EVENT] NUDGE_APPLIED bearing=%.1f meters=%.2f lat=%.6f lon=%.6f\n",
             normalize360Deg(bearingDeg),
             meters,
             newLat,
             newLon);
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

  float heading = currentHeadingValue();
  float bearing = heading;
  if (strcasecmp(dir, "FWD") == 0 || strcasecmp(dir, "FORWARD") == 0) {
    bearing = heading;
  } else if (strcasecmp(dir, "BACK") == 0) {
    bearing = heading + 180.0f;
  } else if (strcasecmp(dir, "LEFT") == 0) {
    bearing = heading - 90.0f;
  } else if (strcasecmp(dir, "RIGHT") == 0) {
    bearing = heading + 90.0f;
  } else {
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

  EEPROM.begin(EEPROM_SIZE);
  settings.load();
  bleSecurity.load();
  logMessage("[SEC] paired=%d\n", bleSecurity.isPaired() ? 1 : 0);
  logMessage("[CFG] HoldHeading=%d\n", (int)settings.get("HoldHeading"));
  if (!minFixTypeIgnoredLogged && fixTypeSource == FixTypeSource::NONE) {
    minFixTypeIgnoredLogged = true;
    logMessage("[EVENT] CONFIG_FIELD_IGNORED field=min_fix_type reason=fix_type_unavailable\n");
  }

  gpsFilter.reset((int)settings.get("GpsFWin"));

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

  updateGpsFromSerial();
  handleBootButton();
  handleStopButton();

  if (!compassReady && millis() - lastCompassRetryMs >= cfg::kCompassRetryIntervalMs) {
    lastCompassRetryMs = millis();
    compassReady = compass.init();
    logMessage("[COMPASS] retry ready=%d source=%s", (int)compassReady, compass.sourceName());
    if (compassReady) {
      logMessage(" addr=0x%02X", compass.bnoI2cAddress());
    }
    logMessage("\n");
    if (compassReady) {
      compass.loadCalibrationFromSettings();
      logMessage("[COMPASS] heading offset=%.1f\n", compass.getHeadingOffsetDeg());
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

  const bool anchorActive = !manualMode && (settings.get("AnchorEnabled") == 1);
  const bool gpsQualityOk = gpsQualityGoodForAnchorOn();
  const bool bleConnected = (bleBoatLock.bleStatus == BLEBoatLock::CONNECTED);
  AnchorSupervisor::Config supCfg;
  supCfg.commTimeoutMs =
      static_cast<unsigned long>(constrain(settings.get("CommToutMs"), 500.0f, 60000.0f));
  supCfg.controlLoopTimeoutMs =
      static_cast<unsigned long>(constrain(settings.get("CtrlLoopMs"), 100.0f, 10000.0f));
  supCfg.sensorTimeoutMs =
      static_cast<unsigned long>(constrain(settings.get("SensorTout"), 300.0f, 30000.0f));
  supCfg.gpsWeakGraceMs =
      static_cast<unsigned long>(constrain(settings.get("GpsWeakHys"), 0.5f, 60.0f) * 1000.0f);
  supCfg.maxCommandThrustPct = 100;
  supCfg.failsafeAction =
      ((int)settings.get("FailAct") == 1) ? AnchorSupervisor::SafeAction::MANUAL
                                          : AnchorSupervisor::SafeAction::STOP;
  supCfg.nanGuardAction =
      ((int)settings.get("NanAct") == 1) ? AnchorSupervisor::SafeAction::MANUAL
                                         : AnchorSupervisor::SafeAction::STOP;

  AnchorSupervisor::Input supIn;
  supIn.nowMs = now;
  supIn.anchorActive = anchorActive;
  supIn.gpsQualityOk = gpsQualityOk;
  supIn.linkOk = bleConnected;
  supIn.sensorsOk = gpsFix && headingAvailable();
  supIn.hasNaN = (!isfinite(dist) || !isfinite(bearing));
  supIn.controlLoopDtMs = loopDtMs;
  supIn.commandThrustPct = motor.pwmPercent();
  applySupervisorDecision(anchorSupervisor.update(supCfg, supIn));

  const bool autoControlActive = !manualMode && (settings.get("AnchorEnabled") == 1);
  const float heading = currentHeadingValue();
  const bool hasHeading = headingAvailable();
  const bool anchorBearingAvailable =
      (settings.get("HoldHeading") == 1) ||
      gpsFix ||
      (anchorBearingCacheValid &&
       millis() - anchorBearingCacheUpdatedMs <= cfg::kAnchorBearingCacheMaxAgeMs);
  const bool hasBearing = autoControlActive && anchorBearingAvailable;
  const float diff = (hasHeading && hasBearing) ? normalizeDiffDeg(bearing, heading) : 0.0f;

  applyMotionControl(now, autoControlActive, hasHeading, hasBearing, heading, diff);
  const AnchorDiagnostics::Events diag = anchorDiagnostics.update(
      now,
      autoControlActive,
      gpsFix,
      hasHeading,
      driftAlertActive,
      motor.pwmPercent(),
      MotorControl::AUTO_MAX_PWM_PCT);
  if (diag.driftAlert) {
    logMessage("[EVENT] DRIFT_ALERT dist=%.2f\n", dist);
  }
  if (diag.controlSaturated) {
    logMessage("[EVENT] CONTROL_SATURATED pwm=%d\n", motor.pwmPercent());
  }
  if (diag.sensorTimeout) {
    logMessage("[EVENT] SENSOR_TIMEOUT\n");
  }
  publishBleAndUi(now, hasHeading, hasBearing, heading, diff);

  bleBoatLock.loop();
}
