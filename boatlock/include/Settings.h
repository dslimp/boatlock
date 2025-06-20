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

// 🔒 Константный эталон (не изменяется, хранится в flash)
const SettingEntry defaultEntries[] = {
    {"Kp",        "Kp for PID",         TYPE_FLOAT, 20.0, 20.0,  0.01, 200.0,  0.1, "", true},
    {"Ki",        "Ki for PID",         TYPE_FLOAT, 0.5,  0.5,   0.0,  10.0,  0.01, "", true},
    {"Kd",        "Kd for PID",         TYPE_FLOAT, 5.0,  5.0,   0.0, 100.0,  0.1, "", true},
    {"DistTh",    "Distance threshold", TYPE_FLOAT, 2.0,  2.0,   0.1,  10.0,  0.1, "m", true},
    {"AngTol",    "Angle tolerance",    TYPE_FLOAT, 3.0,  3.0,   0.1,  180.0, 1.0, "deg", true},
    {"ScreenBr",  "Screen brightness",  TYPE_INT,   128.0,128.0, 0,    255,    1, "", true},
    {"Encoder0",  "Encoder zero",       TYPE_INT,   0,    0,     0,    4095,   1, "", false},
    {"USAngle",   "US angle",           TYPE_FLOAT, 30.0, 30.0,  0.0,  90.0,   1.0, "deg", false},
    {"USThresh",  "US threshold",       TYPE_FLOAT, 1.5,  1.5,   0.1,  6.0,    0.1, "m", false},
    {"GPS_TYPE",  "GPS model",          TYPE_INT,   0, 0, 0, 4, 1, "", false},
    {"IMU_TYPE",  "IMU model",          TYPE_INT,   1, 1, 0, 4, 1, "", false},
    {"AnchorEnabled", "Enable anchor",     TYPE_INT,   0, 0, 0, 1, 1, "", false},
    {"AnchorLat",     "Anchor latitude",   TYPE_FLOAT, 0.0, 0.0, -90.0, 90.0,   0.000001, "", false},
    {"AnchorLon",     "Anchor longitude",  TYPE_FLOAT, 0.0, 0.0,-180.0, 180.0, 0.000001, "", false},
    {"AnchorHead",    "Anchor heading",    TYPE_FLOAT, 0.0, 0.0,   0.0, 360.0, 1.0, "deg", false},
    {"HoldHeading",   "Hold heading",      TYPE_INT,   0, 0, 0, 1, 1, "", true},
    {"MagOffX",      "Compass offset X",  TYPE_FLOAT, 0.0, 0.0, -5000.0, 5000.0, 1.0, "", false},
    {"MagOffY",      "Compass offset Y",  TYPE_FLOAT, 0.0, 0.0, -5000.0, 5000.0, 1.0, "", false},
    {"MagOffZ",      "Compass offset Z",  TYPE_FLOAT, 0.0, 0.0, -5000.0, 5000.0, 1.0, "", false},
    {"MagScaleX",    "Compass scale X",   TYPE_FLOAT, 1.0, 1.0, 0.1, 5.0, 0.01, "", false},
    {"MagScaleY",    "Compass scale Y",   TYPE_FLOAT, 1.0, 1.0, 0.1, 5.0, 0.01, "", false},
    {"MagScaleZ",    "Compass scale Z",   TYPE_FLOAT, 1.0, 1.0, 0.1, 5.0, 0.01, "", false},
    {"StepMaxSpd",   "Stepper max speed", TYPE_FLOAT, 1000.0, 1000.0, 10.0, 5000.0, 10.0, "", true},
    {"StepAccel",    "Stepper accel",     TYPE_FLOAT, 500.0, 500.0, 10.0, 5000.0, 10.0, "", true},
};

static const int count = sizeof(defaultEntries) / sizeof(defaultEntries[0]);

const char* gpsTypeNames[] = { "TinyGPS", "NEO-M8N", "UBlox", "Sim28", "Other" };
const char* imuTypeNames[] = { "BNO055", "MPU9250", "BNO085", "LSM9DS1", "None" };

class Settings {
public:
    static constexpr int VERSION = 0x07;
    static const int EEPROM_ADDR = 256;

    struct KeyIdx { const char* key; int idx; };
    KeyIdx keyMap[count];

    SettingEntry entries[count];  // ⚠️ Копия в RAM (безопасно для EEPROM)

    int idxByKey(const char* key) {
        for (int i = 0; i < count; i++)
            if (strcmp(keyMap[i].key, key) == 0)
                return keyMap[i].idx;
        return -1;
    }

    void buildKeyMap() {
        for (int i = 0; i < count; i++) {
            keyMap[i].key = entries[i].key;
            keyMap[i].idx = i;
        }
    }

    float get(const char* key) {
        int idx = idxByKey(key);
        return (idx >= 0) ? entries[idx].value : 0.0;
    }

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

    void reset() {
        for (int i = 0; i < count; i++)
            entries[i] = defaultEntries[i];
        save();
        buildKeyMap();
    }

    void save() {
        EEPROM.put(EEPROM_ADDR, VERSION);
        float values[count];
        for (int i = 0; i < count; i++)
            values[i] = entries[i].value;
        EEPROM.put(EEPROM_ADDR + sizeof(uint8_t), values);
        EEPROM.commit();
    }

    void load() {
        for (int i = 0; i < count; i++)
            entries[i] = defaultEntries[i];

        uint8_t v = 0;
        EEPROM.get(EEPROM_ADDR, v);
        if (v == VERSION) {
            float values[count];
            EEPROM.get(EEPROM_ADDR + sizeof(uint8_t), values);
            for (int i = 0; i < count; i++)
                entries[i].value = values[i];
        } else {
            save();
        }

        buildKeyMap();
    }
};

constexpr int Settings::VERSION;
