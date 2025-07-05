#pragma once
#include <Wire.h>
#include <Arduino.h>
#include <DFRobot_QMC5883.h>

class QMC5883Compass {
public:
    bool init(uint8_t addr = 0x0D) {
        Wire.begin();
        qmc = DFRobot_QMC5883(&Wire, addr);
        if (!qmc.begin()) return false;
        qmc.setRange(QMC5883_RANGE_8GA);
        qmc.setMeasurementMode(QMC5883_CONTINOUS);
        qmc.setDataRate(QMC5883_DATARATE_50HZ);
        qmc.setSamples(QMC5883_SAMPLES_8);
        return true;
    }

    void read() {
        auto vec = qmc.readRaw();
        qmc.getHeadingDegrees();
        heading = vec.HeadingDegress;
        if (heading < 0) heading += 360.0f;
    }

    float getAzimuth() const { return heading; }

    // Compatibility stubs
    void calibrate() {}
    void setCalibrationOffsets(float, float, float) {}
    void setCalibrationScales(float, float, float) {}
    float getCalibrationOffset(uint8_t) const { return 0.0f; }
    float getCalibrationScale(uint8_t) const { return 1.0f; }

private:
    DFRobot_QMC5883 qmc{&Wire, 0x0D};
    float heading = 0.0f;
};
