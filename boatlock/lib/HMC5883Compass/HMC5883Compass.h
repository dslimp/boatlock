#pragma once
#include <Wire.h>
#include <Arduino.h>
#include <Adafruit_HMC5883_U.h>

class HMC5883Compass {
public:
    bool init() {
        Wire.begin();
        return mag.begin();
    }

    void read() {
        sensors_event_t event;
        mag.getEvent(&event);
        float hdg = atan2(event.magnetic.y, event.magnetic.x);
        if(hdg < 0) hdg += 2 * PI;
        heading = hdg * 180.0f / PI;
    }

    float getAzimuth() const { return heading; }

    // Compatibility stubs
    void calibrate() {}
    void setCalibrationOffsets(float, float, float) {}
    void setCalibrationScales(float, float, float) {}
    float getCalibrationOffset(uint8_t) const { return 0.0f; }
    float getCalibrationScale(uint8_t) const { return 1.0f; }

private:
    Adafruit_HMC5883_Unified mag{12345};
    float heading = 0.0f;
};
