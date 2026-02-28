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
Settings settings;
constexpr size_t EEPROM_SIZE = Settings::EEPROM_ADDR + sizeof(float) * count + sizeof(uint8_t);
#include "AnchorControl.h"
#include "MotorControl.h"
#include "StepperControl.h"

#include "display.h"
#include "HMC5883Compass.h"
#include "PathControl.h"
#include "BleCommandHandler.h"

#include "BLEBoatLock.h"
BLEBoatLock bleBoatLock;

File routeLog;
const char* ROUTE_LOG_PATH = "/route.csv";
bool sdReady = false;

#define STEP_PIN 5
#define DIR_PIN 4
#define I2C_SDA_PIN 47
#define I2C_SCL_PIN 48
#define GPS_RX_PIN 17
#define GPS_TX_PIN 18
#define MOTOR_PWM_PIN 7
#define MOTOR_DIR_PIN1 6
#define MOTOR_DIR_PIN2 10
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;
const int PWM_CHANNEL = 0;

HardwareSerial gpsSerial(1);
TinyGPSPlus gps;
StepperControl stepperControl(STEP_PIN, DIR_PIN);

TaskHandle_t stepperTaskHandle = nullptr;
void stepperTask(void*);

HMC5883Compass compass;
AnchorControl anchor;
MotorControl motor;
PathControl pathControl;
bool compassReady = false;
float fallbackHeading = 0.0f;
float fallbackBearing = 0.0f;

TaskHandle_t compassCalibTaskHandle = nullptr;

void compassCalibTask(void*);

#define BOOT_PIN 0

bool holding = false;
float targetAngle = 0;
float encoderAngle = 0;
unsigned long lastDraw = 0;
const unsigned long drawInterval = 100;
float dist = 0, bearing = 0;

float lastLat = 0, lastLon = 0;
bool gpsFix = false;
float emuHeading = 0;

const int MAX_GPS_FILTER = 20;
float latBuf[MAX_GPS_FILTER];
float lonBuf[MAX_GPS_FILTER];
int gpsBufCount = 0;
int gpsBufIndex = 0;
int gpsWindow = 5;
float gpsLatSum = 0, gpsLonSum = 0;

bool manualMode = false;
int manualDir = -1;
int manualSpeed = 0;

void drawDebug(const String &msg, int y = 48) {
  display_draw_debug(msg, y);
}

void calibrateCompassAndSave() {
  if (!compassReady) return;
  drawDebug("Calibrating compass");
  compass.calibrate();
  drawDebug("Calib done");
}

