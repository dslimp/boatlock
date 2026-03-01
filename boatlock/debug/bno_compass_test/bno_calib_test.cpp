#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_BNO08x.h>
#include "display.h"

namespace cfg {
#if defined(BOATLOCK_BOARD_JC4832W535)
constexpr int kI2cSdaPin = 4;
constexpr int kI2cSclPin = 8;
#else
constexpr int kI2cSdaPin = 47;
constexpr int kI2cSclPin = 48;
#endif
constexpr uint32_t kI2cFreqHz = 100000;
constexpr uint32_t kRotationReportUs = 20000;
constexpr uint32_t kAuxReportUs = 100000;
}

Adafruit_BNO08x bno(-1);
bool displayReady = false;
bool bnoReady = false;
uint8_t bnoAddr = 0;

float headingDeg = 0.0f;
float pitchDeg = 0.0f;
float rollDeg = 0.0f;
float rvAccDeg = 0.0f;
uint8_t rvQuality = 0;
float qW = 0.0f;
float qX = 0.0f;
float qY = 0.0f;
float qZ = 0.0f;

float accelX = 0.0f;
float accelY = 0.0f;
float accelZ = 0.0f;
float accelNorm = 0.0f;
uint8_t accelQuality = 0;

float linAccX = 0.0f;
float linAccY = 0.0f;
float linAccZ = 0.0f;
float linAccNorm = 0.0f;
uint8_t linAccQuality = 0;

float gravX = 0.0f;
float gravY = 0.0f;
float gravZ = 0.0f;
float gravNorm = 0.0f;
uint8_t gravQuality = 0;

float magX = 0.0f;
float magY = 0.0f;
float magZ = 0.0f;
float magNorm = 0.0f;
uint8_t magQuality = 0;

float gyroX = 0.0f;
float gyroY = 0.0f;
float gyroZ = 0.0f;
float gyroNormDps = 0.0f;
uint8_t gyroQuality = 0;

uint16_t stepCount = 0;
uint8_t tapFlags = 0;
uint8_t stabilityClass = STABILITY_CLASSIFIER_UNKNOWN;
uint8_t activityState = PAC_UNKNOWN;
uint8_t activityConfidence = 0;
bool dcdSaved = false;
bool dcdAutoSaveEnabled = false;
bool refSet = false;
float refHeadingDeg = 0.0f;

unsigned long lastUiMs = 0;
unsigned long lastLogMs = 0;

const char* qualityText(uint8_t q) {
  switch (q & 0x03) {
    case 0: return "unreliable";
    case 1: return "low";
    case 2: return "medium";
    case 3: return "high";
    default: return "?";
  }
}

const char* stabilityText(uint8_t cls) {
  switch (cls) {
    case STABILITY_CLASSIFIER_ON_TABLE: return "table";
    case STABILITY_CLASSIFIER_STATIONARY: return "stationary";
    case STABILITY_CLASSIFIER_STABLE: return "stable";
    case STABILITY_CLASSIFIER_MOTION: return "motion";
    case STABILITY_CLASSIFIER_UNKNOWN:
    default: return "unknown";
  }
}

const char* activityText(uint8_t state) {
  switch (state) {
    case PAC_IN_VEHICLE: return "vehicle";
    case PAC_ON_BICYCLE: return "bicycle";
    case PAC_ON_FOOT: return "on_foot";
    case PAC_STILL: return "still";
    case PAC_TILTING: return "tilting";
    case PAC_WALKING: return "walking";
    case PAC_RUNNING: return "running";
    case PAC_UNKNOWN:
    default: return "unknown";
  }
}

float wrap360(float deg) {
  while (deg < 0.0f) deg += 360.0f;
  while (deg >= 360.0f) deg -= 360.0f;
  return deg;
}

float angleDeltaDeg(float fromDeg, float toDeg) {
  float d = wrap360(toDeg) - wrap360(fromDeg);
  if (d > 180.0f) d -= 360.0f;
  if (d < -180.0f) d += 360.0f;
  return d;
}

void drawQualityBar(Arduino_GFX* gfx, int x, int y, const char* name, uint8_t q) {
  gfx->setTextSize(1);
  gfx->setCursor(x, y);
  gfx->printf("%s", name);
  const int bx = x + 28;
  const int by = y + 2;
  const int segW = 10;
  const int segH = 8;
  const int gap = 3;
  for (int i = 0; i < 3; ++i) {
    const int sx = bx + i * (segW + gap);
    const bool on = ((int)q >= (i + 1));
    gfx->fillRect(sx, by, segW, segH, on ? 0x07E0 : 0xC618);
    gfx->drawRect(sx, by, segW, segH, 0x0000);
  }
}

