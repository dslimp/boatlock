// Virtual Anchor - ESP32-S3, OLED, GPS, EEPROM, Stepper (AccelStepper), Compass, AS5600, Trolling Motor PWM + PID, BLE Settings

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
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "MPUCompass.h"
MPUCompass compass;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BUTTON_PIN 0
#define EEPROM_ADDR 0
#define ENCODER_OFFSET_ADDR 100 // EEPROM offset для angle_offset

// Stepper motor pins and parameters
#define STEP_PIN 2
#define DIR_PIN 3
const int STEPS_PER_REV = 200;
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// Trolling motor control
#define MOTOR_PWM_PIN 5
#define MOTOR_DIR_PIN 6
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;

// PID parameters for thrust control (configurable)
float Kp = 20.0;
float Ki = 0.5;
float Kd = 5.0;
float previous_error = 0;
float integral = 0;

// Position control parameters (configurable)
float DISTANCE_THRESHOLD = 2.0; // meters
float ANGLE_TOLERANCE = 3.0; // degrees

HardwareSerial gpsSerial(1);
TinyGPSPlus gps;
Adafruit_BNO055 bno = Adafruit_BNO055(55);
AS5600 encoder;

// --- Новые переменные для калибровки угла ---
uint16_t angle_offset = 0;      // Значение энкодера для нулевой позиции
float encoderAngle = 0.0;       // Скорректированный угол (0–360)

float anchorLat = 0;
float anchorLng = 0;
bool holding = false;
float targetAngle = 0;

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;

#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHAR_UUID    "abcd1234-1234-1234-1234-abcdefabcdef"

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) { deviceConnected = true; }
  void onDisconnect(BLEServer* pServer) { deviceConnected = false; }
};

void saveAnchor() {
  EEPROM.put(EEPROM_ADDR, anchorLat);
  EEPROM.put(EEPROM_ADDR + sizeof(float), anchorLng);
  EEPROM.commit();
}

void loadAnchor() {
  EEPROM.get(EEPROM_ADDR, anchorLat);
  EEPROM.get(EEPROM_ADDR + sizeof(float), anchorLng);
}

// --- Калибровка энкодера (записать текущий угол как "0") ---
void calibrateEncoder() {
  angle_offset = encoder.rawAngle();
  EEPROM.put(ENCODER_OFFSET_ADDR, angle_offset);
  EEPROM.commit();
  Serial.print("AS5600 zero calibrated, offset: "); Serial.println(angle_offset);
}

// --- Загрузка offset из EEPROM при старте ---
void loadEncoderOffset() {
  EEPROM.get(ENCODER_OFFSET_ADDR, angle_offset);
}

// --- Получить калиброванный угол энкодера (0–360°) ---
float readEncoderAngle() {
  int32_t raw = encoder.rawAngle();
  int32_t delta = raw - angle_offset;
  if (delta < 0) delta += 4096;
  float angle = (float)delta * 360.0 / 4096.0;
  return angle;
}

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  EEPROM.begin(512);

  if (!compass.begin(0x68, 100.0f)) {
    Serial.println("MPU-9250 not found!");
    while (1);
  }

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MOTOR_DIR_PIN, OUTPUT);

  ledcAttach(MOTOR_PWM_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcWrite(MOTOR_PWM_PIN, 0);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  bno.begin();
  encoder.begin();
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(500);

  BLEDevice::init("VirtualAnchor");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                     CHAR_UUID,
                     BLECharacteristic::PROPERTY_READ |
                     BLECharacteristic::PROPERTY_WRITE
                   );
  pCharacteristic->setValue("Ready");
  pService->start();
  pServer->getAdvertising()->start();

  loadAnchor();
  loadEncoderOffset();
}

void drawStatus() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Lat: "); display.println(gps.location.lat(), 6);
  display.print("Lng: "); display.println(gps.location.lng(), 6);
  display.print("Dist: "); display.println(distanceToAnchor(), 1);
  display.print("Bear: "); display.println(bearingToAnchor(), 1);
  display.print("Head: "); display.println(currentHeading(), 1);
  display.print("Mode: "); display.println(holding ? "HOLDING" : "IDLE");
  display.setCursor(0, 56);
  display.print("Encoder: ");
  display.print(encoderAngle, 1); display.print(" deg");
  display.display();
}

float distanceToAnchor() {
  return TinyGPSPlus::distanceBetween(
    gps.location.lat(), gps.location.lng(), anchorLat, anchorLng);
}

float bearingToAnchor() {
  return TinyGPSPlus::courseTo(
    gps.location.lat(), gps.location.lng(), anchorLat, anchorLng);
}

float currentHeading() {
  imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
  return euler.x();
}

void adjustPosition(float bearing, float heading) {
  targetAngle = bearing;
}

void updateSteering() {
  float currentAngle = readEncoderAngle();
  float diff = targetAngle - currentAngle;

  if (diff > 180) diff -= 360;
  if (diff < -180) diff += 360;

  if (abs(diff) > ANGLE_TOLERANCE) {
    int steps = map(abs(diff), 0, 180, 0, STEPS_PER_REV);
    stepper.moveTo(stepper.currentPosition() + (diff > 0 ? steps : -steps));
  }
}

void applyThrustPID(float dist) {
  float error = dist;
  integral += error;
  float derivative = error - previous_error;
  float output = Kp * error + Ki * integral + Kd * derivative;
  previous_error = error;

  int pwmValue = constrain((int)output, 0, 255);
  digitalWrite(MOTOR_DIR_PIN, HIGH);
  ledcWrite(MOTOR_PWM_PIN, pwmValue);
}

void handleBLE() {
  if (deviceConnected) {
    String rxValue = String(pCharacteristic->getValue().c_str());
    if (rxValue.length() > 0) {
      String input = String(rxValue.c_str());
      if (input.startsWith("Kp:")) Kp = input.substring(3).toFloat();
      else if (input.startsWith("Ki:")) Ki = input.substring(3).toFloat();
      else if (input.startsWith("Kd:")) Kd = input.substring(3).toFloat();
      else if (input.startsWith("dist:")) DISTANCE_THRESHOLD = input.substring(5).toFloat();
      else if (input.startsWith("angle:")) ANGLE_TOLERANCE = input.substring(6).toFloat();
      pCharacteristic->setValue("Settings updated");
    }
  }
}

void loop() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  handleBLE();
  updateSteering();
  stepper.run();

  // --- Калибровка энкодера по кнопке ---
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(300);
    calibrateEncoder(); // Зафиксировать "ноль" в текущем положении
    while (digitalRead(BUTTON_PIN) == LOW) delay(10); // Ждать отпускания
  }

  encoderAngle = readEncoderAngle();

  drawStatus();

  if (holding && gps.location.isValid()) {
    float dist = distanceToAnchor();
    if (dist > DISTANCE_THRESHOLD) {
      float bearing = bearingToAnchor();
      float heading = currentHeading();
      adjustPosition(bearing, heading);
      applyThrustPID(dist);
    } else {
      ledcWrite(MOTOR_PWM_PIN, 0);
    }
  } else {
    ledcWrite(MOTOR_PWM_PIN, 0);
  }

  delay(100);
}
