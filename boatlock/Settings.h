#pragma once
#include <EEPROM.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "Logger.h"

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
    // GNSS quality gate
    {"MinFixType", "Min fix type",      TYPE_INT,   3.0,  3.0,   2.0,   3.0,    1.0, "", false},
    {"MinSats",    "Min satellites",    TYPE_INT,   10.0,  10.0,   3.0,  20.0,    1.0, "", true},
    {"MaxHdop",    "Max HDOP",          TYPE_FLOAT, 1.8,  1.8,   0.5,  10.0,    0.1, "", true},
    {"MaxGpsAgeMs","Max GPS age",       TYPE_INT,   1500, 1500,  300, 20000,  100.0, "ms", false},
    {"MaxPosJumpM","Max GPS jump",      TYPE_FLOAT, 8.0, 8.0,  1.0, 200.0,    0.5, "m", false},
    {"MinStatSpd", "Min stationary spd",TYPE_FLOAT, 0.25, 0.25,  0.05, 2.0,    0.01, "m/s", false},
    {"SpdSanity",  "Speed sanity",      TYPE_INT,   0.0,  0.0,   0.0,   1.0,    1.0, "", false},
    {"MaxSpdMps",  "Max speed sanity",  TYPE_FLOAT, 25.0, 25.0,  1.0,  60.0,    0.5, "m/s", false},
    {"MaxAccMps2", "Max accel sanity",  TYPE_FLOAT, 6.0,  6.0,   0.5,  20.0,    0.1, "m/s2", false},
    {"ReqSent",    "Required sentences",TYPE_INT,   0.0,  0.0,   0.0,  20.0,    1.0, "", false},

    // Anchor control
    {"HoldRadius", "Hold radius",       TYPE_FLOAT, 2.5,  2.5,   0.5,  20.0,    0.1, "m", true},
    {"DeadbandM",  "Anchor deadband",   TYPE_FLOAT, 1.5,  1.5,   0.2,  10.0,    0.1, "m", true},
    {"MaxThrustA", "Max thrust anchor", TYPE_INT,   60.0, 60.0,  10.0, 100.0,   1.0, "%", true},
    {"ThrRampA",   "Thrust ramp anchor",TYPE_FLOAT, 35.0, 35.0,  1.0, 100.0,    1.0, "%/s", true},
    {"MaxTurnRt",  "Max turn rate",     TYPE_FLOAT, 120.0,120.0, 30.0, 720.0,   1.0, "deg/s", false},
    {"PidILim",    "PID I limit",       TYPE_FLOAT, 100.0,100.0, 1.0, 500.0,    1.0, "", false},
    {"ReacqStrat", "Reacquire strategy",TYPE_INT,   0.0,  0.0,   0.0,   1.0,    1.0, "", false},
    {"AnchorProf", "Anchor profile",    TYPE_INT,   1.0,  1.0,   0.0,   2.0,    1.0, "", true},

    // Safety supervisor
    {"CommToutMs", "Comm timeout",      TYPE_INT,   4000, 4000,  3000, 60000,  100.0, "ms", false},
    {"CtrlLoopMs", "Control loop tout", TYPE_INT,   200,  200,   100, 10000,   10.0, "ms", false},
    {"SensorTout", "Sensor timeout",    TYPE_INT,   1500, 1500,  300, 30000,  100.0, "ms", false},
    {"FailAct",    "Failsafe action",   TYPE_INT,   0.0,  0.0,   0.0,   1.0,    1.0, "", false},
    {"MaxThrustS", "Max thrust time",   TYPE_INT,   60,  60,    10,  3600,    1.0, "s", false},
    {"NanAct",     "NaN guard action",  TYPE_INT,   0.0,  0.0,   0.0,   1.0,    1.0, "", false},

    // UX / events
    {"GpsWeakHys", "GPS weak hysteresis", TYPE_FLOAT, 1.5, 1.5, 0.5, 60.0, 0.5, "s", false},
    {"EventRateMs","Event rate limit",  TYPE_INT,   1000, 1000, 100, 10000, 100.0, "ms", false},

    // Legacy/general params kept for compatibility
    {"Kp",        "Kp for PID",         TYPE_FLOAT, 20.0, 20.0,  0.01, 200.0,  0.1, "", true},
    {"Ki",        "Ki for PID",         TYPE_FLOAT, 0.5,  0.5,   0.0,  10.0,  0.01, "", true},
    {"Kd",        "Kd for PID",         TYPE_FLOAT, 5.0,  5.0,   0.0, 100.0,  0.1, "", true},
    {"DistTh",    "Distance threshold", TYPE_FLOAT, 2.0,  2.0,   0.1,  10.0,  0.1, "m", true},
    {"DriftAlert","Drift alert",        TYPE_FLOAT, 6.0,  6.0,   2.0, 200.0,  0.5, "m", true},
    {"DriftFail", "Drift fail",         TYPE_FLOAT, 12.0, 12.0,  5.0, 500.0,  1.0, "m", true},
    {"AngTol",    "Angle tolerance",    TYPE_FLOAT, 3.0,  3.0,   0.1,  180.0, 1.0, "deg", true},
    {"ScreenBr",  "Screen brightness",  TYPE_INT,   128.0,128.0, 0,    255,    1, "", true},
    {"Encoder0",  "Encoder zero",       TYPE_INT,   0,    0,     0,   16384,   1, "", false},
    {"USAngle",   "US angle",           TYPE_FLOAT, 30.0, 30.0,  0.0,  90.0,   1.0, "deg", false},
    {"USThresh",  "US threshold",       TYPE_FLOAT, 1.5,  1.5,   0.1,  6.0,    0.1, "m", false},
    {"GPS_TYPE",  "GPS model",          TYPE_INT,   0, 0, 0, 4, 1, "", false},
    {"IMU_TYPE",  "IMU model",          TYPE_INT,   1, 1, 0, 4, 1, "", false},
    {"GpsFWin",   "GPS filter window",  TYPE_INT,   5,    5,     1,   20,    1, "", false},
    {"DebugGps",  "Debug GPS serial",   TYPE_INT,   0,    0,     0,   1,     1, "", true},
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
    {"StepMaxSpd",   "Stepper max speed", TYPE_FLOAT, 700.0, 700.0, 100.0, 1500.0, 10.0, "", true},
    {"StepAccel",    "Stepper accel",     TYPE_FLOAT, 250.0, 250.0,  50.0, 1200.0, 10.0, "", true},
    {"StepSpr",      "Stepper steps/rev", TYPE_INT,   4096, 4096,  4096,  4096,   1, "", false},
};