void printI2cDevices() {
  uint8_t found[32] = {0};
  size_t count = 0;
  for (uint8_t addr = 1; addr < 127 && count < sizeof(found); ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      found[count++] = addr;
    }
  }

  Serial.printf("[I2C] SDA=%d SCL=%d -> %u device(s)", cfg::kI2cSdaPin, cfg::kI2cSclPin, (unsigned)count);
  if (count == 0) {
    Serial.print(": none\n");
    return;
  }
  Serial.print(":");
  for (size_t i = 0; i < count; ++i) {
    Serial.printf(" 0x%02X", found[i]);
  }
  Serial.print("\n");
}

bool initBnoAt(uint8_t addr) {
  if (!bno.begin_I2C(addr, &Wire, -1)) {
    return false;
  }
  // Enable runtime calibration for all relevant sensors.
  sh2_setCalConfig(SH2_CAL_ACCEL | SH2_CAL_GYRO | SH2_CAL_MAG);
  dcdAutoSaveEnabled = (sh2_setDcdAutoSave(true) == SH2_OK);
  if (!bno.enableReport(SH2_ROTATION_VECTOR, cfg::kRotationReportUs)) {
    return false;
  }
  bool ok = true;
  ok &= bno.enableReport(SH2_ACCELEROMETER, cfg::kAuxReportUs);
  ok &= bno.enableReport(SH2_LINEAR_ACCELERATION, cfg::kAuxReportUs);
  ok &= bno.enableReport(SH2_GRAVITY, cfg::kAuxReportUs);
  ok &= bno.enableReport(SH2_MAGNETIC_FIELD_CALIBRATED, cfg::kAuxReportUs);
  ok &= bno.enableReport(SH2_GYROSCOPE_CALIBRATED, cfg::kAuxReportUs);
  ok &= bno.enableReport(SH2_STABILITY_CLASSIFIER, 200000);
  ok &= bno.enableReport(SH2_PERSONAL_ACTIVITY_CLASSIFIER, 200000);
  ok &= bno.enableReport(SH2_STEP_COUNTER, 200000);
  ok &= bno.enableReport(SH2_TAP_DETECTOR, 100000);
  return ok;
}

void drawUi() {
  if (!displayReady) {
    return;
  }
  Arduino_GFX* gfx = display_gfx();
  gfx->fillScreen(0xFFFF);
  const int w = gfx->width();
  const int h = gfx->height();
  const int rightPanelW = 86;
  const int leftW = w - rightPanelW;

  const float deltaDeg = refSet ? angleDeltaDeg(refHeadingDeg, headingDeg) : 0.0f;

  gfx->setTextColor(0x0000);
  gfx->setTextSize(1);
  gfx->setCursor(2, 2);
  gfx->printf("BNO08x 0x%02X %s", bnoAddr, dcdAutoSaveEnabled ? (dcdSaved ? "AUTO+CAL" : "AUTO") : "OFF");
  gfx->setCursor(leftW - 62, 2);
  gfx->printf("ref:%3.0f", refSet ? refHeadingDeg : 0.0f);

  const int cx = leftW / 2;
  const int cy = h / 2 - 6;
  int r = (leftW < h ? leftW : h) / 2 - 24;
  if (r < 28) r = 28;
  gfx->drawCircle(cx, cy, r, 0x0000);
  gfx->drawCircle(cx, cy, r - 1, 0x0000);
  gfx->setCursor(cx - 3, cy - r - 10);
  gfx->print("N");

  const float rad = (headingDeg - 90.0f) * DEG_TO_RAD;
  const int tipX = cx + (int)((r - 4) * cosf(rad));
  const int tipY = cy + (int)((r - 4) * sinf(rad));
  const int tailX = cx - (int)((r - 10) * cosf(rad));
  const int tailY = cy - (int)((r - 10) * sinf(rad));
  gfx->drawLine(cx, cy, tipX, tipY, 0xF800);
  gfx->drawLine(cx, cy, tailX, tailY, 0x001F);
  gfx->fillCircle(tipX, tipY, 4, 0xF800);
  gfx->fillCircle(cx, cy, 3, 0x0000);

  gfx->setTextSize(2);
  gfx->setCursor(8, h - 48);
  gfx->printf("%03.0f", headingDeg);
  gfx->setTextSize(1);
  gfx->setCursor(72, h - 40);
  gfx->print("deg");

  gfx->setTextSize(2);
  gfx->setCursor(104, h - 48);
  gfx->printf("%+04.0f", deltaDeg);
  gfx->setTextSize(1);
  gfx->setCursor(168, h - 40);
  gfx->print("delta");

  drawQualityBar(gfx, leftW + 4, 20, "RV", rvQuality);
  drawQualityBar(gfx, leftW + 4, 36, "MAG", magQuality);
  drawQualityBar(gfx, leftW + 4, 52, "GYR", gyroQuality);

  gfx->setCursor(leftW + 4, 74);
  gfx->printf("rvAcc %.1f", rvAccDeg);
  gfx->setCursor(leftW + 4, 86);
  gfx->printf("B %.1f uT", magNorm);
  gfx->setCursor(leftW + 4, 98);
  gfx->printf("w %.1f dps", gyroNormDps);
  gfx->setCursor(leftW + 4, 110);
  gfx->printf("p/r %.1f/%.1f", pitchDeg, rollDeg);
  gfx->setCursor(leftW + 4, 122);
  gfx->printf("stb %s", stabilityText(stabilityClass));
  gfx->setCursor(leftW + 4, 134);
  gfx->printf("act %s", activityText(activityState));

  display_flush_now();
}

