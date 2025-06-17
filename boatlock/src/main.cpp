#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <EEPROM.h>
#include <AccelStepper.h>
#include <math.h>

#include "Settings.h"
Settings settings;
constexpr size_t EEPROM_SIZE = Settings::EEPROM_ADDR + sizeof(float) * count + sizeof(uint8_t);
#include "AnchorControl.h"
#include "EncoderCalib.h"
#include "MotorControl.h"

#include "BoatDisplay.h"
#include "QMCCompass.h"

unsigned long lastNotifyBle = 0;

#include "BLEBoatLock.h"
BLEBoatLock bleBoatLock;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BoatDisplay boatDisplay(&display);

#define BUTTON_PIN 0
#define STEP_PIN 2
#define DIR_PIN 3
#define MOTOR_PWM_PIN 5
#define MOTOR_DIR_PIN 6

const int STEPS_PER_REV = 200;
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;
const int PWM_CHANNEL = 0;

HardwareSerial gpsSerial(1);
TinyGPSPlus gps;
AS5600 encoder;
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

QMCCompass compass;
AnchorControl anchor;
EncoderCalib encoderCalib;
MotorControl motor;

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

void moveStepperToBearing(float bearing, float heading) {
  float diff = bearing - heading;
  if (diff > 180) diff -= 360;
  if (diff < -180) diff += 360;
  long targetSteps = lround(diff / 360.0f * STEPS_PER_REV);
  stepper.moveTo(targetSteps);
  stepper.run();
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n[BoatLock] ESP32 стартует! Версия прошивки: 0.1.0");

  Wire.begin(8, 9);
  compass.begin();

  pinMode(BOOT_PIN, INPUT_PULLUP);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  gpsSerial.begin(9600, SERIAL_8N1, 17, 18);

  bleBoatLock.setCommandHandler([](const std::string& cmd) {
    if (cmd.rfind("SET_ANCHOR:", 0) == 0) {
      float lat = 0, lon = 0;
      sscanf(cmd.c_str() + 11, "%f,%f", &lat, &lon);
      anchor.saveAnchor(lat, lon);
      Serial.printf("[BLE] Anchor set via BLE: %.6f, %.6f\n", lat, lon);
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

  bleBoatLock.registerParam("heading",  makeFloatParam([&](){ return compass.getHeading(); }, "%.1f"));
  bleBoatLock.registerParam("anchorLat", makeFloatParam([&](){ return isnan(anchor.anchorLat) ? 0.0 : anchor.anchorLat;}, "%.6f"));
  bleBoatLock.registerParam("anchorLon",makeFloatParam([&](){ return anchor.anchorLng; }, "%.6f"));
  bleBoatLock.registerParam("anchorLng", makeFloatParam([&](){ return isnan(anchor.anchorLng) ? 0.0 : anchor.anchorLng;}, "%.6f"));

  EEPROM.begin(EEPROM_SIZE);
  settings.load();
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
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(500);

  if(settings.get("AnchorEnabled") == 1) {
    anchorSet = true;
  }
}

void loop() {

  static bool lastButton = HIGH;
  bool nowButton = digitalRead(BOOT_PIN);

  float lat = gps.location.lat();
  float lon = gps.location.lng();

  if (lastButton == HIGH && nowButton == LOW) {
    if (gps.location.isValid()) {
      anchor.saveAnchor(lat, lon);
      anchorSet = true;
      Serial.println("Anchor point set!");
    }
  }
  lastButton = nowButton;

  compass.update();

  if (gps.location.isValid() && settings.get("AnchorEnabled")==1) {
    dist = anchor.distanceToAnchor(gps);
    bearing = anchor.bearingToAnchor(gps);
    moveStepperToBearing(bearing, compass.getHeading());
  }

    while (gpsSerial.available()) {
      char c = gpsSerial.read();
      gps.encode(c);
    }

    static unsigned long lastNotify = 0;
    unsigned long now = millis();
    if (now - lastNotify > 5000) {
        bleBoatLock.notifyAll();
        lastNotify = now;

        boatDisplay.showStatus(
                gps,
                anchor.anchorLat, anchor.anchorLng, settings.get("AnchorEnabled"),
                dist, bearing,
                compass.getHeading(),
                holding
            );
    }

  bleBoatLock.loop();
}
