#include <Arduino.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <EEPROM.h>
#include <AccelStepper.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <SPI.h>
#include <SD.h>

#include "ParamHelpers.h"
#include "Settings.h"
#include "Logger.h"
#include "AnchorControl.h"
#include "MotorControl.h"
#include "StepperControl.h"
#include "display.h"
#include "BNO08xCompass.h"
#include "PathControl.h"
#include "BleCommandHandler.h"
#include "BLEBoatLock.h"

Settings settings;
constexpr size_t EEPROM_SIZE =
    Settings::EEPROM_ADDR + sizeof(float) * count + sizeof(uint8_t);

BLEBoatLock bleBoatLock;

namespace cfg {
constexpr char kFirmwareVersion[] = "0.2.0";

constexpr int kStepPin = 5;
constexpr int kDirPin = 4;
#if defined(BOATLOCK_BOARD_JC4832W535)
// JC4832W535 display uses GPIO47/48 for LCD QSPI, so I2C must be moved.
constexpr int kI2cSdaPin = 4;
constexpr int kI2cSclPin = 8;
#else
constexpr int kI2cSdaPin = 47;
constexpr int kI2cSclPin = 48;
#endif
constexpr int kGpsRxPin = 17;
constexpr int kGpsTxPin = 18;
constexpr int kMotorPwmPin = 7;
constexpr int kMotorDirPin1 = 6;
constexpr int kMotorDirPin2 = 10;
constexpr int kBootPin = 0;

constexpr int kPwmFreq = 5000;
constexpr int kPwmResolution = 8;
constexpr int kPwmChannel = 0;

constexpr int kMaxGpsFilterWindow = 20;
constexpr unsigned long kBleNotifyIntervalMs = 1000;
constexpr unsigned long kUiRefreshIntervalMs = 120;
constexpr unsigned long kStepperCheckIntervalMs = 3000;
constexpr unsigned long kHardwareGpsMaxAgeMs = 2000;
constexpr unsigned long kPhoneGpsTimeoutMs = 5000;
constexpr unsigned long kPhoneHeadingTimeoutMs = 3000;
constexpr unsigned long kCompassRetryIntervalMs = 5000;
constexpr float kGpsCorrMinSpeedKmh = 3.0f;
constexpr float kGpsCorrMinMoveM = 4.0f;
constexpr float kGpsCorrAlpha = 0.18f;
constexpr float kGpsCorrMaxAbsDeg = 90.0f;
constexpr unsigned long kGpsCorrMaxAgeMs = 180000;

constexpr const char* kRouteLogPath = "/route.csv";
} // namespace cfg

File routeLog;
bool sdReady = false;

HardwareSerial gpsSerial(1);
TinyGPSPlus gps;
StepperControl stepperControl(cfg::kStepPin, cfg::kDirPin);

BNO08xCompass compass;
AnchorControl anchor;
MotorControl motor;
PathControl pathControl;

bool compassReady = false;
float fallbackHeading = 0.0f;
float fallbackBearing = 0.0f;
float dist = 0.0f;
float bearing = 0.0f;
float lastLat = 0.0f;
float lastLon = 0.0f;
bool gpsFix = false;
float currentSpeedKmh = 0.0f;
int currentSatellites = 0;
float phoneLat = 0.0f;
float phoneLon = 0.0f;
float phoneSpeedKmh = 0.0f;
int phoneSatellites = 0;
unsigned long phoneGpsUpdatedMs = 0;
bool phoneGpsValid = false;
unsigned long phoneHeadingUpdatedMs = 0;
bool phoneHeadingValid = false;
bool gpsSourcePhone = false;
float emuHeading = 0.0f;
float gpsHeadingCorrDeg = 0.0f;
unsigned long gpsHeadingCorrUpdatedMs = 0;
bool gpsHeadingCorrValid = false;
float gpsCorrRefLat = 0.0f;
float gpsCorrRefLon = 0.0f;
bool gpsCorrRefValid = false;

bool manualMode = false;
int manualDir = -1;
int manualSpeed = 0;

void startCompassCalibration();
void exportRouteLog();
void clearRouteLog();
void setPhoneGpsFix(float lat, float lon, float speedKmh, int satellites);
void setPhoneHeading(float headingDeg);

