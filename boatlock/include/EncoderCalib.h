#pragma once
class Settings; // Forward declaration

#include <AS5600.h>

class EncoderCalib {
public:
    Settings* settings;
    EncoderCalib() : settings(nullptr) {} // дефолтный конструктор!
    void setSettings(Settings* s) { settings = s; }
    void calibrate(AS5600& encoder) {
        if (!settings) return;
        settings->data.encoderZeroOffset = encoder.rawAngle();
        settings->save();
    }
    float readAngle(AS5600& encoder) {
        if (!settings) return 0.0;
        int32_t raw = encoder.rawAngle();
        int32_t delta = raw - settings->data.encoderZeroOffset;
        if (delta < 0) delta += 4096;
        float angle = (float)delta * 360.0 / 4096.0;
        return angle;
    }
};
