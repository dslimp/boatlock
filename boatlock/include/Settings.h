#pragma once
#include <EEPROM.h>
#include <string.h>

enum SettingType { TYPE_INT, TYPE_FLOAT, TYPE_BOOL };

struct SettingEntry {
    const char* key;
    const char* description;
    SettingType type;
    float value;
    float defaultValue;
    float minValue, maxValue, step;
    const char* unit;
    bool visibleInMenu;
};

    // --- Заполняй параметры тут ---
SettingEntry entries[] = {
    {"Kp",        "Kp for PID",         TYPE_FLOAT, 20.0, 20.0,  0.01, 200.0,  0.1, "", true},
    {"Ki",        "Ki for PID",         TYPE_FLOAT, 0.5,  0.5,   0.0,  10.0,  0.01, "", true},
    {"Kd",        "Kd for PID",         TYPE_FLOAT, 5.0,  5.0,   0.0, 100.0,  0.1, "", true},
    {"DistTh",    "Distance threshold", TYPE_FLOAT, 2.0,  2.0,   0.1,  10.0,  0.1, "m", true},
    {"AngTol",    "Angle tolerance",    TYPE_FLOAT, 3.0,  3.0,   0.1,  180.0, 1.0, "deg", true},
    {"ScreenBr",  "Screen brightness",  TYPE_INT,   128.0,128.0, 0,    255,    1, "", false},
    {"Encoder0",  "Encoder zero",       TYPE_INT,   0,    0,     0,    4095,   1, "", false},
    {"USAngle",   "US angle",           TYPE_FLOAT, 30.0, 30.0,  0.0,  90.0,   1.0, "deg", true},
    {"USThresh",  "US threshold",       TYPE_FLOAT, 1.5,  1.5,   0.1,  6.0,    0.1, "m", true},
    {"GPS_TYPE",   "GPS model",         TYPE_INT,   0, 0, 0, 4, 1, "", true},
    {"IMU_TYPE",   "IMU model",         TYPE_INT,   1, 1, 0, 4, 1, "", true},
    {"AnchorEnabled",  "Enable anchor", TYPE_INT,   0,0, 0,    1,    1, "", false},

    // Добавляй новые параметры сюда!
};

static const int count = sizeof(entries)/sizeof(entries[0]);

const char* gpsTypeNames[] = { "TinyGPS", "NEO-M8N", "UBlox", "Sim28", "Other" };
const char* imuTypeNames[] = { "BNO055", "MPU9250", "BNO085", "LSM9DS1", "None" };

class Settings {
public:
    static constexpr int VERSION = 0x03;
    static const int EEPROM_ADDR = 256;

    struct KeyIdx { const char* key; int idx; };
    KeyIdx keyMap[count];

    // --- Индекс по ключу ---
    int idxByKey(const char* key) {
        for (int i = 0; i < count; i++)
            if (strcmp(keyMap[i].key, key) == 0)
                return keyMap[i].idx;
        return -1;
    }

    // --- Быстро построить map (после load/reset) ---
    void buildKeyMap() {
        for (int i = 0; i < count; i++) {
            keyMap[i].key = entries[i].key;
            keyMap[i].idx = i;
        }
    }

    // --- Получить значение ---
    float get(const char* key) { 
        int idx = idxByKey(key);
        return (idx >= 0) ? entries[idx].value : 0.0;
    }
    // --- Задать значение ---
    bool set(const char* key, float value) {
        int idx = idxByKey(key);
        if (idx >= 0) {
            if (value < entries[idx].minValue) value = entries[idx].minValue;
            if (value > entries[idx].maxValue) value = entries[idx].maxValue;
            entries[idx].value = value;
            return true;
        }
        return false;
    }

    // --- Сбросить ко всем дефолтам ---
    void reset() {
        for (int i = 0; i < count; i++)
            entries[i].value = entries[i].defaultValue;
        save();
        buildKeyMap();
    }

    // --- Сохранить в EEPROM ---
    void save() {
        EEPROM.put(EEPROM_ADDR, VERSION);
        EEPROM.put(EEPROM_ADDR + sizeof(uint8_t), entries);
        EEPROM.commit();
    }

    // --- Загрузить из EEPROM (с автосбросом по версии) ---
    void load() {
        uint8_t v = 0;
        EEPROM.get(EEPROM_ADDR, v);
        if (v != VERSION) {
            reset();
        } else {
            EEPROM.get(EEPROM_ADDR + sizeof(uint8_t), entries);
            buildKeyMap();
        }
    }
};

constexpr int Settings::VERSION;
