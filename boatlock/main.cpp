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
#include "HMC5883Compass.h"
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

constexpr const char* kRouteLogPath = "/route.csv";
} // namespace cfg

File routeLog;
bool sdReady = false;

HardwareSerial gpsSerial(1);
TinyGPSPlus gps;
StepperControl stepperControl(cfg::kStepPin, cfg::kDirPin);

HMC5883Compass compass;
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
TaskHandle_t compassCalibTaskHandle = nullptr;

unsigned long lastGpsDebugMs = 0;
unsigned long lastBleNotifyMs = 0;
unsigned long lastUiRefreshMs = 0;
unsigned long lastStepperCheckMs = 0;
bool lastBootButton = HIGH;

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

float normalizeDiffDeg(float targetDeg, float currentDeg) {
  float diff = targetDeg - currentDeg;
  if (diff > 180.0f) diff -= 360.0f;
  if (diff < -180.0f) diff += 360.0f;
  return diff;
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
    return compass.getAzimuth();
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

void drawDebug(const String& msg, int y = 48) {
  display_draw_debug(msg, y);
}

void calibrateCompassAndSave() {
  if (!compassReady) {
    return;
  }
  drawDebug("Calibrating compass");
  compass.calibrate();
  drawDebug("Calib done");
}

void compassCalibTask(void*) {
  calibrateCompassAndSave();
  compassCalibTaskHandle = nullptr;
  vTaskDelete(nullptr);
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
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
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
    return;
  }

  currentSpeedKmh = 0.0f;
  currentSatellites = 0;
  gpsSourcePhone = false;
  gpsFix = false;
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
                     float errorDeg) {
  const char* mode = currentModeLabel();
  const float displayHeading = hasHeading ? heading : fallbackHeading;
  const float displayBearing = hasBearing ? bearing : fallbackBearing;
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
                    displayBearing,
                    dist,
                    errorDeg,
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
  if (compassCalibTaskHandle == nullptr && compassReady) {
    xTaskCreatePinnedToCore(compassCalibTask,
                            "compassCalib",
                            4096,
                            nullptr,
                            1,
                            &compassCalibTaskHandle,
                            1);
  }
}

void setup() {
  Serial.begin(115200);
  logMessage("\n[BoatLock] ESP32 start, firmware: %s\n", cfg::kFirmwareVersion);

  pinMode(cfg::kBootPin, INPUT_PULLUP);

  Wire.begin(cfg::kI2cSdaPin, cfg::kI2cSclPin);
  compassReady = compass.init();

  if (!display_init()) {
    logMessage("Display init failed\n");
  }

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
  const float errorDeg = (hasHeading && hasBearing) ? fabsf(diff) : 0.0f;

  const unsigned long now = millis();
  applyMotionControl(now, autoControlActive, hasHeading, hasBearing, heading, diff);
  publishBleAndUi(now, hasHeading, hasBearing, heading, errorDeg);

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
