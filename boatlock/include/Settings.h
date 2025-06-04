#pragma once
#include <EEPROM.h>

struct SettingsData {
    float Kp = 20.0;
    float Ki = 0.5;
    float Kd = 5.0;
    float distanceThreshold = 2.0;
    float angleTolerance = 3.0;
    int screenBrightness = 128;
    uint16_t encoderZeroOffset = 0;
    // Добавь любые другие параметры
};

class Settings {
public:
    static const int EEPROM_ADDR = 256; // где храним структуру
    SettingsData data;

    void load() {
        EEPROM.get(EEPROM_ADDR, data);
        // Если значения “левые” после холодного старта — выставить дефолты:
        if (data.Kp < 0.01 || data.Kp > 200.0) reset();
    }

    void save() {
        EEPROM.put(EEPROM_ADDR, data);
        EEPROM.commit();
    }

    void reset() {
        data = SettingsData();
        save();
    }
};
