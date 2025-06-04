#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <EEPROM.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <AS5600.h>
#include <AccelStepper.h>

// --- твои собственные либы ---
#include "Settings.h"
Settings settings;
#include "MPUCompass.h"
#include "AnchorControl.h"
#include "EncoderCalib.h"
#include "MotorControl.h"


// --- Дисплей ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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

bool holding = false;
float targetAngle = 0;
float encoderAngle = 0;
unsigned long lastDraw = 0;
const unsigned long drawInterval = 100;

// --- BLE ---
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;
#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHAR_UUID    "abcd1234-1234-1234-1234-abcdefabcdef"

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override { deviceConnected = true; }
  void onDisconnect(BLEServer* pServer) override { deviceConnected = false; }
};

// --- Рисуем статус ---
void drawStatus() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Lat: "); display.println(gps.location.lat(), 6);
  display.print("Lng: "); display.println(gps.location.lng(), 6);
  display.print("Dist: "); display.println(anchor.distanceToAnchor(gps), 1);
  display.print("Bear: "); display.println(anchor.bearingToAnchor(gps), 1);
  display.print("Head: "); display.println(compass.getHeading(), 1);
  display.print("Mode: "); display.println(holding ? "HOLDING" : "IDLE");
  display.setCursor(0, 56);
  display.print("Encoder: "); display.print(encoderAngle, 1); display.print(" deg");
  display.display();
}

// --- Управление положением ---
void adjustPosition(float bearing) {
  targetAngle = bearing;
}

// --- Поворот моторчика ---
void updateSteering() {
  float currentAngle = encoderCalib.readAngle(encoder);
  float diff = targetAngle - currentAngle;
  if (diff > 180) diff -= 360;
  if (diff < -180) diff += 360;
  if (abs(diff) > settings.data.angleTolerance) {
    int steps = map(abs(diff), 0, 180, 0, STEPS_PER_REV);
    stepper.moveTo(stepper.currentPosition() + (diff > 0 ? steps : -steps));
  }
}

void handleBLE() {
  if (deviceConnected) {
    String rxValue = String(pCharacteristic->getValue().c_str());
    if (rxValue.length() > 0) {
      String input = String(rxValue.c_str());
      if (input.startsWith("Kp:")) settings.data.Kp = input.substring(3).toFloat();
      else if (input.startsWith("Ki:")) settings.data.Ki = input.substring(3).toFloat();
      else if (input.startsWith("Kd:")) settings.data.Kd = input.substring(3).toFloat();
      else if (input.startsWith("dist:")) settings.data.distanceThreshold = input.substring(5).toFloat();
      else if (input.startsWith("angle:")) settings.data.angleTolerance = input.substring(6).toFloat();
      pCharacteristic->setValue("Settings updated");
    }
  }
}

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  EEPROM.begin(512);

  // --- Инициализация железа ---
  if (!compass.begin(0x68, 100.0f)) {
    Serial.println("MPU-9250 not found!");
    while (1);
  }

  settings.load();

  encoderCalib.setSettings(&settings);    
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  motor.setupPWM(MOTOR_PWM_PIN, PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  motor.setDirPin(MOTOR_DIR_PIN);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  bno.begin();
  encoder.begin();
//   encoderCalib.loadOffset();
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(500);

  // --- BLE ---
  BLEDevice::init("VirtualAnchor");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
      CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristic->setValue("Ready");
  pService->start();
  pServer->getAdvertising()->start();

  // --- Anchor ---
  anchor.loadAnchor();
}

void loop() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  compass.update();
  handleBLE();
  updateSteering();
  stepper.run();

  // --- Калибровка энкодера по кнопке ---
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(300);
    encoderCalib.calibrate(encoder);
    while (digitalRead(BUTTON_PIN) == LOW) delay(10);
  }

  encoderAngle = encoderCalib.readAngle(encoder);

  if (millis() - lastDraw > drawInterval) {
    drawStatus();
    lastDraw = millis();
  }

  // --- Управление якорем и мотором ---
  if (holding && gps.location.isValid()) {
    float dist = anchor.distanceToAnchor(gps);
    if (dist > settings.data.distanceThreshold) {
      float bearing = anchor.bearingToAnchor(gps);
      adjustPosition(bearing);
      motor.applyPID(dist);
    } else {
      motor.stop();
    }
  } else {
    motor.stop();
  }

}
