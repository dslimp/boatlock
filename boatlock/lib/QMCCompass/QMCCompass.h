#pragma once
#include <Wire.h>
#include <Arduino.h>

class QMCCompass {
public:
    bool begin(uint8_t addr = 0x0D) {
        _addr = addr;
        Wire.begin();
        if (!writeReg(0x0B, 0x01)) return false; // Set/reset period
        // Continuous mode, ODR=200Hz, RNG=8G, OSR=512
        if (!writeReg(0x09, 0x1D)) return false;
        return true;
    }

    void update() {
        Wire.beginTransmission(_addr);
        Wire.write(0x00);
        if (Wire.endTransmission() != 0) return;
        Wire.requestFrom((int)_addr, 6);
        if (Wire.available() < 6) return;
        int16_t x = Wire.read() | (Wire.read() << 8);
        int16_t y = Wire.read() | (Wire.read() << 8);
        int16_t z = Wire.read() | (Wire.read() << 8);
        (void)z;
        heading = atan2((float)y, (float)x) * 180.0f / PI;
        if (heading < 0) heading += 360.0f;
    }

    float getHeading() const { return heading; }

private:
    uint8_t _addr = 0x0D;
    float heading = 0.0f;

    bool writeReg(uint8_t reg, uint8_t val) {
        Wire.beginTransmission(_addr);
        Wire.write(reg);
        Wire.write(val);
        return Wire.endTransmission() == 0;
    }
};
