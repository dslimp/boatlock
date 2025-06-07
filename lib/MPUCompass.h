#pragma once
#include <Wire.h>
#include <MPU9250.h>
#include <MadgwickAHRS.h>

class MPUCompass {
public:
    MPU9250 mpu;
    Madgwick filter;
    float heading = 0;

    MPUCompass() {}

    bool begin(uint8_t addr = 0x68, float filterRate = 100.0f) {
        Wire.begin();
        if (!mpu.setup(addr)) {
            return false;
        }
        filter.begin(filterRate);
        return true;
    }

    void update() {
        if (mpu.update()) {
            float ax = mpu.getAccX();
            float ay = mpu.getAccY();
            float az = mpu.getAccZ();
            float gx = mpu.getGyroX();
            float gy = mpu.getGyroY();
            float gz = mpu.getGyroZ();
            float mx = mpu.getMagX();
            float my = mpu.getMagY();
            float mz = mpu.getMagZ();

            filter.update(gx, gy, gz, ax, ay, az, mx, my, mz);
            heading = filter.getYaw();
            if (heading < 0) heading += 360;
        }
    }

    float getHeading() const {
        return heading;
    }
};
