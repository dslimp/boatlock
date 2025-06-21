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
Settings settings;
constexpr size_t EEPROM_SIZE = Settings::EEPROM_ADDR + sizeof(float) * count + sizeof(uint8_t);
#include "AnchorControl.h"
#include "EncoderCalib.h"
#include "MotorControl.h"
#include "StepperControl.h"

#include "BoatDisplay.h"
#include "QMC5883LCompass.h"
#include "PathControl.h"

unsigned long lastNotifyBle = 0;

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
#define MOTOR_DIR_PIN 6
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;
const int PWM_CHANNEL = 0;

HardwareSerial gpsSerial(1);
TinyGPSPlus gps;
AS5600 encoder;
StepperControl stepperControl(STEP_PIN, DIR_PIN);

TaskHandle_t stepperTaskHandle = nullptr;
void stepperTask(void*);

QMC5883LCompass compass;
AnchorControl anchor;
EncoderCalib encoderCalib;
MotorControl motor;
PathControl pathControl;

TaskHandle_t compassCalibTaskHandle = nullptr;

void compassCalibTask(void*);

#define BOOT_PIN 0

bool anchorSet = false;
bool holding = false;
float targetAngle = 0;
float encoderAngle = 0;
unsigned long lastDraw = 0;
const unsigned long drawInterval = 100;
float dist = 0, bearing = 0;

float lastLat = 0, lastLon = 0;
float prevBearing = 0;
bool gpsFix = false;
float emuHeading = 0;

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

void adjustPosition(float bearing) {
  targetAngle = bearing;
}

void calibrateCompassAndSave() {
  drawDebug("Calibrating compass");
  compass.calibrate();
  settings.set("MagOffX", compass.getCalibrationOffset(0));
  settings.set("MagOffY", compass.getCalibrationOffset(1));
  settings.set("MagOffZ", compass.getCalibrationOffset(2));
  settings.set("MagScaleX", compass.getCalibrationScale(0));
  settings.set("MagScaleY", compass.getCalibrationScale(1));
  settings.set("MagScaleZ", compass.getCalibrationScale(2));
  settings.save();
  drawDebug("Calib done");
}

