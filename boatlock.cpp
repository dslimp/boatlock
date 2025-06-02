// Virtual Anchor - ESP32-S3, OLED, GPS, EEPROM, Stepper (SERVO42C w/ Closed Loop), Compass, AS5600, Trolling Motor PWM + PID
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

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BUTTON_PIN 0
#define EEPROM_ADDR 0

// Stepper motor pins and parameters (SERVO42C driver)
#define STEP_PIN 2
#define DIR_PIN 3
const int STEPS_PER_REV = 200; // SERVO42C default steps/rev
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// GT2 pulley parameters
const int MOTOR_TEETH = 16;
const int LOAD_TEETH = 120;
const float GEAR_RATIO = (float)LOAD_TEETH / MOTOR_TEETH; // 7.5:1
const int EFFECTIVE_STEPS_PER_REV = STEPS_PER_REV * GEAR_RATIO;

// Trolling motor control
#define MOTOR_PWM_PIN 5
#define MOTOR_DIR_PIN 6
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;

// PID parameters for thrust control
float Kp = 20.0;
float Ki = 0.5;
float Kd = 5.0;
float previous_error = 0;
float integral = 0;

// Position control parameters
const float DISTANCE_THRESHOLD = 2.0; // meters
const float ANGLE_TOLERANCE = 3.0; // degrees

HardwareSerial gpsSerial(1);
TinyGPSPlus gps;
Adafruit_BNO055 bno = Adafruit_BNO055(55);
AS5600 encoder;

float anchorLat = 0;
float anchorLng = 0;
bool holding = false;
float targetAngle = 0;

void saveAnchor() {
  EEPROM.put(EEPROM_ADDR, anchorLat);
  EEPROM.put(EEPROM_ADDR + sizeof(float), anchorLng);
  EEPROM.commit();
}

void loadAnchor() {
  EEPROM.get(EEPROM_ADDR, anchorLat);
  EEPROM.get(EEPROM_ADDR + sizeof(float), anchorLng);
}

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  EEPROM.begin(512);
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
  loadAnchor();
}

void drawStatus() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Lat: "); display.println(gps.location.lat(), 6);
  display.print("Lng: "); display.println(gps.location.lng(), 6);
  display.print("Mode: "); display.println(holding ? "HOLDING" : "IDLE");
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

int readEncoderAngle() {
  return encoder.rawAngle() * 360.0 / 4096.0;
}

void adjustPosition(float bearing, float heading) {
  targetAngle = bearing;
}

void updateSteering() {
  float currentAngle = encoder.rawAngle() * 360.0 / 4096.0;
  float diff = targetAngle - currentAngle;

  if (diff > 180) diff -= 360;
  if (diff < -180) diff += 360;

  if (abs(diff) > ANGLE_TOLERANCE) {
    int steps = map(abs(diff), 0, 180, 0, EFFECTIVE_STEPS_PER_REV);
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
  digitalWrite(MOTOR_DIR_PIN, HIGH); // всегда вперед на удержание
  ledcWrite(MOTOR_PWM_PIN, pwmValue);
}

void loop() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  updateSteering();
  stepper.run();

  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(300);
    if (!holding) {
      anchorLat = gps.location.lat();
      anchorLng = gps.location.lng();
      saveAnchor();
      holding = true;
    } else {
      holding = false;
      ledcWrite(MOTOR_PWM_PIN, 0);
    }
  }

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
