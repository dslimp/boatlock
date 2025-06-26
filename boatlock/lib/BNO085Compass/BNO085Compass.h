#pragma once
#include <Wire.h>
#include <Arduino.h>
#include <SparkFun_BNO08x_Arduino_Library.h>

class BNO085Compass {
public:
    bool init(uint8_t addr = 0x4B, uint16_t reportIntervalMs = 20) {
        Wire.begin();
        if (!bno08x.begin(addr)) return false;
        bno08x.enableRotationVector(reportIntervalMs);
        return true;
    }

    void read() {
        if (bno08x.wasReset()) {
            bno08x.enableRotationVector(20);
        }
        if (bno08x.getSensorEvent()) {
            float rad = bno08x.getYaw();
            heading = rad * 180.0f / PI;
            if (heading < 0) heading += 360.0f;
        }
    }

    float getAzimuth() const { return heading; }

    // Compatibility stubs
    void calibrate() {}
    void setCalibrationOffsets(float, float, float) {}
    void setCalibrationScales(float, float, float) {}
    float getCalibrationOffset(uint8_t) const { return 0.0f; }
    float getCalibrationScale(uint8_t) const { return 1.0f; }

private:
    BNO08x bno08x;
    float heading = 0.0f;
};
