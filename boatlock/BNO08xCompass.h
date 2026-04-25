#pragma once

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "BNO08xMath.h"
#include "BnoRvcFrame.h"
#include "Settings.h"

#ifndef BOATLOCK_COMPASS_SH2_UART
#define BOATLOCK_COMPASS_SH2_UART 0
#endif

#if BOATLOCK_COMPASS_SH2_UART
#include <sh2.h>
#include <sh2_SensorValue.h>
#include <sh2_err.h>
#endif

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
    return begin(serial, rxPin, -1, baud);
  }

  bool begin(HardwareSerial& serial, int rxPin, int txPin, uint32_t baud) {
    serialPort = &serial;
    rvcRxPin = rxPin;
    rvcTxPin = txPin;
    rvcBaud = baud;
    clearEventState();
#if BOATLOCK_COMPASS_SH2_UART
    return beginSh2Uart();
#else
    parser.reset();
    serialPort->end();
    delay(10);
    serialPort->begin(rvcBaud, SERIAL_8N1, rvcRxPin, -1);
    serialOpen = true;
    return true;
#endif
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

  const char* sourceName() const {
#if BOATLOCK_COMPASS_SH2_UART
    return "BNO08x-SH2-UART";
#else
    return "BNO08x-RVC";
#endif
  }
  int rxPin() const { return rvcRxPin; }
  int txPin() const { return rvcTxPin; }
  uint32_t baud() const { return rvcBaud; }
  bool sh2Enabled() const {
#if BOATLOCK_COMPASS_SH2_UART
    return true;
#else
    return false;
#endif
  }

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
    headingOffsetDeg = bno08xNormalize180(offsetDeg);
    headingDeg = normalized360(headingRawDeg + headingOffsetDeg);
  }

  float getHeadingOffsetDeg() const { return headingOffsetDeg; }

  BNO08xCompassReadResult read() {
#if BOATLOCK_COMPASS_SH2_UART
    return readSh2();
#else
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
#endif
  }

  float getAzimuth() const { return headingDeg; }
  float getRawAzimuth() const { return headingRawDeg; }
  uint8_t getHeadingQuality() const { return hasHeadingEvent ? headingQuality : 0; }
  float getRvAccuracyDeg() const { return rvAccuracyDeg; }
  float getMagNormUT() const { return magNormUT; }
  float getGyroNormDps() const { return gyroNormDps; }
  uint8_t getMagQuality() const { return magQuality; }
  uint8_t getGyroQuality() const { return gyroQuality; }
  float getPitchDeg() const { return pitchDeg; }
  float getRollDeg() const { return rollDeg; }
  float getAccelXMps2() const { return accelXMps2; }
  float getAccelYMps2() const { return accelYMps2; }
  float getAccelZMps2() const { return accelZMps2; }
  uint8_t lastFrameIndex() const { return frameIndex; }

  bool startDynamicCalibration(bool autoSave) {
#if BOATLOCK_COMPASS_SH2_UART
    if (!sh2Ready) {
      return false;
    }
    const int calStatus =
        sh2_setCalConfig(SH2_CAL_ACCEL | SH2_CAL_GYRO | SH2_CAL_MAG | SH2_CAL_PLANAR);
    if (calStatus != SH2_OK) {
      return false;
    }
    return sh2_setDcdAutoSave(autoSave) == SH2_OK;
#else
    (void)autoSave;
    return false;
#endif
  }

  bool saveDynamicCalibration() {
#if BOATLOCK_COMPASS_SH2_UART
    return sh2Ready && sh2_saveDcdNow() == SH2_OK;
#else
    return false;
#endif
  }

  bool setDcdAutoSave(bool enabled) {
#if BOATLOCK_COMPASS_SH2_UART
    return sh2Ready && sh2_setDcdAutoSave(enabled) == SH2_OK;
#else
    (void)enabled;
    return false;
#endif
  }

  bool tareHeadingNow() {
#if BOATLOCK_COMPASS_SH2_UART
    return sh2Ready &&
           sh2_setTareNow(SH2_TARE_Z, SH2_TARE_BASIS_ROTATION_VECTOR) == SH2_OK;
#else
    return false;
#endif
  }

  bool saveTare() {
#if BOATLOCK_COMPASS_SH2_UART
    return sh2Ready && sh2_persistTare() == SH2_OK;
#else
    return false;
#endif
  }

  bool clearTare() {
#if BOATLOCK_COMPASS_SH2_UART
    return sh2Ready && sh2_clearTare() == SH2_OK;
#else
    return false;
#endif
  }

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
    headingQuality = 3;
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
    headingQuality = 0;
  }

  static float normalized360(float deg) {
    return bno08xNormalize360(deg);
  }

