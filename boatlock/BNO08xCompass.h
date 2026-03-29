#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BNO08x.h>
#include "Settings.h"

class BNO08xCompass {
public:
    void attachSettings(Settings* s) { settings = s; }

    bool init() {
        headingDeg = 0.0f;
        headingRawDeg = 0.0f;
        pitchDeg = 0.0f;
        rollDeg = 0.0f;
        rvAccDeg = 0.0f;
        rvQuality = 0;
        magQuality = 0;
        gyroQuality = 0;
        magNormUT = 0.0f;
        gyroNormDps = 0.0f;
        bnoAddress = 0;
        lastEventMs = 0;

        if (initBno(0x4B) || initBno(0x4A)) {
            sh2_setCalConfig(SH2_CAL_ACCEL | SH2_CAL_GYRO | SH2_CAL_MAG);
            sh2_setDcdAutoSave(true);
            return true;
        }
        return false;
    }

    const char* sourceName() const { return "BNO08x"; }
    uint8_t bnoI2cAddress() const { return bnoAddress; }

    void loadCalibrationFromSettings() {
        if (!settings) return;
        // Reuse existing persisted key to avoid EEPROM schema change.
        setHeadingOffsetDeg(settings->get("MagOffX"));
    }

    void saveCalibrationToSettings() {
        if (!settings) return;
        settings->set("MagOffX", headingOffsetDeg);
        settings->save();
    }

    void setHeadingOffsetDeg(float offsetDeg) {
        float norm = fmodf(offsetDeg, 360.0f);
        if (norm > 180.0f) norm -= 360.0f;
        if (norm < -180.0f) norm += 360.0f;
        headingOffsetDeg = norm;
    }

    float getHeadingOffsetDeg() const { return headingOffsetDeg; }

    bool read() {
        bool gotAnyEvent = false;
        sh2_SensorValue_t event;
        while (bno08x.getSensorEvent(&event)) {
            gotAnyEvent = true;
            lastEventMs = millis();
            if (event.sensorId == SH2_ROTATION_VECTOR) {
                const float qw = event.un.rotationVector.real;
                const float qx = event.un.rotationVector.i;
                const float qy = event.un.rotationVector.j;
                const float qz = event.un.rotationVector.k;

                const float siny = 2.0f * (qw * qz + qx * qy);
                const float cosy = 1.0f - 2.0f * (qy * qy + qz * qz);
                float yaw = atan2f(siny, cosy) * 180.0f / PI;
                if (yaw < 0.0f) yaw += 360.0f;
                headingRawDeg = yaw;
                headingDeg = normalized360(headingRawDeg + headingOffsetDeg);

                const float sinp = 2.0f * (qw * qy - qz * qx);
                if (fabsf(sinp) >= 1.0f) {
                    pitchDeg = copysignf(90.0f, sinp);
                } else {
                    pitchDeg = asinf(sinp) * 180.0f / PI;
                }
                const float sinr = 2.0f * (qw * qx + qy * qz);
                const float cosr = 1.0f - 2.0f * (qx * qx + qy * qy);
                rollDeg = atan2f(sinr, cosr) * 180.0f / PI;

                rvQuality = (event.status & 0x03);
                rvAccDeg = event.un.rotationVector.accuracy * 180.0f / PI;
            } else if (event.sensorId == SH2_MAGNETIC_FIELD_CALIBRATED) {
                const float x = event.un.magneticField.x;
                const float y = event.un.magneticField.y;
                const float z = event.un.magneticField.z;
                magNormUT = sqrtf(x * x + y * y + z * z);
                magQuality = (event.status & 0x03);
            } else if (event.sensorId == SH2_GYROSCOPE_CALIBRATED) {
                const float x = event.un.gyroscope.x * 57.2958f;
                const float y = event.un.gyroscope.y * 57.2958f;
                const float z = event.un.gyroscope.z * 57.2958f;
                gyroNormDps = sqrtf(x * x + y * y + z * z);
                gyroQuality = (event.status & 0x03);
            }
        }
        return gotAnyEvent;
    }

    float getAzimuth() const { return headingDeg; }
    float getRawAzimuth() const { return headingRawDeg; }
    uint8_t getHeadingQuality() const { return rvQuality; }
    float getRvAccuracyDeg() const { return rvAccDeg; }
    float getMagNormUT() const { return magNormUT; }
    float getGyroNormDps() const { return gyroNormDps; }
    uint8_t getMagQuality() const { return magQuality; }
    uint8_t getGyroQuality() const { return gyroQuality; }
    float getPitchDeg() const { return pitchDeg; }
    float getRollDeg() const { return rollDeg; }
    unsigned long lastEventAgeMs(unsigned long nowMs) const {
        if (lastEventMs == 0 || nowMs < lastEventMs) {
            return 0xFFFFFFFFUL;
        }
        return nowMs - lastEventMs;
    }

    // Legacy command kept to avoid breaking BLE API; BNO calibrates itself.
    void calibrate() {}

private:
    bool initBno(uint8_t address) {
        if (!bno08x.begin_I2C(address, &Wire, -1)) {
            return false;
        }
        if (!bno08x.enableReport(SH2_ROTATION_VECTOR, 20000)) return false;
        if (!bno08x.enableReport(SH2_MAGNETIC_FIELD_CALIBRATED, 100000)) return false;
        if (!bno08x.enableReport(SH2_GYROSCOPE_CALIBRATED, 100000)) return false;
        bnoAddress = address;
        return true;
    }

    static float normalized360(float deg) {
        float v = fmodf(deg, 360.0f);
        if (v < 0.0f) v += 360.0f;
        return v;
    }

    Settings* settings = nullptr;
    Adafruit_BNO08x bno08x{-1};
    uint8_t bnoAddress = 0;
    unsigned long lastEventMs = 0;

    float headingDeg = 0.0f;
    float headingRawDeg = 0.0f;
    float headingOffsetDeg = 0.0f;
    float pitchDeg = 0.0f;
    float rollDeg = 0.0f;
    float rvAccDeg = 0.0f;
    float magNormUT = 0.0f;
    float gyroNormDps = 0.0f;
    uint8_t rvQuality = 0;
    uint8_t magQuality = 0;
    uint8_t gyroQuality = 0;
};