void processBnoEvents() {
  if (!bnoReady) {
    return;
  }

  sh2_SensorValue_t v;
  while (bno.getSensorEvent(&v)) {
    if (v.sensorId == SH2_ROTATION_VECTOR) {
      qW = v.un.rotationVector.real;
      qX = v.un.rotationVector.i;
      qY = v.un.rotationVector.j;
      qZ = v.un.rotationVector.k;
      const float qw = qW;
      const float qx = qX;
      const float qy = qY;
      const float qz = qZ;
      const float siny = 2.0f * (qw * qz + qx * qy);
      const float cosy = 1.0f - 2.0f * (qy * qy + qz * qz);
      float yaw = atan2f(siny, cosy) * 180.0f / PI;
      if (yaw < 0.0f) {
        yaw += 360.0f;
      }
      const float sinp = 2.0f * (qw * qy - qz * qx);
      if (fabsf(sinp) >= 1.0f) {
        pitchDeg = copysignf(90.0f, sinp);
      } else {
        pitchDeg = asinf(sinp) * 180.0f / PI;
      }
      const float sinr = 2.0f * (qw * qx + qy * qz);
      const float cosr = 1.0f - 2.0f * (qx * qx + qy * qy);
      rollDeg = atan2f(sinr, cosr) * 180.0f / PI;
      headingDeg = yaw;
      rvQuality = (v.status & 0x03);
      rvAccDeg = v.un.rotationVector.accuracy * 180.0f / PI;
    } else if (v.sensorId == SH2_ACCELEROMETER) {
      accelX = v.un.accelerometer.x;
      accelY = v.un.accelerometer.y;
      accelZ = v.un.accelerometer.z;
      accelNorm = sqrtf(accelX * accelX + accelY * accelY + accelZ * accelZ);
      accelQuality = (v.status & 0x03);
    } else if (v.sensorId == SH2_LINEAR_ACCELERATION) {
      linAccX = v.un.linearAcceleration.x;
      linAccY = v.un.linearAcceleration.y;
      linAccZ = v.un.linearAcceleration.z;
      linAccNorm = sqrtf(linAccX * linAccX + linAccY * linAccY + linAccZ * linAccZ);
      linAccQuality = (v.status & 0x03);
    } else if (v.sensorId == SH2_GRAVITY) {
      gravX = v.un.gravity.x;
      gravY = v.un.gravity.y;
      gravZ = v.un.gravity.z;
      gravNorm = sqrtf(gravX * gravX + gravY * gravY + gravZ * gravZ);
      gravQuality = (v.status & 0x03);
    } else if (v.sensorId == SH2_MAGNETIC_FIELD_CALIBRATED) {
      magX = v.un.magneticField.x;
      magY = v.un.magneticField.y;
      magZ = v.un.magneticField.z;
      magNorm = sqrtf(magX * magX + magY * magY + magZ * magZ);
      magQuality = (v.status & 0x03);
    } else if (v.sensorId == SH2_GYROSCOPE_CALIBRATED) {
      gyroX = v.un.gyroscope.x * 57.2958f;
      gyroY = v.un.gyroscope.y * 57.2958f;
      gyroZ = v.un.gyroscope.z * 57.2958f;
      gyroNormDps = sqrtf(gyroX * gyroX + gyroY * gyroY + gyroZ * gyroZ);
      gyroQuality = (v.status & 0x03);
    } else if (v.sensorId == SH2_STEP_COUNTER) {
      stepCount = v.un.stepCounter.steps;
    } else if (v.sensorId == SH2_TAP_DETECTOR) {
      tapFlags = v.un.tapDetector.flags;
    } else if (v.sensorId == SH2_STABILITY_CLASSIFIER) {
      stabilityClass = v.un.stabilityClassifier.classification;
    } else if (v.sensorId == SH2_PERSONAL_ACTIVITY_CLASSIFIER) {
      activityState = v.un.personalActivityClassifier.mostLikelyState;
      if (activityState < 10) {
        activityConfidence = v.un.personalActivityClassifier.confidence[activityState];
      } else {
        activityConfidence = 0;
      }
    }
  }
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == 'h' || c == 'H') {
      Serial.println("[BNO] commands: 'h' help, 'z' set ref heading");
    } else if (c == 'z' || c == 'Z') {
      refHeadingDeg = headingDeg;
      refSet = true;
      Serial.printf("[BNO] ref heading set to %.1f deg\n", refHeadingDeg);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n[BNO-TEST] start");

  Wire.begin(cfg::kI2cSdaPin, cfg::kI2cSclPin, cfg::kI2cFreqHz);
  Wire.setTimeOut(20);
  delay(80);
  printI2cDevices();

  displayReady = display_init();
  Serial.printf("[DISPLAY] ready=%d\n", (int)displayReady);

  if (initBnoAt(0x4B)) {
    bnoReady = true;
    bnoAddr = 0x4B;
  } else if (initBnoAt(0x4A)) {
    bnoReady = true;
    bnoAddr = 0x4A;
  }

  Serial.printf("[BNO] ready=%d", (int)bnoReady);
  if (bnoReady) {
    Serial.printf(" addr=0x%02X", bnoAddr);
  }
  Serial.print("\n");
  Serial.printf("[BNO] DCD auto-save=%d\n", (int)dcdAutoSaveEnabled);
  Serial.println("[BNO] commands: 'h' help, 'z' set ref heading");

  drawUi();
}

