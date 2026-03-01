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
constexpr uint32_t kRvReportUs = 20000;
constexpr uint32_t kGeoReportUs = 20000;
constexpr float kRoseSmoothAlpha = 0.22f;
constexpr uint16_t kBootSettleMs = 1200;
constexpr uint16_t kPostBeginSettleMs = 250;
constexpr uint8_t kReportRetryCount = 6;
constexpr uint16_t kReportRetryDelayMs = 120;
constexpr uint16_t kStaleDataTimeoutMs = 12000;
constexpr uint16_t kRecoveryRetryPeriodMs = 4000;
// Podolsk, Moscow Oblast (used for fixed true-north correction).
constexpr float kSiteLatDeg = 55.431f;
constexpr float kSiteLonDeg = 37.545f;
// East declination is positive: true = magnetic + declination.
constexpr float kDeclinationDeg = 11.2f;
}

Adafruit_BNO08x bno(-1);
bool displayReady = false;
bool bnoReady = false;
uint8_t bnoAddr = 0;

float headingDeg = 0.0f;
float headingRvDeg = 0.0f;
float headingGeoDeg = 0.0f;
float headingMagDeg = 0.0f;
float headingTrueDeg = 0.0f;
uint8_t headingQ = 0;
uint8_t geoQ = 0;
uint8_t magQ = 0;
float rvAccDeg = 0.0f;
float geoAccDeg = 0.0f;
bool roseInit = false;
const char* sourceName = "NONE";
uint8_t sourceQ = 0;
float sourceAccDeg = 999.0f;

unsigned long lastUiMs = 0;
unsigned long lastLogMs = 0;
unsigned long lastEventMs = 0;
unsigned long lastRecoverMs = 0;

static float wrap360(float deg) {
  while (deg < 0.0f) deg += 360.0f;
  while (deg >= 360.0f) deg -= 360.0f;
  return deg;
}

static float wrap180(float deg) {
  while (deg > 180.0f) deg -= 360.0f;
  while (deg < -180.0f) deg += 360.0f;
  return deg;
}

static float smoothAngle(float prev, float target, float alpha) {
  float d = wrap180(target - prev);
  return wrap360(prev + d * alpha);
}

bool enableBnoReports() {
  for (uint8_t i = 0; i < cfg::kReportRetryCount; ++i) {
    const bool okRv = bno.enableReport(SH2_ROTATION_VECTOR, cfg::kRvReportUs);
    const bool okGeo = bno.enableReport(SH2_GEOMAGNETIC_ROTATION_VECTOR, cfg::kGeoReportUs);
    const bool okMag = bno.enableReport(SH2_MAGNETIC_FIELD_CALIBRATED, 100000);
    if (okRv && okGeo && okMag) {
      return true;
    }
    delay(cfg::kReportRetryDelayMs);
  }
  return false;
}

bool initBnoAt(uint8_t addr) {
  if (!bno.begin_I2C(addr, &Wire, -1)) {
    return false;
  }
  delay(cfg::kPostBeginSettleMs);
  sh2_setCalConfig(SH2_CAL_ACCEL | SH2_CAL_GYRO | SH2_CAL_MAG);
  sh2_setDcdAutoSave(true);
  return enableBnoReports();
}

void drawArrow(Arduino_GFX* gfx,
               int cx,
               int cy,
               int r,
               float bearingDeg,
               uint16_t color,
               const char* label) {
  // NORTH-UP: 0 deg (North) is fixed to top of screen.
  const float screenDeg = bearingDeg;
  const float rad = (screenDeg - 90.0f) * DEG_TO_RAD;

  const int tailLen = r / 4;
  const int tipLen = r - 2;
  const int tx = cx + (int)(tailLen * cosf(rad));
  const int ty = cy + (int)(tailLen * sinf(rad));
  const int x = cx + (int)(tipLen * cosf(rad));
  const int y = cy + (int)(tipLen * sinf(rad));

  gfx->drawLine(tx, ty, x, y, color);

  const float lrad = rad + 2.55f;
  const float rrad = rad - 2.55f;
  const int head = 10;
  const int lx = x + (int)(head * cosf(lrad));
  const int ly = y + (int)(head * sinf(lrad));
  const int rx = x + (int)(head * cosf(rrad));
  const int ry = y + (int)(head * sinf(rrad));
  gfx->fillTriangle(x, y, lx, ly, rx, ry, color);

  if (label && label[0] != '\0') {
    gfx->setTextColor(color);
    gfx->setTextSize(2);
    gfx->setCursor(x - 7, y - 10);
    gfx->print(label);
  }
}

