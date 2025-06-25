#pragma once
#include <string>
#include "Settings.h"
#include "AnchorControl.h"
#include "PathControl.h"
#include "StepperControl.h"
#include "Logger.h"
#include "QMC5883LCompass.h"
#include <mbedtls/base64.h>

extern AnchorControl anchor;
extern PathControl pathControl;
extern StepperControl stepperControl;
extern Settings settings;
extern QMC5883LCompass compass;
extern float emuHeading;
void startOtaUpdate(size_t size);
void otaWriteChunk(const uint8_t* data, size_t len);
void finishOtaUpdate();
void startCompassCalibration();

inline void handleBleCommand(const std::string& cmd) {
    if (cmd.rfind("SET_ANCHOR:", 0) == 0) {
        float lat = 0, lon = 0;
        sscanf(cmd.c_str() + 11, "%f,%f", &lat, &lon);
        anchor.saveAnchor(lat, lon, settings.get("EmuCompass") ? emuHeading : compass.getAzimuth());
        logMessage("[BLE] Anchor set via BLE: %.6f, %.6f\n", lat, lon);
    } else if (cmd.rfind("SET_HOLD_HEADING:", 0) == 0) {
        int val = atoi(cmd.c_str() + 17);
        settings.set("HoldHeading", val);
        settings.save();
        logMessage("[BLE] HoldHeading set to %d\n", val);
    } else if (cmd.rfind("SET_ROUTE:",0) == 0) {
        pathControl.reset();
        const char* s = cmd.c_str() + 10;
        while (*s) {
            float lat=0, lon=0; int n=0;
            if (sscanf(s, "%f,%f%n", &lat, &lon, &n) == 2) {
                pathControl.addPoint(lat, lon);
                s += n;
                if (*s == ';') s++;
            } else break;
        }
        logMessage("[BLE] Route set with %d points\n", pathControl.numPoints);
    } else if (cmd == "START_ROUTE") {
        pathControl.start();
        settings.set("AnchorEnabled", 0);
    } else if (cmd == "STOP_ROUTE") {
        pathControl.stop();
    } else if (cmd == "CALIB_COMPASS") {
        startCompassCalibration();
    } else if (cmd.rfind("SET_HEADING:",0) == 0) {
        emuHeading = atof(cmd.c_str() + 12);
    } else if (cmd.rfind("EMU_COMPASS:",0) == 0) {
        int v = atoi(cmd.c_str() + 12);
        settings.set("EmuCompass", v);
        settings.save();
    } else if (cmd.rfind("SET_STEP_SPR:",0) == 0) {
        int v = atoi(cmd.c_str() + 13);
        settings.set("StepSpr", v);
        settings.save();
        stepperControl.loadFromSettings();
    } else if (cmd.rfind("SET_STEP_MAXSPD:",0) == 0) {
        float v = atof(cmd.c_str() + 15);
        settings.set("StepMaxSpd", v);
        settings.save();
        stepperControl.loadFromSettings();
    } else if (cmd.rfind("SET_STEP_ACCEL:",0) == 0) {
        float v = atof(cmd.c_str() + 15);
        settings.set("StepAccel", v);
        settings.save();
        stepperControl.loadFromSettings();
    } else if (cmd.rfind("OTA_BEGIN:",0) == 0) {
        size_t total = atoi(cmd.c_str() + 10);
        startOtaUpdate(total);
    } else if (cmd.rfind("OTA_DATA:",0) == 0) {
        const char* b64 = cmd.c_str() + 9;
        size_t len = strlen(b64);
        std::string bin;
        bin.resize(len*3/4 + 4);
        size_t outLen = 0;
        if (mbedtls_base64_decode((unsigned char*)bin.data(), bin.size(), &outLen,
                (const unsigned char*)b64, len) == 0) {
            otaWriteChunk((const uint8_t*)bin.data(), outLen);
        }
    } else if (cmd == "OTA_END") {
        finishOtaUpdate();
    } else {
        logMessage("[BLE] Unhandled command: %s\n", cmd.c_str());
    }
}
