#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <EEPROM.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <AS5600.h>
#include <AccelStepper.h>
#include <math.h>

#include "Settings.h"
Settings settings;
#include "MPUCompass.h"
#include "AnchorControl.h"
#include "EncoderCalib.h"
#include "MotorControl.h"

#include "BoatDisplay.h"
// #include "RemoteControl.h"
// RemoteControlServer remote;

unsigned long lastNotifyBle = 0;

#include "BLEBoatLock.h"
BLEBoatLock bleBoatLock;



float fakeDistance = 15.0;
int fakeBattery = 89;
std::string fakeStatus = "HOLD";

#define US_TRIG  13
#define US_ECHO  12

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
MPUCompass compass;
AnchorControl anchor;
EncoderCalib encoderCalib;
MotorControl motor;


#define BOOT_PIN 0

float anchorLat = 0;
float anchorLon = 0;
bool anchorSet = false;


bool holding = false;
float targetAngle = 0;
float encoderAngle = 0;
unsigned long lastDraw = 0;
const unsigned long drawInterval = 100;
// float anchorLat, anchorLon = 0;
float dist = 0, bearing = 0;

const int windowSize = 10;
float latBuffer[windowSize];
float lonBuffer[windowSize];
int idx = 0;

float distanceAnchor=0;

#define CX 64
#define CY 54    // ниже, чтобы не мешать тексту
#define R  10    // длина стрелки

float lastLat = 0, lastLon = 0;
float prevBearing = 0;

void drawArrow(float bearing) {
  float rad = (bearing - 90) * DEG_TO_RAD; // -90 чтобы 0° был вверх
  int x2 = CX + R * cos(rad);
  int y2 = CY + R * sin(rad);
  display.drawLine(CX, CY, x2, y2, SSD1306_WHITE);
  int x_head1 = CX + (R - 3) * cos(rad + 0.3);
  int y_head1 = CY + (R - 3) * sin(rad + 0.3);
  int x_head2 = CX + (R - 3) * cos(rad - 0.3);
  int y_head2 = CY + (R - 3) * sin(rad - 0.3);
  display.drawLine(x2, y2, x_head1, y_head1, SSD1306_WHITE);
  display.drawLine(x2, y2, x_head2, y_head2, SSD1306_WHITE);
}


void setMotorRaw(int power, bool dir) {
    digitalWrite(MOTOR_DIR_PIN, dir ? HIGH : LOW);
    ledcWrite(PWM_CHANNEL, constrain(power, 0, 255));
}

void drawDebug(const String &msg, int y = 48) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,  48);
  display.print("[D]");
  display.print(msg);
  display.display();
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

  // // ble.begin(getBoatData, handleBoatCmd);
  //   auto dataFunc = []() -> std::string {
  //       return "{\"battery\":89}";
  //   };
  //   // Колбэк: обработать команду от клиента
  //   auto cmdFunc = [](const std::string& cmd) {
  //       Serial.print("[BLE] Got cmd: ");
  //       Serial.println(cmd.c_str());
  //       // Здесь обработка команд
  //   };
  //   bleBoatLock.begin(dataFunc, cmdFunc);
  bleBoatLock.begin();

  bleBoatLock.registerParam("distance", [&]() {
      char buf[16];
      snprintf(buf, sizeof(buf), "%.2f", dist);
      return std::string(buf);
  });


  bleBoatLock.registerParam("heading", []() { return "91"; });
  bleBoatLock.setCommandHandler([](const std::string& cmd) {
      Serial.printf("[BLE] Unhandled command: %s\n", cmd.c_str());
  });

  bleBoatLock.registerParam("lat", [&]() {
      char buf[16];
      snprintf(buf, sizeof(buf), "%.6f", gps.location.lat());
      return std::string(buf);
  });
  bleBoatLock.registerParam("lon", [&]() {
      char buf[16];
      snprintf(buf, sizeof(buf), "%.6f", gps.location.lng());
      return std::string(buf);
  });
  bleBoatLock.registerParam("heading", [&]() {
      char buf[8];
      snprintf(buf, sizeof(buf), "%.1f", compass.getHeading());
      return std::string(buf);
  });

  bleBoatLock.registerParam("anchorLat", [&]() {
      char buf[16];
      snprintf(buf, sizeof(buf), "%.6f", anchorLat);
      return std::string(buf);
  });
  bleBoatLock.registerParam("anchorLon", [&]() {
      char buf[16];
      snprintf(buf, sizeof(buf), "%.6f", anchorLon);
      return std::string(buf);
  });

  EEPROM.begin(512);
  if (!compass.begin(0x68, 100.0f)) {
    drawDebug("MPU-9250 not found!");
    // while (1);
  }

  settings.load();
  drawDebug("settings load");

  encoderCalib.setSettings(&settings);    
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  motor.setupPWM(MOTOR_PWM_PIN, PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  motor.setDirPin(MOTOR_DIR_PIN);
  drawDebug("motor load");

  display.clearDisplay();
  display.display();

  // bno.begin();
  // encoder.begin();
  drawDebug("encoder begin");
//   encoderCalib.loadOffset();
  // stepper.setMaxSpeed(1000);
  // stepper.setAcceleration(500);

  // --- Anchor ---
  anchor.loadAnchor();
}

void loop() {

  static bool lastButton = HIGH;
  bool nowButton = digitalRead(BOOT_PIN);

  if (lastButton == HIGH && nowButton == LOW) {
    if (gps.location.isValid()) {
      anchorLat = gps.location.lat();
      anchorLon = gps.location.lng();
      anchorSet = true;
      Serial.println("Anchor point set!");
    }
  }
  lastButton = nowButton;

  float lat = gps.location.lat();
  float lon = gps.location.lng();

  
  if (anchorSet && gps.location.isValid()) {
    dist = TinyGPSPlus::distanceBetween(lat, lon, anchorLat, anchorLon); // в метрах
    bearing = TinyGPSPlus::courseTo(lat, lon, anchorLat, anchorLon);    // в градусах
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
  //   while (gpsSerial.available()) {
  //   char c = gpsSerial.read();
  //   Serial.write(c); // просто пересылаем данные с GPS в монитор порта
  // }
  // delay(10);
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
                anchorLat, anchorLon, anchorSet,
                dist, bearing,
                compass.getHeading(),
                holding,
                gps.speed.kmph()
            );
    }

  bleBoatLock.loop();
}