#if BOATLOCK_COMPASS_SH2_UART
  bool beginSh2Uart() {
    if (!serialPort || rvcRxPin < 0 || rvcTxPin < 0) {
      serialOpen = false;
      sh2Ready = false;
      return false;
    }

    activeSh2CompassRef() = this;
    resetSh2FrameParser();
    sh2Hal.open = &BNO08xCompass::sh2HalOpen;
    sh2Hal.close = &BNO08xCompass::sh2HalClose;
    sh2Hal.read = &BNO08xCompass::sh2HalRead;
    sh2Hal.write = &BNO08xCompass::sh2HalWrite;
    sh2Hal.getTimeUs = &BNO08xCompass::sh2HalGetTimeUs;

    serialOpen = true;
    sh2Ready = false;
    if (sh2_open(&sh2Hal, &BNO08xCompass::sh2EventHandler, this) != SH2_OK) {
      serialOpen = false;
      return false;
    }

    memset(&productIds, 0, sizeof(productIds));
    if (sh2_getProdIds(&productIds) != SH2_OK) {
      sh2_close();
      serialOpen = false;
      return false;
    }

    if (sh2_setSensorCallback(&BNO08xCompass::sh2SensorHandler, this) != SH2_OK) {
      sh2_close();
      serialOpen = false;
      return false;
    }

    sh2Ready = enableSh2Reports();
    if (!sh2Ready) {
      sh2_close();
      serialOpen = false;
      return false;
    }
    startDynamicCalibration(false);
    return true;
  }

  bool enableSh2Reports() {
    return enableSh2Report(SH2_ROTATION_VECTOR, 20000) &&
           enableSh2Report(SH2_MAGNETIC_FIELD_CALIBRATED, 100000) &&
           enableSh2Report(SH2_GYROSCOPE_CALIBRATED, 100000) &&
           enableSh2Report(SH2_ACCELEROMETER, 100000);
  }

  bool enableSh2Report(sh2_SensorId_t sensorId, uint32_t intervalUs) {
    sh2_SensorConfig_t config;
    memset(&config, 0, sizeof(config));
    config.reportInterval_us = intervalUs;
    return sh2_setSensorConfig(sensorId, &config) == SH2_OK;
  }

  BNO08xCompassReadResult readSh2() {
    BNO08xCompassReadResult result;
    if (!serialPort || !serialOpen || !sh2Ready) {
      return result;
    }
    const uint32_t anyBefore = sh2AnyEventCount;
    const uint32_t headingBefore = sh2HeadingEventCount;
    for (uint8_t i = 0; i < 8; ++i) {
      sh2_service();
    }
    result.anyEvent = sh2AnyEventCount != anyBefore;
    result.headingEvent = sh2HeadingEventCount != headingBefore;
    return result;
  }

  void applySh2SensorValue(const sh2_SensorValue_t& value) {
    lastAnyEventMs = millis();
    hasAnyEvent = true;
    sh2AnyEventCount++;
    frameIndex = value.sequence;

    switch (value.sensorId) {
      case SH2_ROTATION_VECTOR: {
        const BNO08xEulerDeg euler = bno08xEulerFromQuaternion(
            value.un.rotationVector.i,
            value.un.rotationVector.j,
            value.un.rotationVector.k,
            value.un.rotationVector.real);
        headingRawDeg = euler.yaw;
        headingDeg = normalized360(headingRawDeg + headingOffsetDeg);
        pitchDeg = euler.pitch;
        rollDeg = euler.roll;
        rvAccuracyDeg = bno08xRadToDeg(value.un.rotationVector.accuracy);
        headingQuality = value.status & 0x03;
        lastHeadingEventMs = lastAnyEventMs;
        hasHeadingEvent = true;
        sh2HeadingEventCount++;
        break;
      }
      case SH2_MAGNETIC_FIELD_CALIBRATED:
        magNormUT = bno08xVectorNorm3(value.un.magneticField.x,
                                      value.un.magneticField.y,
                                      value.un.magneticField.z);
        magQuality = value.status & 0x03;
        break;
      case SH2_GYROSCOPE_CALIBRATED:
        gyroNormDps = bno08xRadToDeg(bno08xVectorNorm3(value.un.gyroscope.x,
                                                       value.un.gyroscope.y,
                                                       value.un.gyroscope.z));
        gyroQuality = value.status & 0x03;
        break;
      case SH2_ACCELEROMETER:
        accelXMps2 = value.un.accelerometer.x;
        accelYMps2 = value.un.accelerometer.y;
        accelZMps2 = value.un.accelerometer.z;
        break;
      default:
        break;
    }
  }

  int sh2Open() {
    serialPort->end();
    delay(10);
    serialPort->begin(kSh2UartBaud, SERIAL_8N1, rvcRxPin, rvcTxPin);
    rvcBaud = kSh2UartBaud;
    delay(10);
    while (serialPort->available() > 0) {
      serialPort->read();
      yield();
    }
    const uint8_t softreset[] = {0x7E, 1, 5, 0, 1, 0, 1, 0x7E};
    for (size_t i = 0; i < sizeof(softreset); ++i) {
      serialPort->write(softreset[i]);
      delay(1);
    }
    return 0;
  }

  void sh2Close() {
    if (serialPort) {
      serialPort->end();
    }
  }

  int sh2Read(uint8_t* pBuffer, unsigned len, uint32_t* tUs) {
    if (!serialPort || !pBuffer || len == 0) {
      return 0;
    }
    while (serialPort->available() > 0) {
      uint8_t c = static_cast<uint8_t>(serialPort->read());
      if (!sh2InFrame) {
        if (c == 0x7E) {
          sh2InFrame = true;
          sh2SawProtocol = false;
          sh2Escaped = false;
          sh2PacketLen = 0;
        }
        continue;
      }

      if (!sh2SawProtocol) {
        if (c == 0x7E) {
          continue;
        }
        if (c != 0x01) {
          resetSh2FrameParser();
          continue;
        }
        sh2SawProtocol = true;
        continue;
      }

      if (c == 0x7E) {
        const size_t packetLen = sh2PacketLen;
        sh2InFrame = true;
        sh2SawProtocol = false;
        sh2Escaped = false;
        sh2PacketLen = 0;
        if (packetLen == 0 || packetLen > len) {
          return 0;
        }
        memcpy(pBuffer, sh2Packet, packetLen);
        if (tUs) {
          *tUs = micros();
        }
        return (int)packetLen;
      }

      if (c == 0x7D) {
        sh2Escaped = true;
        continue;
      }
      if (sh2Escaped) {
        c ^= 0x20;
        sh2Escaped = false;
      }
      if (sh2PacketLen >= sizeof(sh2Packet)) {
        resetSh2FrameParser();
        continue;
      }
      sh2Packet[sh2PacketLen++] = c;
    }
    return 0;
  }

  int sh2Write(uint8_t* pBuffer, unsigned len) {
    if (!serialPort || !pBuffer) {
      return 0;
    }
    serialPort->write(0x7E);
    delay(1);
    serialPort->write(0x01);
    delay(1);
    for (unsigned i = 0; i < len; ++i) {
      uint8_t c = pBuffer[i];
      if (c == 0x7E || c == 0x7D) {
        serialPort->write(0x7D);
        delay(1);
        c ^= 0x20;
      }
      serialPort->write(c);
      delay(1);
    }
    serialPort->write(0x7E);
    return (int)len;
  }

  void resetSh2FrameParser() {
    sh2InFrame = false;
    sh2SawProtocol = false;
    sh2Escaped = false;
    sh2PacketLen = 0;
  }

  static int sh2HalOpen(sh2_Hal_t*) {
    return activeSh2CompassRef() ? activeSh2CompassRef()->sh2Open() : -1;
  }

  static void sh2HalClose(sh2_Hal_t*) {
    if (activeSh2CompassRef()) {
      activeSh2CompassRef()->sh2Close();
    }
  }

  static int sh2HalRead(sh2_Hal_t*, uint8_t* pBuffer, unsigned len, uint32_t* tUs) {
    return activeSh2CompassRef() ? activeSh2CompassRef()->sh2Read(pBuffer, len, tUs) : 0;
  }

  static int sh2HalWrite(sh2_Hal_t*, uint8_t* pBuffer, unsigned len) {
    return activeSh2CompassRef() ? activeSh2CompassRef()->sh2Write(pBuffer, len) : 0;
  }

  static uint32_t sh2HalGetTimeUs(sh2_Hal_t*) {
    return micros();
  }

  static void sh2EventHandler(void* cookie, sh2_AsyncEvent_t* event) {
    (void)event;
    BNO08xCompass* self = static_cast<BNO08xCompass*>(cookie);
    if (self) {
      self->sh2AsyncEventCount++;
    }
  }

  static void sh2SensorHandler(void* cookie, sh2_SensorEvent_t* event) {
    BNO08xCompass* self = static_cast<BNO08xCompass*>(cookie);
    if (!self || !event) {
      return;
    }
    sh2_SensorValue_t value;
    if (sh2_decodeSensorEvent(&value, event) == SH2_OK) {
      self->applySh2SensorValue(value);
    }
  }

  static constexpr uint32_t kSh2UartBaud = 3000000UL;
  static BNO08xCompass*& activeSh2CompassRef() {
    static BNO08xCompass* active = nullptr;
    return active;
  }
  sh2_Hal_t sh2Hal;
  sh2_ProductIds_t productIds;
  bool sh2Ready = false;
  uint8_t sh2Packet[SH2_HAL_MAX_TRANSFER_IN];
  size_t sh2PacketLen = 0;
  bool sh2InFrame = false;
  bool sh2SawProtocol = false;
  bool sh2Escaped = false;
  uint32_t sh2AnyEventCount = 0;
  uint32_t sh2HeadingEventCount = 0;
  uint32_t sh2AsyncEventCount = 0;
#endif

  Settings* settings = nullptr;
  HardwareSerial* serialPort = nullptr;
  BnoRvcStreamParser parser;
  int resetPin = -1;
  int rvcRxPin = -1;
  int rvcTxPin = -1;
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
  float rvAccuracyDeg = 0.0f;
  float magNormUT = 0.0f;
  float gyroNormDps = 0.0f;
  uint8_t headingQuality = 0;
  uint8_t magQuality = 0;
  uint8_t gyroQuality = 0;
};