void compassCalibTask(void*) {
  calibrateCompassAndSave();
  compassCalibTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

void startCompassCalibration() {
  if (compassCalibTaskHandle == nullptr && compassReady) {
    xTaskCreatePinnedToCore(compassCalibTask, "compassCalib", 4096, nullptr, 1, &compassCalibTaskHandle, 1);
  }
}

void stepperTask(void*) {
  for(;;) {
    stepperControl.run();
    vTaskDelay(1);
  }
}

void setup() {
  Serial.begin(115200);
  logMessage("\n[BoatLock] ESP32 стартует! Версия прошивки: 0.1.5\n");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  compassReady = compass.init();

  pinMode(BOOT_PIN, INPUT_PULLUP);

  if (!display_init()) {
    logMessage("Display init failed\n");
  }
  randomSeed(micros());
  fallbackHeading = random(0, 360);
  fallbackBearing = random(0, 360);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  bleBoatLock.setCommandHandler(handleBleCommand);
  bleBoatLock.registerParam("lat", makeFloatParam([&](){
      return gpsFix ? lastLat : 0.0f;
  }, "%.6f"));
  bleBoatLock.registerParam("lon", makeFloatParam([&](){
      return gpsFix ? lastLon : 0.0f;
  }, "%.6f"));
  bleBoatLock.registerParam("heading",  makeFloatParam([&](){
      if (settings.get("EmuCompass")) return emuHeading;
      return compassReady ? (float)compass.getAzimuth() : 0.0f;
  }, "%.1f"));
  bleBoatLock.registerParam("anchorLat", makeFloatParam([&](){ return isnan(anchor.anchorLat) ? 0.0 : anchor.anchorLat;}, "%.6f"));
  bleBoatLock.registerParam("anchorLon", makeFloatParam([&](){ return isnan(anchor.anchorLon) ? 0.0 : anchor.anchorLon;}, "%.6f"));
  bleBoatLock.registerParam("anchorHead", makeFloatParam([&](){ return anchor.anchorHeading; }, "%.1f"));
  bleBoatLock.registerParam("holdHeading", makeFloatParam([&](){ return settings.get("HoldHeading"); }, "%.0f"));
  bleBoatLock.registerParam("emuCompass", makeFloatParam([&](){ return settings.get("EmuCompass"); }, "%.0f"));
  bleBoatLock.registerParam("routeIdx", makeFloatParam([&](){ return (float)pathControl.currentIndex; }, "%.0f"));
  bleBoatLock.registerParam("stepSpr", makeFloatParam([&](){ return settings.get("StepSpr"); }, "%.0f"));
  bleBoatLock.registerParam("stepMaxSpd", makeFloatParam([&](){ return settings.get("StepMaxSpd"); }, "%.0f"));
  bleBoatLock.registerParam("stepAccel", makeFloatParam([&](){ return settings.get("StepAccel"); }, "%.0f"));

  EEPROM.begin(EEPROM_SIZE);
  settings.load();
  gpsWindow = (int)settings.get("GpsFWin");
  compass.attachSettings(&settings);
  if (compassReady) {
    compass.loadCalibrationFromSettings();
  }
  anchor.attachSettings(&settings);
  anchor.loadAnchor();
  bleBoatLock.begin();
 
  sdReady = SD.begin();
  if (sdReady) {
    routeLog = SD.open(ROUTE_LOG_PATH, FILE_APPEND);
  }

  bleBoatLock.registerParam("distance", makeFloatParam([&](){ return dist; }, "%.2f"));
  bleBoatLock.registerParam("mode", [ & ](){
      if (manualMode) return std::string("MANUAL");
      if (pathControl.active) return std::string("ROUTE");
      if (settings.get("AnchorEnabled") == 1) return std::string("ANCHOR");
      return std::string("IDLE");
  });

  pinMode(BOOT_PIN, INPUT_PULLUP);
  motor.setupPWM(MOTOR_PWM_PIN, PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  motor.setDirPins(MOTOR_DIR_PIN1, MOTOR_DIR_PIN2);
  motor.loadPIDfromSettings();
  stepperControl.attachSettings(&settings);
  stepperControl.loadFromSettings();

  xTaskCreatePinnedToCore(stepperTask, "stepper", 2048, nullptr, 1, &stepperTaskHandle, 1);
}

void loop() {
  static bool lastButton = HIGH;
  bool nowButton = digitalRead(BOOT_PIN);

  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  static unsigned long lastGpsDebug = 0;
  if (settings.get("DebugGps") == 1 && millis() - lastGpsDebug >= 1000) {
    Serial.printf("[GPS] bytes=%lu valid=%d fix=%d sats=%d hdop=%.2f age=%lu\n",
                  gps.charsProcessed(),
                  gps.location.isValid(),
                  gpsFix,
                  gps.satellites.value(),
                  gps.hdop.value() * 0.01f,
                  gps.location.age());
    lastGpsDebug = millis();
  }

  float lat = gps.location.lat();
  float lon = gps.location.lng();
  if (gps.location.isValid() && gps.location.age() < 2000) {
    int window = (int)settings.get("GpsFWin");
    if (window < 1) window = 1;
    if (window > MAX_GPS_FILTER) window = MAX_GPS_FILTER;
    if (window != gpsWindow) {
      gpsWindow = window;
      gpsBufCount = 0;
      gpsBufIndex = 0;
      gpsLatSum = 0;
      gpsLonSum = 0;
    }
    if (gpsBufCount < gpsWindow) {
      latBuf[gpsBufIndex] = lat;
      lonBuf[gpsBufIndex] = lon;
      gpsLatSum += lat;
      gpsLonSum += lon;
      gpsBufCount++;
    } else {
      gpsLatSum -= latBuf[gpsBufIndex];
      gpsLonSum -= lonBuf[gpsBufIndex];
      latBuf[gpsBufIndex] = lat;
      lonBuf[gpsBufIndex] = lon;
      gpsLatSum += lat;
      gpsLonSum += lon;
    }
    gpsBufIndex = (gpsBufIndex + 1) % gpsWindow;
    lastLat = gpsLatSum / gpsBufCount;
    lastLon = gpsLonSum / gpsBufCount;
    gpsFix = true;
  } else {
    gpsFix = false;
  }

  if (sdReady && gps.location.isUpdated() && gps.location.isValid()) {
    if (routeLog) {
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
  }

  unsigned long now = millis();

  if (lastButton == HIGH && nowButton == LOW) {
    if (gps.location.isValid() || gpsFix) {
      anchor.saveAnchor(lastLat, lastLon, settings.get("EmuCompass") ? emuHeading : (compassReady ? compass.getAzimuth() : 0.0f));
      settings.set("AnchorEnabled", 1);
      logMessage("Anchor point set!\n");
    }
  }
  lastButton = nowButton;

  if (compassReady) {
    compass.read();
  }

  if (!manualMode && pathControl.active && gpsFix && pathControl.currentIndex < pathControl.numPoints) {
    const Waypoint &wp = pathControl.points[pathControl.currentIndex];
    dist = TinyGPSPlus::distanceBetween(lastLat, lastLon, wp.lat, wp.lon);
    bearing = TinyGPSPlus::courseTo(lastLat, lastLon, wp.lat, wp.lon);
    if (dist < settings.get("DistTh")) {
      pathControl.currentIndex++;
      if (pathControl.currentIndex >= pathControl.numPoints) {
        pathControl.active = false;
      }
    }
  } else if (!manualMode && settings.get("AnchorEnabled") == 1 && gpsFix) {
    dist = TinyGPSPlus::distanceBetween(lastLat, lastLon, anchor.anchorLat, anchor.anchorLon);
    if (settings.get("HoldHeading") == 1) {
      bearing = anchor.anchorHeading;
    } else {
      bearing = TinyGPSPlus::courseTo(lastLat, lastLon, anchor.anchorLat, anchor.anchorLon);
    }
  }

  float heading = settings.get("EmuCompass") ? emuHeading : (compassReady ? compass.getAzimuth() : 0.0f);
  bool hasHeading = settings.get("EmuCompass") || compassReady;
  bool hasBearing = gpsFix;
  float diff = 0.0f;
  float errorDeg = 0.0f;
  if (hasHeading && hasBearing) {
    diff = bearing - heading;
    if (diff > 180) diff -= 360;
    if (diff < -180) diff += 360;
    errorDeg = fabs(diff);
  }
  if (manualMode) {
    if (manualDir == 0) {
      stepperControl.startManual(-1);
    } else if (manualDir == 1) {
      stepperControl.startManual(1);
    } else {
      stepperControl.stopManual();
    }
    motor.driveManual(manualSpeed);
    holding = false;
  } else {
    stepperControl.stopManual();
    if (hasHeading && hasBearing) {
      static unsigned long lastStepCheck = 0;
      const unsigned long stepInterval = 3000;
      if (!stepperControl.busy && fabs(diff) > 2.0f && now - lastStepCheck > stepInterval) {
        stepperControl.moveToBearing(bearing, heading);
        lastStepCheck = now;
      }
    } else {
      stepperControl.cancelMove();
    }

    if (pathControl.active || settings.get("AnchorEnabled") == 1) {
      motor.applyPID(dist);
      holding = true;
    } else {
      motor.stop();
      holding = false;
    }
  }

  static unsigned long lastNotify = 0;
  if (now - lastNotify > 1000) {
      std::string modeStr;
      if (manualMode) modeStr = "MANUAL";
      else if (pathControl.active) modeStr = "ROUTE";
      else if (settings.get("AnchorEnabled") == 1) modeStr = "ANCHOR";
      else modeStr = "IDLE";

      std::string errStr;
      if (!gps.location.isValid() && !gpsFix) errStr = "NO_GPS";
      if (!compassReady) {
          if (!errStr.empty()) errStr += ",";
          errStr += "NO_COMPASS";
      }
      std::string bleStr = bleBoatLock.statusString();
      std::string status = bleStr + ":" + modeStr;
      if (!errStr.empty()) status += ":" + errStr;
      bleBoatLock.setStatus(status);

      bleBoatLock.notifyAll();
      lastNotify = now;

      int batteryPercent = 0;
      float displayHeading = hasHeading ? heading : fallbackHeading;
      float displayBearing = hasBearing ? bearing : fallbackBearing;
      display_draw_ui(
          gpsFix,
          gps.satellites.value(),
          gps.speed.kmph(),
          displayHeading,
          displayBearing,
          dist,
          errorDeg,
          modeStr.c_str(),
          batteryPercent
      );
  }
  bleBoatLock.loop();
}

void exportRouteLog() {
  if (!sdReady) return;
  routeLog.flush();
  routeLog.close();
  File f = SD.open(ROUTE_LOG_PATH, FILE_READ);
  if (!f) return;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    if (line.length() > 0) {
      String out = String("ROUTE:") + line;
      bleBoatLock.sendLog(out.c_str());
      delay(5);
    }
  }
  f.close();
  routeLog = SD.open(ROUTE_LOG_PATH, FILE_APPEND);
}

void clearRouteLog() {
  if (!sdReady) return;
  routeLog.close();
  SD.remove(ROUTE_LOG_PATH);
  routeLog = SD.open(ROUTE_LOG_PATH, FILE_APPEND);
}