void loop() {
  handleSerialCommands();
  processBnoEvents();
  const bool allHigh = (rvQuality >= 3) && (magQuality >= 3) && (gyroQuality >= 3);
  if (allHigh) {
    dcdSaved = true;
    if (!refSet) {
      refHeadingDeg = headingDeg;
      refSet = true;
    }
  }

  const unsigned long now = millis();
  if (now - lastUiMs >= 250) {
    lastUiMs = now;
    drawUi();
  }

  if (now - lastLogMs >= 1000) {
    lastLogMs = now;
    Serial.printf("[BNO] ypr=%.1f/%.1f/%.1f rvQ=%u(%s) rvAcc=%.2f q=[%.3f %.3f %.3f %.3f] "
                  "Aq%u|A|=%.2f LAq%u|LA|=%.2f Gq%u|G|=%.2f "
                  "magQ=%u|B|=%.1f gyroQ=%u|w|=%.1f steps=%u tap=0x%02X stb=%s act=%s(%u%%) save=%s\n",
                  headingDeg,
                  pitchDeg,
                  rollDeg,
                  rvQuality,
                  qualityText(rvQuality),
                  rvAccDeg,
                  qW, qX, qY, qZ,
                  accelQuality, accelNorm,
                  linAccQuality, linAccNorm,
                  gravQuality, gravNorm,
                  magQuality,
                  magNorm,
                  gyroQuality,
                  gyroNormDps,
                  (unsigned)stepCount,
                  tapFlags,
                  stabilityText(stabilityClass),
                  activityText(activityState),
                  activityConfidence,
                  dcdAutoSaveEnabled ? (dcdSaved ? "AUTO+CAL" : "AUTO") : "OFF");
  }
}