static const int count = sizeof(defaultEntries) / sizeof(defaultEntries[0]);

const char* gpsTypeNames[] = { "TinyGPS", "NEO-M8N", "UBlox", "Sim28", "Other" };
const char* imuTypeNames[] = { "BNO055", "MPU9250", "BNO085", "LSM9DS1", "None" };

class Settings {
public:
    static constexpr uint8_t VERSION = 0x15;
    static const int EEPROM_ADDR = 256;
    static constexpr int VALUES_BYTES = sizeof(float) * count;
    static constexpr int VALUES_ADDR = EEPROM_ADDR + sizeof(uint8_t);
    static constexpr int CRC_ADDR = VALUES_ADDR + VALUES_BYTES;
    static constexpr int STORED_BYTES = sizeof(uint8_t) + VALUES_BYTES + sizeof(uint32_t);

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

    bool setStrict(const char* key, float value) {
        int idx = idxByKey(key);
        if (idx < 0) {
            return false;
        }
        if (!isfinite(value) || value < entries[idx].minValue || value > entries[idx].maxValue) {
            logMessage("[EVENT] CONFIG_REJECTED key=%s raw=%.5f\n", key, value);
            return false;
        }
        if (entries[idx].type == TYPE_INT) {
            value = roundf(value);
        } else if (entries[idx].type == TYPE_BOOL) {
            value = value >= 0.5f ? 1.0f : 0.0f;
        }
        entries[idx].value = value;
        return true;
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
        EEPROM.put(VALUES_ADDR, values);
        const uint32_t crc = calcCrc32(reinterpret_cast<const uint8_t*>(values), VALUES_BYTES);
        EEPROM.put(CRC_ADDR, crc);
        EEPROM.commit();
        logMessage("[EEPROM] settings saved ver=%d crc=0x%08lX\n",
                   VERSION,
                   static_cast<unsigned long>(crc));
    }

    void load() {
        for (int i = 0; i < count; i++)
            entries[i] = defaultEntries[i];

        uint8_t v = 0;
        bool shouldSave = false;
        EEPROM.get(EEPROM_ADDR, v);
        if (v == VERSION) {
            float values[count];
            uint32_t storedCrc = 0;
            EEPROM.get(VALUES_ADDR, values);
            EEPROM.get(CRC_ADDR, storedCrc);
            const uint32_t calcCrc =
                calcCrc32(reinterpret_cast<const uint8_t*>(values), VALUES_BYTES);
            if (storedCrc == calcCrc) {
                for (int i = 0; i < count; i++) {
                    float normalized = sanitizeLoadedValue(entries[i], values[i]);
                    if (normalized != values[i]) {
                        shouldSave = true;
                        logMessage("[EVENT] CONFIG_REJECTED key=%s raw=%.5f normalized=%.5f\n",
                                   entries[i].key,
                                   values[i],
                                   normalized);
                    }
                    entries[i].value = normalized;
                }
            } else {
                shouldSave = true;
                logMessage("[EVENT] CONFIG_CRC_FAIL stored=0x%08lX calc=0x%08lX\n",
                           static_cast<unsigned long>(storedCrc),
                           static_cast<unsigned long>(calcCrc));
            }
        } else {
            shouldSave = true;
            logMessage("[EVENT] CONFIG_MIGRATION from_ver=%d to_ver=%d\n", v, VERSION);
        }

        if (shouldSave) {
            save();
        }

        logMessage("[EEPROM] settings loaded (ver=%d)\n", v);

        buildKeyMap();
    }

private:
    static uint32_t calcCrc32(const uint8_t* data, size_t len) {
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int bit = 0; bit < 8; ++bit) {
                const uint32_t mask = static_cast<uint32_t>(-(static_cast<int>(crc & 1u)));
                crc = (crc >> 1) ^ (0xEDB88320u & mask);
            }
        }
        return ~crc;
    }

    static float sanitizeLoadedValue(const SettingEntry& entry, float raw) {
        if (!isfinite(raw)) {
            return entry.defaultValue;
        }

        float value = raw;
        if (entry.type == TYPE_BOOL) {
            value = value >= 0.5f ? 1.0f : 0.0f;
        } else if (entry.type == TYPE_INT) {
            value = roundf(value);
        }

        if (value < entry.minValue || value > entry.maxValue) {
            value = entry.defaultValue;
        }
        if (value < entry.minValue) {
            value = entry.minValue;
        }
        if (value > entry.maxValue) {
            value = entry.maxValue;
        }
        return value;
    }
};

extern Settings settings;

constexpr uint8_t Settings::VERSION;