void drawUi() {
  if (!displayReady) return;

  Arduino_GFX* gfx = display_gfx();
  const int w = gfx->width();
  const int h = gfx->height();
  const int cx = w / 2;
  const int cy = h / 2 + 28;
  const int r = (w < h ? w : h) / 2 - 56;

  gfx->fillScreen(0xFFFF);
  gfx->setTextColor(0x0000);
  gfx->setTextSize(2);
  gfx->setCursor(6, 6);
  gfx->printf("BNO 0x%02X  I2C %d/%d", bnoAddr, cfg::kI2cSdaPin, cfg::kI2cSclPin);
  gfx->setCursor(6, 30);
  gfx->printf("SRC %s  Q%u  A%.0f", sourceName, sourceQ, sourceAccDeg);
  gfx->setCursor(6, 54);
  gfx->printf("NOSE %03.0f", headingTrueDeg);
  gfx->setTextSize(1);
  gfx->setCursor(6, 74);
  gfx->printf("PODOLSK %.3f %.3f DEC+%.1f", cfg::kSiteLatDeg, cfg::kSiteLonDeg, cfg::kDeclinationDeg);
  gfx->setCursor(6, 86);
  gfx->printf("rvQ=%u rvA=%.1f  gQ=%u gA=%.1f  mQ=%u", headingQ, rvAccDeg, geoQ, geoAccDeg, magQ);

  drawArrow(gfx, cx, cy, r - 4, headingTrueDeg, 0xF800, "N");
  gfx->fillCircle(cx, cy, 3, 0x0000);

  gfx->setTextSize(2);
  gfx->setCursor(8, h - 34);
  gfx->printf("NORTH %03.0f", headingTrueDeg);

  display_flush_now();
}

void processBno() {
  if (!bnoReady) return;
  sh2_SensorValue_t v;
  while (bno.getSensorEvent(&v)) {
    lastEventMs = millis();
    if (v.sensorId == SH2_ROTATION_VECTOR) {
      const float qw = v.un.rotationVector.real;
      const float qx = v.un.rotationVector.i;
      const float qy = v.un.rotationVector.j;
      const float qz = v.un.rotationVector.k;
      const float siny = 2.0f * (qw * qz + qx * qy);
      const float cosy = 1.0f - 2.0f * (qy * qy + qz * qz);
      headingRvDeg = wrap360(atan2f(siny, cosy) * 180.0f / PI);
      headingQ = (v.status & 0x03);
      rvAccDeg = v.un.rotationVector.accuracy * 180.0f / PI;
    } else if (v.sensorId == SH2_GEOMAGNETIC_ROTATION_VECTOR) {
      const float qw = v.un.geoMagRotationVector.real;
      const float qx = v.un.geoMagRotationVector.i;
      const float qy = v.un.geoMagRotationVector.j;
      const float qz = v.un.geoMagRotationVector.k;
      const float siny = 2.0f * (qw * qz + qx * qy);
      const float cosy = 1.0f - 2.0f * (qy * qy + qz * qz);
      headingGeoDeg = wrap360(atan2f(siny, cosy) * 180.0f / PI);
      geoQ = (v.status & 0x03);
      geoAccDeg = v.un.geoMagRotationVector.accuracy * 180.0f / PI;
    } else if (v.sensorId == SH2_MAGNETIC_FIELD_CALIBRATED) {
      // Magnetic heading in sensor plane: robust for quick compass debugging.
      const float mx = v.un.magneticField.x;
      const float my = v.un.magneticField.y;
      headingMagDeg = wrap360(atan2f(my, mx) * 180.0f / PI);
      magQ = (v.status & 0x03);
    }
  }
  float targetRose = headingRvDeg;
  sourceName = "RV";
  sourceQ = headingQ;
  sourceAccDeg = rvAccDeg;
  if (geoQ >= 2 && geoAccDeg < 120.0f) {
    targetRose = headingGeoDeg;
    sourceName = "GEO";
    sourceQ = geoQ;
    sourceAccDeg = geoAccDeg;
  } else if (headingQ >= 2) {
    targetRose = headingRvDeg;
    sourceName = "RV";
    sourceQ = headingQ;
    sourceAccDeg = rvAccDeg;
  } else if (magQ >= 1) {
    targetRose = headingMagDeg;
    sourceName = "MAG";
    sourceQ = magQ;
    sourceAccDeg = 999.0f;
  }

  if (!roseInit) {
    headingDeg = targetRose;
    roseInit = true;
  } else {
    headingDeg = smoothAngle(headingDeg, targetRose, cfg::kRoseSmoothAlpha);
  }
  headingTrueDeg = wrap360(headingDeg + cfg::kDeclinationDeg);
}

