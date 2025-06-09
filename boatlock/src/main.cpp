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
#include "UltrasonicAlert.h"
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
UltrasonicAlert noseAlert;

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

String getBoatData() {
  String payload = String("Lat:") + String(gps.location.lat(), 6) +
                  ",Lon:" + String(gps.location.lng(), 6) +
                  ",Spd:" + String(gps.speed.kmph()) +
                  ",Sat:" + String(gps.satellites.value());
  return payload;
}

void handleBoatCmd(const String &cmd) {
  Serial.print("BLE command: "); Serial.println(cmd);
  if (cmd == "HOLD") holding = true;
  if (cmd == "STOP") holding = false;
  // и т.д.
}


void updateAvg(float lat, float lon) {
  latBuffer[idx] = lat;
  lonBuffer[idx] = lon;
  idx = (idx + 1) % windowSize;
}


float avgLat() {
  float sum = 0;
  for (int i = 0; i < windowSize; i++) sum += latBuffer[i];
  return sum / windowSize;
}

float avgLon() {
  float sum = 0;
  for (int i = 0; i < windowSize; i++) sum += lonBuffer[i];
  return sum / windowSize;
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
  delay(100);
  display.display();
}


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

  display.print("US: ");
  display.print(noseAlert.lastDistance(), 2);
  display.print(" m");
  if (noseAlert.isAlert()) display.print(" ALERT!");

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

  // byte error, address;
  // int nDevices = 0;

  // for(address = 1; address < 127; address++ ) {
  //   Wire.beginTransmission(address);
  //   error = Wire.endTransmission();
  //   if (error == 0) {
  //     Serial.print("I2C device found at address 0x");
  //     if (address<16)
  //       Serial.print("0");
  //     Serial.print(address,HEX);
  //     Serial.println(" !");
  //     nDevices++;
  //   } else if (error==4) {
  //     Serial.print("Unknown error at address 0x");
  //     if (address<16)
  //       Serial.print("0");
  //     Serial.println(address,HEX);
  //   }    
  // }
  // if (nDevices == 0)
  //   Serial.println("No I2C devices found\n");
  // else
  //   Serial.println("done\n");
// }

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed!");
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Hello!");

  // drawDebug("ESP32 started");

  // display.display();

 
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
  // drawDebug("bno begin");
  // encoder.begin();
  drawDebug("encoder begin");
  // noseAlert.setPins(US_TRIG, US_ECHO);
  // noseAlert.setSettings(&settings);
  // noseAlert.begin();
//   encoderCalib.loadOffset();
  // stepper.setMaxSpeed(1000);
  // stepper.setAcceleration(500);

  // --- Anchor ---
  // anchor.loadAnchor();
}

void loop() {

  static bool lastButton = HIGH;
  bool nowButton = digitalRead(BOOT_PIN);

  if (lastButton == HIGH && nowButton == LOW) { // Только на нажатие
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

  // --- ОТРИСОВКА ---
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.printf("L:%.5f\nN:%.5f\n", lat, lon);
  display.printf("Sat:%d", gps.satellites.value());
  display.print(" HDOP: ");
  display.print(gps.hdop.value() * 0.01);


  display.setCursor(0, 30);
  if (anchorSet) {
    display.printf("Dst:%.1fm\n", dist);
    display.printf("Brg:%.0f%c\n", bearing, 176);
    drawArrow(bearing); // Функция рисует стрелку на дисплее (см. ваш drawArrow)
  } else {
    display.println("Press BOOT to anchor");
  }

  display.display();
  // while (gpsSerial.available()) {
  //   gps.encode(gpsSerial.read());
  // }

  // compass.update();
  // updateSteering();
  // stepper.run();

  // if (digitalRead(BUTTON_PIN) == LOW) {
  //   delay(300);
  //   encoderCalib.calibrate(encoder);
  //   while (digitalRead(BUTTON_PIN) == LOW) delay(10);
  // }

  // encoderAngle = encoderCalib.readAngle(encoder);

  // if (millis() - lastDraw > drawInterval) {
  //   drawStatus();
  //   lastDraw = millis();
  // }

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


  // fakeDistance += 0.02;
  // if (fakeDistance > 20.0) fakeDistance = 10.0;
  // bleBoatLock.setDistance(fakeDistance);

  // fakeBattery -= 1;
  // if (fakeBattery < 80) fakeBattery = 99;
  // bleBoatLock.setBattery(fakeBattery);

  // // fakeStatus = ... (обновляй как хочешь)
  // bleBoatLock.setStatus(fakeStatus);

    static unsigned long lastNotify = 0;
    unsigned long now = millis();
    if (now - lastNotify > 5000) {
        bleBoatLock.notifyAll();
        lastNotify = now;
    }

  bleBoatLock.loop();

  // if (gps.location.isValid()) {

  // display.clearDisplay();
  // display.setTextSize(1);
  // display.setCursor(0, 0);

  //  float lat = gps.location.lat();
  //   float lon = gps.location.lng();
  //   display.printf("L:%.5f\nN:%.5f\n", lat, lon);

  //   display.printf("S:%d", gps.satellites.value());
  //   display.setCursor(60, 0);
  //   display.printf("H:%.2f", gps.hdop.value() * 0.01);

  //   display.setCursor(0, 16);
  //   display.printf("V:%.1f km/h", gps.speed.kmph());

  //   display.setCursor(70, 16);
  //   display.printf("sat:%.1i", gps.satellites.value());

  //   // display.setCursor(0, 30);

  //   float dist = 0;
  //   float bearing = 0;
  //   if (lastLat != 0 && lastLon != 0) {
  //     dist = TinyGPSPlus::distanceBetween(lastLat, lastLon, lat, lon);
  //     bearing = TinyGPSPlus::courseTo(lastLat, lastLon, lat, lon);
  //     prevBearing = bearing; // для отрисовки стрелки даже если dist=0
  //   }
  //   display.setCursor(0, 28);
  //   display.printf("d:%.1fm", dist);

  //   display.setCursor(40, 28);
  //   // display.printf("hdop:%i", gps.hdop.value() * 0.01);
  //   // display.setCursor(0, 50);
  //   display.print("HDOP: ");
  //   display.println(gps.hdop.value() * 0.01);

  //   display.setCursor(0, 50);
  //   display.print("BLE: ");
  //   display.print(bleBoatLock.statusString());

  //   // Сохраняем предыдущую точку для следующего шага
  //   lastLat = lat;
  //   lastLon = lon;

  //   // Нарисовать стрелку только если есть валидное расстояние
  //   if (dist > 1) drawArrow(bearing);
  //   else if (prevBearing != 0) drawArrow(prevBearing);

  //   display.display();

  // } else {
  //   display.println("Waiting for GPS...");
  // }

  // display.display();

  // delay(1000);

//   static BLEBoatLock::Status last = BLEBoatLock::ADVERTISING;
//   if (bleBoatLock.getStatus() != last) {
//     last = bleBoatLock.getStatus();
//     Serial.printf("[BLE] Status changed: %s\n", bleBoatLock.statusString());
// }
}
