#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <EEPROM.h>
#include <AccelStepper.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Settings.h"
#include "Logger.h"
Settings settings;
constexpr size_t EEPROM_SIZE = Settings::EEPROM_ADDR + sizeof(float) * count + sizeof(uint8_t);
#include "AnchorControl.h"
#include "MotorControl.h"
#include "StepperControl.h"

#include "BoatDisplay.h"
#include "HMC5883Compass.h"
#include "PathControl.h"
#include "BleCommandHandler.h"

#include "BLEBoatLock.h"
BLEBoatLock bleBoatLock;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BoatDisplay boatDisplay(&display);

#define BUTTON_PIN 0
#define STEP_PIN 5
#define DIR_PIN 4
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

bool manualMode = false;
int manualDir = -1;
int manualSpeed = 0;

void drawDebug(const String &msg, int y = 48) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,  48);
  display.print("[D]");
  display.print(msg);
  display.display();
}

template<typename F>
std::function<std::string()> makeFloatParam(F valueFunc, const char* fmt) {
    return [valueFunc, fmt]() {
        char buf[20];
        snprintf(buf, sizeof(buf), fmt, valueFunc());
        return std::string(buf);
    };
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
  logMessage("\n[BoatLock] ESP32 стартует! Версия прошивки: 0.1.2\n");

  Wire.begin(8, 9);
  compassReady = compass.init();

  pinMode(BOOT_PIN, INPUT_PULLUP);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  gpsSerial.begin(9600, SERIAL_8N1, 17, 18);

  bleBoatLock.setCommandHandler(handleBleCommand);
  bleBoatLock.registerParam("lat", makeFloatParam([&](){
      return gps.location.isValid() ? gps.location.lat() : 0.0;
  }, "%.6f"));
  bleBoatLock.registerParam("lon", makeFloatParam([&](){
      return gps.location.isValid() ? gps.location.lng() : 0.0;
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
  compass.attachSettings(&settings);
  if (compassReady) {
    compass.loadCalibrationFromSettings();
  }
  anchor.attachSettings(&settings);
  anchor.loadAnchor();
  bleBoatLock.begin();
  
  bleBoatLock.registerParam("distance", makeFloatParam([&](){ return dist; }, "%.2f"));

  pinMode(BUTTON_PIN, INPUT_PULLUP);
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

  float lat = gps.location.lat();
  float lon = gps.location.lng();
  if (gps.location.isValid()) {
    lastLat = lat;
    lastLon = lon;
    gpsFix = true;
  }

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

  stepperControl.run();

  if (!manualMode && pathControl.active) {
    if (gps.location.isValid()) {
      dist = pathControl.distanceToCurrent(gps);
      bearing = pathControl.bearingToCurrent(gps);
      pathControl.update(gps, settings.get("DistTh"));
    } else if (gpsFix && pathControl.currentIndex < pathControl.numPoints) {
      const Waypoint &wp = pathControl.points[pathControl.currentIndex];
      dist = TinyGPSPlus::distanceBetween(lastLat, lastLon, wp.lat, wp.lon);
      bearing = TinyGPSPlus::courseTo(lastLat, lastLon, wp.lat, wp.lon);
    }
  } else if (!manualMode && settings.get("AnchorEnabled") == 1) {
    if (gps.location.isValid()) {
      dist = anchor.distanceToAnchor(gps);
      if (settings.get("HoldHeading") == 1) {
        bearing = anchor.anchorHeading;
      } else {
        bearing = anchor.bearingToAnchor(gps);
      }
    } else if (gpsFix) {
      dist = TinyGPSPlus::distanceBetween(lastLat, lastLon, anchor.anchorLat, anchor.anchorLon);
      if (settings.get("HoldHeading") == 1) {
        bearing = anchor.anchorHeading;
      } else {
        bearing = TinyGPSPlus::courseTo(lastLat, lastLon, anchor.anchorLat, anchor.anchorLon);
      }
    }
  }

  float heading = settings.get("EmuCompass") ? emuHeading : (compassReady ? compass.getAzimuth() : 0.0f);
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
    float diff = bearing - heading;
    if (diff > 180) diff -= 360;
    if (diff < -180) diff += 360;
    if (!stepperControl.busy && fabs(diff) > 2.0f) {
      stepperControl.moveToBearing(bearing, heading);
    }

    if (pathControl.active || settings.get("AnchorEnabled") == 1) {
      motor.applyPID(dist);
      holding = true;
    } else {
      motor.stop();
      holding = false;
    }
  }

    while (gpsSerial.available()) {
      char c = gpsSerial.read();
      gps.encode(c);
    }

    static unsigned long lastNotify = 0;
    unsigned long now = millis();
    if (now - lastNotify > 1000) {
        bleBoatLock.notifyAll();
        lastNotify = now;

        boatDisplay.showStatus(
                gps,
                anchor.anchorLat, anchor.anchorLon, settings.get("AnchorEnabled"),
                dist, bearing,
                settings.get("EmuCompass") ? emuHeading : (compassReady ? compass.getAzimuth() : 0.0f),
                holding
            );
    }
  bleBoatLock.loop();
}