namespace {
TaskHandle_t stepperTaskHandle = nullptr;

unsigned long lastGpsDebugMs = 0;
unsigned long lastBleNotifyMs = 0;
unsigned long lastUiRefreshMs = 0;
unsigned long lastStepperCheckMs = 0;
unsigned long lastCompassRetryMs = 0;
bool lastBootButton = HIGH;
bool gpsUartSeen = false;
bool gpsNoDataWarned = false;
unsigned long bootMs = 0;

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
  if (!compassReady || settings.get("EmuCompass") == 1) {
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
  if (pathControl.active) return "ROUTE";
  if (settings.get("AnchorEnabled") == 1) return "ANCHOR";
  return "IDLE";
}

float currentHeadingValue() {
  const bool phoneHeadingFresh =
      phoneHeadingValid &&
      (millis() - phoneHeadingUpdatedMs <= cfg::kPhoneHeadingTimeoutMs);
  const bool usePhoneHeading =
      (settings.get("EmuCompass") == 1) || (!compassReady && phoneHeadingFresh);

  if (usePhoneHeading) {
    return emuHeading;
  }
  if (compassReady) {
    return correctedCompassHeading(compass.getAzimuth());
  }
  return 0.0f;
}

bool headingAvailable() {
  const bool phoneHeadingFresh =
      phoneHeadingValid &&
      (millis() - phoneHeadingUpdatedMs <= cfg::kPhoneHeadingTimeoutMs);
  return (settings.get("EmuCompass") == 1) || compassReady || (!compassReady && phoneHeadingFresh);
}

bool headingSourceIsPhone() {
  const bool phoneHeadingFresh =
      phoneHeadingValid &&
      (millis() - phoneHeadingUpdatedMs <= cfg::kPhoneHeadingTimeoutMs);
  return (settings.get("EmuCompass") == 1) || (!compassReady && phoneHeadingFresh);
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

  bleBoatLock.registerParam("emuCompass", makeFloatParam([&]() {
    return settings.get("EmuCompass");
  }, "%.0f"));

  bleBoatLock.registerParam("routeIdx", makeFloatParam([&]() {
    return (float)pathControl.currentIndex;
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

  bleBoatLock.registerParam("mode", [&]() {
    return std::string(currentModeLabel());
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

  const bool hardwareFix = gps.location.isValid() && gps.location.age() < cfg::kHardwareGpsMaxAgeMs;
  if (hardwareFix) {
    int requestedWindow = constrain((int)settings.get("GpsFWin"),
                                    1,
                                    cfg::kMaxGpsFilterWindow);
    if (requestedWindow != gpsFilter.window) {
      gpsFilter.reset(requestedWindow);
    }

    gpsFilter.push(gps.location.lat(), gps.location.lng());
    lastLat = gpsFilter.avgLat();
    lastLon = gpsFilter.avgLon();
    const float hwSpeed = gps.speed.kmph();
    currentSpeedKmh = (isfinite(hwSpeed) && hwSpeed > 0.0f) ? hwSpeed : 0.0f;
    currentSatellites = max(0, (int)gps.satellites.value());
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
    currentSatellites = max(0, phoneSatellites);
    gpsSourcePhone = true;
    gpsFix = true;
    maybeUpdateGpsHeadingCorrection(lastLat, lastLon, currentSpeedKmh);
    return;
  }

  currentSpeedKmh = 0.0f;
  currentSatellites = 0;
  gpsSourcePhone = false;
  gpsFix = false;
  gpsCorrRefValid = false;
}

void appendRouteLogIfNeeded() {
  if (!sdReady || !gps.location.isUpdated() || !gps.location.isValid() || !routeLog) {
    return;
  }

  if (gps.date.isValid() && gps.time.isValid()) {
    routeLog.printf("%04d-%02d-%02dT%02d:%02d:%02d,%.6f,%.6f\n",
                    gps.date.year(), gps.date.month(), gps.date.day(),
                    gps.time.hour(), gps.time.minute(), gps.time.second(),
                    gps.location.lat(), gps.location.lng());
  } else {
    routeLog.printf("%lu,%.6f,%.6f\n", millis(), gps.location.lat(), gps.location.lng());
  }

  routeLog.flush();
}

void handleBootButton() {
  const bool nowButton = digitalRead(cfg::kBootPin);
  if (lastBootButton == HIGH && nowButton == LOW && (gps.location.isValid() || gpsFix)) {
    anchor.saveAnchor(lastLat,
                      lastLon,
                      settings.get("EmuCompass") ? emuHeading : currentHeadingValue());
    settings.set("AnchorEnabled", 1);
    logMessage("Anchor point set!\n");
  }
  lastBootButton = nowButton;
}

void updateDistanceAndBearing() {
  if (!manualMode && pathControl.active && gpsFix &&
      pathControl.currentIndex < pathControl.numPoints) {
    const Waypoint& wp = pathControl.points[pathControl.currentIndex];
    dist = TinyGPSPlus::distanceBetween(lastLat, lastLon, wp.lat, wp.lon);
    bearing = TinyGPSPlus::courseTo(lastLat, lastLon, wp.lat, wp.lon);

    if (dist < settings.get("DistTh")) {
      ++pathControl.currentIndex;
      if (pathControl.currentIndex >= pathControl.numPoints) {
        pathControl.active = false;
      }
    }
    return;
  }

  if (!manualMode && settings.get("AnchorEnabled") == 1 && gpsFix) {
    dist = TinyGPSPlus::distanceBetween(lastLat, lastLon, anchor.anchorLat, anchor.anchorLon);
    bearing = (settings.get("HoldHeading") == 1)
                  ? anchor.anchorHeading
                  : TinyGPSPlus::courseTo(lastLat, lastLon, anchor.anchorLat, anchor.anchorLon);
    return;
  }

  dist = 0.0f;
  bearing = 0.0f;
}

void applyMotionControl(unsigned long now,
                        bool autoControlActive,
                        bool hasHeading,
                        bool hasBearing,
                        float heading,
                        float diff) {
  if (manualMode) {
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

  if (autoControlActive && hasHeading && hasBearing) {
    if (!stepperControl.busy && fabsf(diff) > 2.0f && now - lastStepperCheckMs > cfg::kStepperCheckIntervalMs) {
      stepperControl.moveToBearing(bearing, heading);
      lastStepperCheckMs = now;
    }
  } else {
    stepperControl.cancelMove();
  }

  if (autoControlActive) {
    motor.applyPID(dist);
  } else {
    motor.stop();
  }
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
  const int batteryPercent = 0;
  const bool headingFromPhone = hasHeading && headingSourceIsPhone();

  if (now - lastUiRefreshMs >= cfg::kUiRefreshIntervalMs) {
    display_draw_ui(gpsFix,
                    currentSatellites,
                    gpsSourcePhone,
                    currentSpeedKmh,
                    displayHeading,
                    hasHeading,
                    headingFromPhone,
                    compassQuality,
                    displayBearing,
                    dist,
                    diffDeg,
                    mode,
                    batteryPercent);
    lastUiRefreshMs = now;
  }

  if (now - lastBleNotifyMs >= cfg::kBleNotifyIntervalMs) {
    std::string err;
    if (!gps.location.isValid() && !gpsFix) {
      err = "NO_GPS";
    }
    if (!hasHeading) {
      if (!err.empty()) {
        err += ",";
      }
      err += "NO_COMPASS";
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

} // namespace

void setPhoneGpsFix(float lat, float lon, float speedKmh, int satellites) {
  phoneLat = lat;
  phoneLon = lon;
  phoneSpeedKmh = speedKmh;
  phoneSatellites = max(0, satellites);
  phoneGpsUpdatedMs = millis();
  phoneGpsValid = true;
}

void setPhoneHeading(float headingDeg) {
  if (!isfinite(headingDeg)) {
    return;
  }
  float normalized = fmodf(headingDeg, 360.0f);
  if (normalized < 0.0f) {
    normalized += 360.0f;
  }
  emuHeading = normalized;
  phoneHeadingUpdatedMs = millis();
  phoneHeadingValid = true;
}

void startCompassCalibration() {
  logMessage("[COMPASS] explicit calibration command ignored (BNO08x dynamic calibration in use)\n");
}

void setup() {
  Serial.begin(115200);
  logMessage("\n[BoatLock] ESP32 start, firmware: %s\n", cfg::kFirmwareVersion);
  bootMs = millis();

  pinMode(cfg::kBootPin, INPUT_PULLUP);

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
  logMessage("[CFG] EmuCompass=%d HoldHeading=%d\n",
             (int)settings.get("EmuCompass"),
             (int)settings.get("HoldHeading"));

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

  #if defined(BOATLOCK_BOARD_JC4832W535)
  // SD over default SPI can reconfigure the same host used by LCD QSPI on this board.
  sdReady = false;
  #else
  sdReady = SD.begin();
  if (sdReady) {
    routeLog = SD.open(cfg::kRouteLogPath, FILE_APPEND);
  }
  #endif

  motor.setupPWM(cfg::kMotorPwmPin, cfg::kPwmChannel, cfg::kPwmFreq, cfg::kPwmResolution);
  motor.setDirPins(cfg::kMotorDirPin1, cfg::kMotorDirPin2);
  motor.loadPIDfromSettings();

  stepperControl.attachSettings(&settings);
  stepperControl.loadFromSettings();

  xTaskCreatePinnedToCore(stepperTask, "stepper", 2048, nullptr, 1, &stepperTaskHandle, 1);
}

void loop() {
  updateGpsFromSerial();
  appendRouteLogIfNeeded();
  handleBootButton();

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

  const bool autoControlActive =
      !manualMode && (pathControl.active || settings.get("AnchorEnabled") == 1);
  const float heading = currentHeadingValue();
  const bool hasHeading = headingAvailable();
  const bool hasBearing = gpsFix && autoControlActive;
  const float diff = (hasHeading && hasBearing) ? normalizeDiffDeg(bearing, heading) : 0.0f;

  const unsigned long now = millis();
  applyMotionControl(now, autoControlActive, hasHeading, hasBearing, heading, diff);
  publishBleAndUi(now, hasHeading, hasBearing, heading, diff);

  bleBoatLock.loop();
}

void exportRouteLog() {
  if (!sdReady) {
    return;
  }

  routeLog.flush();
  routeLog.close();

  File f = SD.open(cfg::kRouteLogPath, FILE_READ);
  if (!f) {
    return;
  }

  while (f.available()) {
    String line = f.readStringUntil('\n');
    if (line.length() > 0) {
      String out = String("ROUTE:") + line;
      bleBoatLock.sendLog(out.c_str());
      delay(5);
    }
  }

  f.close();
  routeLog = SD.open(cfg::kRouteLogPath, FILE_APPEND);
}

void clearRouteLog() {
  if (!sdReady) {
    return;
  }

  routeLog.close();
  SD.remove(cfg::kRouteLogPath);
  routeLog = SD.open(cfg::kRouteLogPath, FILE_APPEND);
}