void compassCalibTask(void*) {
  calibrateCompassAndSave();
  compassCalibTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

void startCompassCalibration() {
  if (compassCalibTaskHandle == nullptr) {
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
  Serial.println("\n[BoatLock] ESP32 стартует! Версия прошивки: 0.1.0");

  Wire.begin(8, 9);
  compass.init();

  pinMode(BOOT_PIN, INPUT_PULLUP);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  gpsSerial.begin(9600, SERIAL_8N1, 17, 18);

  bleBoatLock.setCommandHandler([](const std::string& cmd) {
    if (cmd.rfind("SET_ANCHOR:", 0) == 0) {
      float lat = 0, lon = 0;
      sscanf(cmd.c_str() + 11, "%f,%f", &lat, &lon);
      anchor.saveAnchor(lat, lon, settings.get("EmuCompass") ? emuHeading : compass.getAzimuth());
      Serial.printf("[BLE] Anchor set via BLE: %.6f, %.6f\n", lat, lon);
    } else if (cmd.rfind("SET_HOLD_HEADING:", 0) == 0) {
      int val = atoi(cmd.c_str() + 17);
      settings.set("HoldHeading", val);
      settings.save();
      Serial.printf("[BLE] HoldHeading set to %d\n", val);
    } else if (cmd.rfind("SET_ROUTE:",0) == 0) {
      pathControl.reset();
      const char* s = cmd.c_str() + 10;
      while (*s) {
        float lat=0, lon=0; int n=0;
        if (sscanf(s, "%f,%f%n", &lat, &lon, &n) == 2) {
          pathControl.addPoint(lat, lon);
          s += n;
          if (*s == ';') s++;
        } else break;
      }
      Serial.printf("[BLE] Route set with %d points\n", pathControl.numPoints);
    } else if (cmd == "START_ROUTE") {
      pathControl.start();
      settings.set("AnchorEnabled", 0);
    } else if (cmd == "STOP_ROUTE") {
      pathControl.stop();
    } else if (cmd == "CALIB_COMPASS") {
      startCompassCalibration();
    } else if (cmd.rfind("SET_HEADING:",0) == 0) {
      emuHeading = atof(cmd.c_str() + 12);
    } else if (cmd.rfind("EMU_COMPASS:",0) == 0) {
      int v = atoi(cmd.c_str() + 12);
      settings.set("EmuCompass", v);
      settings.save();
    } else if (cmd.rfind("SET_STEP_SPR:",0) == 0) {
      int v = atoi(cmd.c_str() + 13);
      settings.set("StepSpr", v);
      settings.save();
      stepperControl.loadFromSettings();
    } else if (cmd.rfind("SET_STEP_MAXSPD:",0) == 0) {
      float v = atof(cmd.c_str() + 15);
      settings.set("StepMaxSpd", v);
      settings.save();
      stepperControl.loadFromSettings();
    } else if (cmd.rfind("SET_STEP_ACCEL:",0) == 0) {
      float v = atof(cmd.c_str() + 15);
      settings.set("StepAccel", v);
      settings.save();
      stepperControl.loadFromSettings();
    } else {
      Serial.printf("[BLE] Unhandled command: %s\n", cmd.c_str());
    }
  });

  bleBoatLock.registerParam("distance", makeFloatParam([&](){ return dist; }, "%.2f"));
  // bleBoatLock.registerParam("lat",      makeFloatParam([&](){ return gps.location.lat(); }, "%.6f"));
  // bleBoatLock.registerParam("lon",      makeFloatParam([&](){ return gps.location.lng(); }, "%.6f"));
  bleBoatLock.registerParam("lat", makeFloatParam([&](){
      return gps.location.isValid() ? gps.location.lat() : 0.0;
  }, "%.6f"));

  bleBoatLock.registerParam("lon", makeFloatParam([&](){
      return gps.location.isValid() ? gps.location.lng() : 0.0;
  }, "%.6f"));

  bleBoatLock.registerParam("heading",  makeFloatParam([&](){
      return settings.get("EmuCompass") ? emuHeading : (float)compass.getAzimuth();
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
  compass.setCalibrationOffsets(
      settings.get("MagOffX"),
      settings.get("MagOffY"),
      settings.get("MagOffZ"));
  compass.setCalibrationScales(
      settings.get("MagScaleX"),
      settings.get("MagScaleY"),
      settings.get("MagScaleZ"));
  anchor.attachSettings(&settings);
  anchor.loadAnchor();
  drawDebug("settings load");

  bleBoatLock.begin();

  encoderCalib.setSettings(&settings);    
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  motor.setupPWM(MOTOR_PWM_PIN, PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  motor.setDirPin(MOTOR_DIR_PIN);
  motor.loadPIDfromSettings();
  drawDebug("motor load");

  // encoder.begin();
  drawDebug("encoder begin");
//   encoderCalib.loadOffset();
  stepperControl.attachSettings(&settings);
  stepperControl.loadFromSettings();

  xTaskCreatePinnedToCore(stepperTask, "stepper", 2048, nullptr, 1, &stepperTaskHandle, 1);

  if(settings.get("AnchorEnabled") == 1) {
    anchorSet = true;
  }
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
      anchor.saveAnchor(lastLat, lastLon, settings.get("EmuCompass") ? emuHeading : compass.getAzimuth());
      anchorSet = true;
      Serial.println("Anchor point set!");
    }
  }
  lastButton = nowButton;

  compass.read();
  stepperControl.run();

  if (pathControl.active) {
    if (gps.location.isValid()) {
      dist = pathControl.distanceToCurrent(gps);
      bearing = pathControl.bearingToCurrent(gps);
      pathControl.update(gps, settings.get("DistTh"));
    } else if (gpsFix && pathControl.currentIndex < pathControl.numPoints) {
      const Waypoint &wp = pathControl.points[pathControl.currentIndex];
      dist = TinyGPSPlus::distanceBetween(lastLat, lastLon, wp.lat, wp.lon);
      bearing = TinyGPSPlus::courseTo(lastLat, lastLon, wp.lat, wp.lon);
    }
  } else if (settings.get("AnchorEnabled") == 1) {
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

  float heading = settings.get("EmuCompass") ? emuHeading : compass.getAzimuth();
  float diff = bearing - prevBearing;
  if (diff > 180) diff -= 360;
  if (diff < -180) diff += 360;
  if (!stepperControl.busy && fabs(diff) > 2.0f) {
    stepperControl.moveToBearing(bearing, heading);
    prevBearing = bearing;
  }

    while (gpsSerial.available()) {
      char c = gpsSerial.read();
      gps.encode(c);
    }
    if (gps.location.isValid()) {
      lastLat = gps.location.lat();
      lastLon = gps.location.lng();
      gpsFix = true;
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
                settings.get("EmuCompass") ? emuHeading : compass.getAzimuth(),
                holding
            );
    }
  bleBoatLock.loop();
  // stepperControl.moveToBearing(bearing, compass.getAzimuth());
}
