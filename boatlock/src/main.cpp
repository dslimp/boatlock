#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <EEPROM.h>
#include <Adafruit_BNO055.h>
// #include <utility/imumaths.h>
// #include <AS5600.h>
#include <AccelStepper.h>
#include <math.h>

#include "Settings.h"
Settings settings;
constexpr size_t EEPROM_SIZE =
    Settings::EEPROM_ADDR + sizeof(entries) + sizeof(uint8_t);
// #include "MPUCompass.h"
#include "AnchorControl.h"
#include "EncoderCalib.h"
#include "MotorControl.h"

#include "BoatDisplay.h"

#include "BoatIMU.h"

// BoatIMU boatIMU;

// #include "RemoteControl.h"
// RemoteControlServer remote;

unsigned long lastNotifyBle = 0;

#include "BLEBoatLock.h"
BLEBoatLock bleBoatLock;

// --- Дисплей ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BoatDisplay boatDisplay(&display);

// --- Пины и константы ---
#define BUTTON_PIN 0
#define STEP_PIN 2
#define DIR_PIN 3
#define MOTOR_PWM_PIN 5
#define MOTOR_DIR_PIN 6

const int STEPS_PER_REV = 200;
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;
const int PWM_CHANNEL = 0;

// --- Глобальные объекты ---
HardwareSerial gpsSerial(1);
TinyGPSPlus gps;
Adafruit_BNO055 bno = Adafruit_BNO055(55);
AS5600 encoder;
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// --- Логика ---
// MPUCompass compass;
AnchorControl anchor;
EncoderCalib encoderCalib;
MotorControl motor;

#define BOOT_PIN 0

// float anchorLat = 0;
// float anchorLon = 0;
bool anchorSet = false;


bool holding = false;
float targetAngle = 0;
float encoderAngle = 0;
unsigned long lastDraw = 0;
const unsigned long drawInterval = 100;
float dist = 0, bearing = 0;

float lastLat = 0, lastLon = 0;
float prevBearing = 0;

// void setMotorRaw(int power, bool dir) {
//     digitalWrite(MOTOR_DIR_PIN, dir ? HIGH : LOW);
//     ledcWrite(PWM_CHANNEL, constrain(power, 0, 255));
// }

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


void scanI2CAndDisplay() {
    // Wire.begin(sdaPin, sclPin);

    Serial.println("I2C scan:");
    drawDebug("I2C scan:");

    uint8_t found = 0;
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            Serial.print("Found: 0x");
            Serial.println(address, HEX);
            // drawDebug("Found",address, HEX);
            found++;

            if (found % 5 == 0) display.println();
        }
    }
    if (!found) {
        Serial.println("Nothing found");
    }
}


void adjustPosition(float bearing) {
  targetAngle = bearing;
}

void updateSteering() {
  float currentAngle = encoderCalib.readAngle(encoder);
  float diff = targetAngle - currentAngle;
  if (diff > 180) diff -= 360;
  if (diff < -180) diff += 360;
  if (abs(diff) > settings.get("angleTolerance")) {
    int steps = map(abs(diff), 0, 180, 0, STEPS_PER_REV);
    stepper.moveTo(stepper.currentPosition() + (diff > 0 ? steps : -steps));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n[BoatLock] ESP32 стартует! Версия прошивки: 0.1.0");

  Wire.begin(8, 9);

  pinMode(BOOT_PIN, INPUT_PULLUP);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  gpsSerial.begin(9600, SERIAL_8N1, 17, 18);

  bleBoatLock.begin();

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
  bleBoatLock.registerParam("lat",      makeFloatParam([&](){ return gps.location.lat(); }, "%.6f"));
  bleBoatLock.registerParam("lon",      makeFloatParam([&](){ return gps.location.lng(); }, "%.6f"));
  bleBoatLock.registerParam("heading",  makeFloatParam([&](){ return 17; }, "%.1f"));
  bleBoatLock.registerParam("anchorLat",makeFloatParam([&](){ return anchor.anchorLat; }, "%.6f"));
  bleBoatLock.registerParam("anchorLon",makeFloatParam([&](){ return anchor.anchorLng; }, "%.6f"));

  EEPROM.begin(EEPROM_SIZE);

  settings.load();
  anchor.attachSettings(&settings);
  anchor.loadAnchor();
  drawDebug("settings load");

  encoderCalib.setSettings(&settings);    
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  motor.setupPWM(MOTOR_PWM_PIN, PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  motor.setDirPin(MOTOR_DIR_PIN);
  drawDebug("motor load");

  // display.clearDisplay();
  // display.display();

  // bno.begin();
  // encoder.begin();
  drawDebug("encoder begin");
//   encoderCalib.loadOffset();
  // stepper.setMaxSpeed(1000);
  // stepper.setAcceleration(500);

  // --- Anchor ---

  if(settings.get("AnchorEnabled") == 1) {
    anchorSet = true;
  }

  // boatIMU.begin();

  // scanI2CAndDisplay();
  // delay(3)

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

  if (gps.location.isValid() && settings.get("AnchorEnabled")==1) {
    dist = anchor.distanceToAnchor(gps);
    bearing = anchor.bearingToAnchor(gps);
  }

  // compass.update();
  // updateSteering();
  // stepper.run();

  // encoderAngle = encoderCalib.readAngle(encoder);

  // if (holding && gps.location.isValid()) {
  //   float dist = anchor.distanceToAnchor(gps);
  //   if (dist > settings.get("distanceThreshold")) {
  //     float bearing = anchor.bearingToAnchor(gps);
  //     adjustPosition(bearing);
  //     // motor.applyPID(dist);
  //     setMotorRaw(50, 0);
  //   } else {
  //     motor.stop();
  //   }
  // } else {
  //   motor.stop();
  // }

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
                17, // Heading
                holding
            );
    }

  bleBoatLock.loop();
  // boatIMU.update();
  //   Serial.printf("A: %.2f %.2f %.2f | G: %.2f %.2f %.2f | M: %.2f %.2f %.2f | Heading: %.1f | Temp: %.1f\n",
  //       boatIMU.accelX(), boatIMU.accelY(), boatIMU.accelZ(),
  //       boatIMU.gyroX(), boatIMU.gyroY(), boatIMU.gyroZ(),
  //       boatIMU.magX(), boatIMU.magY(), boatIMU.magZ());
        // boatIMU.heading(),));
  // delay(500); // для теста
}