void setup() {
  Serial.begin(115200);
  delay(700);
  Serial.println("\n[COMPASS-NORTH-LOCK-TEST] start");

  delay(cfg::kBootSettleMs);
  Wire.begin(cfg::kI2cSdaPin, cfg::kI2cSclPin, cfg::kI2cFreqHz);
  Wire.setTimeOut(20);

  displayReady = display_init();
  if (initBnoAt(0x4B)) {
    bnoReady = true;
    bnoAddr = 0x4B;
  } else if (initBnoAt(0x4A)) {
    bnoReady = true;
    bnoAddr = 0x4A;
  }
  Serial.printf("[BNO] ready=%d addr=0x%02X\n", (int)bnoReady, bnoAddr);
  lastEventMs = millis();
  lastRecoverMs = millis();
  drawUi();
}

void loop() {
  const unsigned long now = millis();
  if (!bnoReady && (now - lastRecoverMs > cfg::kRecoveryRetryPeriodMs)) {
    Serial.println("[BNO] retry init");
    if (initBnoAt(0x4B)) {
      bnoReady = true;
      bnoAddr = 0x4B;
      lastEventMs = now;
      Serial.println("[BNO] init ok at 0x4B");
    } else if (initBnoAt(0x4A)) {
      bnoReady = true;
      bnoAddr = 0x4A;
      lastEventMs = now;
      Serial.println("[BNO] init ok at 0x4A");
    } else {
      Serial.println("[BNO] init retry failed");
    }
    lastRecoverMs = now;
  }

  processBno();

  if (bnoReady && bno.wasReset()) {
    Serial.println("[BNO] reset detected, re-enabling reports");
    if (enableBnoReports()) {
      Serial.println("[BNO] reports restored after reset");
      lastEventMs = now;
    } else {
      Serial.println("[BNO] report restore failed");
    }
    lastRecoverMs = now;
  }
  if (bnoReady && (now - lastEventMs > cfg::kStaleDataTimeoutMs) && (now - lastRecoverMs > cfg::kRecoveryRetryPeriodMs)) {
    Serial.println("[BNO] no fresh events, retrying report setup");
    if (enableBnoReports()) {
      Serial.println("[BNO] reports restored");
      lastEventMs = now;
    }
    lastRecoverMs = now;
  }
  if (now - lastUiMs >= 120) {
    lastUiMs = now;
    drawUi();
  }
  if (now - lastLogMs >= 1000) {
    lastLogMs = now;
    Serial.printf("[HDG] ready=%u mag=%.1f true=%.1f dec=%.1f geo=%.1f(gQ=%u acc=%.2f) rv=%.1f(rvQ=%u acc=%.2f) magRaw=%.1f(mQ=%u)\n",
                  (unsigned)bnoReady,
                  headingDeg,
                  headingTrueDeg,
                  cfg::kDeclinationDeg,
                  headingGeoDeg,
                  geoQ,
                  geoAccDeg,
                  headingRvDeg,
                  headingQ,
                  rvAccDeg,
                  headingMagDeg,
                  magQ);
  }
}
