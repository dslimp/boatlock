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
#include <SPI.h>
#include <SD.h>

#include "HardwareConfig.h"
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

File routeLog;
const char* ROUTE_LOG_PATH = "/route.csv";
bool sdReady = false;

#if BOATLOCK_ENABLE_CAMERA
#include <esp_camera.h>
#endif

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BoatDisplay boatDisplay(&display);

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

struct HardwareStatus {
  bool displayReady = false;
  bool cameraReady = false;
  bool imuReady = false;
  bool sdReady = false;
};

HardwareStatus hardwareStatus;

void drawDebug(const String &msg, int y = 48) {
  if (!hardwareStatus.displayReady) {
    logMessage("[D]%s\n", msg.c_str());
    return;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,  y);
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

#if BOATLOCK_ENABLE_IMU
bool probeImu() {
  Wire.beginTransmission(BOATLOCK_IMU_ADDR_PRIMARY);
  if (Wire.endTransmission() == 0) {
    return true;
  }
  Wire.beginTransmission(BOATLOCK_IMU_ADDR_SECONDARY);
  return Wire.endTransmission() == 0;
}
#endif

#if BOATLOCK_ENABLE_CAMERA
bool initCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = BOATLOCK_CAM_Y2_PIN;
  config.pin_d1 = BOATLOCK_CAM_Y3_PIN;
  config.pin_d2 = BOATLOCK_CAM_Y4_PIN;
  config.pin_d3 = BOATLOCK_CAM_Y5_PIN;
  config.pin_d4 = BOATLOCK_CAM_Y6_PIN;
  config.pin_d5 = BOATLOCK_CAM_Y7_PIN;
  config.pin_d6 = BOATLOCK_CAM_Y8_PIN;
  config.pin_d7 = BOATLOCK_CAM_Y9_PIN;
  config.pin_xclk = BOATLOCK_CAM_XCLK_PIN;
  config.pin_pclk = BOATLOCK_CAM_PCLK_PIN;
  config.pin_vsync = BOATLOCK_CAM_VSYNC_PIN;
  config.pin_href = BOATLOCK_CAM_HREF_PIN;
  config.pin_sscb_sda = BOATLOCK_CAM_SIOD_PIN;
  config.pin_sscb_scl = BOATLOCK_CAM_SIOC_PIN;
  config.pin_pwdn = BOATLOCK_CAM_PWDN_PIN;
  config.pin_reset = BOATLOCK_CAM_RESET_PIN;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    logMessage("Camera init failed: %d\n", err);
    return false;
  }
  return true;
}
#endif

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

  hardwareStatus.displayReady = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!hardwareStatus.displayReady) {
    logMessage("Display init failed\n");
  }
  gpsSerial.begin(9600, SERIAL_8N1, 17, 18);

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
  hardwareStatus.sdReady = sdReady;
  if (hardwareStatus.sdReady) {
    routeLog = SD.open(ROUTE_LOG_PATH, FILE_APPEND);
  }

#if BOATLOCK_ENABLE_IMU
  hardwareStatus.imuReady = probeImu();
  if (!hardwareStatus.imuReady) {
    logMessage("IMU not detected on I2C\n");
  }
#else
  hardwareStatus.imuReady = true;
#endif

#if BOATLOCK_ENABLE_CAMERA
  hardwareStatus.cameraReady = initCamera();
#endif

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
    if (gpsFix) {
      float diff = bearing - heading;
      if (diff > 180) diff -= 360;
      if (diff < -180) diff += 360;
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
#if BOATLOCK_ENABLE_IMU
      if (!hardwareStatus.imuReady) {
          if (!errStr.empty()) errStr += ",";
          errStr += "NO_IMU";
      }
#endif
      if (!hardwareStatus.displayReady) {
          if (!errStr.empty()) errStr += ",";
          errStr += "NO_DISPLAY";
      }
#if BOATLOCK_ENABLE_CAMERA
      if (!hardwareStatus.cameraReady) {
          if (!errStr.empty()) errStr += ",";
          errStr += "NO_CAMERA";
      }
#endif
      std::string bleStr = bleBoatLock.statusString();
      std::string status = bleStr + ":" + modeStr;
      if (!errStr.empty()) status += ":" + errStr;
      bleBoatLock.setStatus(status);

      bleBoatLock.notifyAll();
      lastNotify = now;

      if (hardwareStatus.displayReady) {
        boatDisplay.showStatus(
                gps,
                anchor.anchorLat, anchor.anchorLon, settings.get("AnchorEnabled"),
                dist, bearing,
                settings.get("EmuCompass") ? emuHeading : (compassReady ? compass.getAzimuth() : 0.0f),
                holding
            );
      }
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
