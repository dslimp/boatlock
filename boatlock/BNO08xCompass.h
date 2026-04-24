#pragma once

#include <Arduino.h>
#include <math.h>

#include "BnoRvcFrame.h"
#include "Settings.h"

struct BNO08xCompassReadResult {
  bool anyEvent = false;
  bool headingEvent = false;
};

class BNO08xCompass {
public:
  void attachSettings(Settings* s) { settings = s; }

  void attachResetPin(int pin) {
    resetPin = pin;
    if (resetPin >= 0) {
      pinMode(resetPin, OUTPUT);
      digitalWrite(resetPin, HIGH);
    }
  }

  bool begin(HardwareSerial& serial, int rxPin, uint32_t baud) {
    serialPort = &serial;
    rvcRxPin = rxPin;
    rvcBaud = baud;
    clearEventState();
    parser.reset();
    serialPort->end();
    delay(10);
    serialPort->begin(rvcBaud, SERIAL_8N1, rvcRxPin, -1);
    serialOpen = true;
    return true;
  }

  bool init() { return serialOpen; }

  bool hardwareReset() {
    if (resetPin < 0) {
      return false;
    }
    digitalWrite(resetPin, HIGH);
    delay(10);
    digitalWrite(resetPin, LOW);
    delay(100);
    digitalWrite(resetPin, HIGH);
    delay(800);
    clearEventState();
    parser.reset();
    return true;
  }

  const char* sourceName() const { return "BNO08x-RVC"; }
  int rxPin() const { return rvcRxPin; }
  uint32_t baud() const { return rvcBaud; }

  void loadCalibrationFromSettings() {
    if (!settings) {
      return;
    }
    setHeadingOffsetDeg(settings->get("MagOffX"));
  }

  void saveCalibrationToSettings() {
    if (!settings) {
      return;
    }
    settings->set("MagOffX", headingOffsetDeg);
    settings->save();
  }

  void setHeadingOffsetDeg(float offsetDeg) {
    float norm = fmodf(offsetDeg, 360.0f);
    if (norm > 180.0f) {
      norm -= 360.0f;
    }
    if (norm < -180.0f) {
      norm += 360.0f;
    }
    headingOffsetDeg = norm;
    headingDeg = normalized360(headingRawDeg + headingOffsetDeg);
  }

  float getHeadingOffsetDeg() const { return headingOffsetDeg; }

  BNO08xCompassReadResult read() {
    BNO08xCompassReadResult result;
    if (!serialPort || !serialOpen) {
      return result;
    }

    while (serialPort->available() > 0) {
      const uint8_t byte = static_cast<uint8_t>(serialPort->read());
      BnoRvcSample sample;
      if (!parser.push(byte, &sample)) {
        continue;
      }

      applySample(sample);
      result.anyEvent = true;
      result.headingEvent = true;
    }
    return result;
  }

  float getAzimuth() const { return headingDeg; }
  float getRawAzimuth() const { return headingRawDeg; }
  uint8_t getHeadingQuality() const { return hasHeadingEvent ? 3 : 0; }
  float getRvAccuracyDeg() const { return 0.0f; }
  float getMagNormUT() const { return 0.0f; }
  float getGyroNormDps() const { return 0.0f; }
  uint8_t getMagQuality() const { return 0; }
  uint8_t getGyroQuality() const { return 0; }
  float getPitchDeg() const { return pitchDeg; }
  float getRollDeg() const { return rollDeg; }
  float getAccelXMps2() const { return accelXMps2; }
  float getAccelYMps2() const { return accelYMps2; }
  float getAccelZMps2() const { return accelZMps2; }
  uint8_t lastFrameIndex() const { return frameIndex; }

  unsigned long lastHeadingEventAgeMs(unsigned long nowMs) const {
    if (!hasHeadingEvent) {
      return 0xFFFFFFFFUL;
    }
    return nowMs - lastHeadingEventMs;
  }

  unsigned long lastAnyEventAgeMs(unsigned long nowMs) const {
    if (!hasAnyEvent) {
      return 0xFFFFFFFFUL;
    }
    return nowMs - lastAnyEventMs;
  }

private:
  void applySample(const BnoRvcSample& sample) {
    lastAnyEventMs = millis();
    lastHeadingEventMs = lastAnyEventMs;
    hasAnyEvent = true;
    hasHeadingEvent = true;
    frameIndex = sample.index;
    headingRawDeg = normalized360(sample.yawDeg);
    headingDeg = normalized360(headingRawDeg + headingOffsetDeg);
    pitchDeg = sample.pitchDeg;
    rollDeg = sample.rollDeg;
    accelXMps2 = sample.accelXMps2;
    accelYMps2 = sample.accelYMps2;
    accelZMps2 = sample.accelZMps2;
  }

  void clearEventState() {
    lastAnyEventMs = 0;
    lastHeadingEventMs = 0;
    hasAnyEvent = false;
    hasHeadingEvent = false;
    frameIndex = 0;
  }

  static float normalized360(float deg) {
    float v = fmodf(deg, 360.0f);
    if (v < 0.0f) {
      v += 360.0f;
    }
    return v;
  }

  Settings* settings = nullptr;
  HardwareSerial* serialPort = nullptr;
  BnoRvcStreamParser parser;
  int resetPin = -1;
  int rvcRxPin = -1;
  uint32_t rvcBaud = 115200;
  bool serialOpen = false;
  unsigned long lastAnyEventMs = 0;
  unsigned long lastHeadingEventMs = 0;
  bool hasAnyEvent = false;
  bool hasHeadingEvent = false;
  uint8_t frameIndex = 0;

  float headingDeg = 0.0f;
  float headingRawDeg = 0.0f;
  float headingOffsetDeg = 0.0f;
  float pitchDeg = 0.0f;
  float rollDeg = 0.0f;
  float accelXMps2 = 0.0f;
  float accelYMps2 = 0.0f;
  float accelZMps2 = 0.0f;
};
