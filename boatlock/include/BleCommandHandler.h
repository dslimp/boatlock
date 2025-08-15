#pragma once
#include <string>
#include "Settings.h"
#include "AnchorControl.h"
#include "PathControl.h"
#include "StepperControl.h"
#include "MotorControl.h"
#include "Logger.h"
#include "HMC5883Compass.h"

extern AnchorControl anchor;
extern PathControl pathControl;
extern StepperControl stepperControl;
extern MotorControl motor;
extern Settings settings;
extern HMC5883Compass compass;
extern float emuHeading;
extern bool compassReady;
extern bool manualMode;
extern int manualDir;
extern int manualSpeed;
void startCompassCalibration();
void exportRouteLog();
void clearRouteLog();

inline void handleBleCommand(const std::string& cmd) {
    if (cmd.rfind("SET_ANCHOR:", 0) == 0) {
        float lat = 0, lon = 0;
        sscanf(cmd.c_str() + 11, "%f,%f", &lat, &lon);
        anchor.saveAnchor(lat, lon, settings.get("EmuCompass") ? emuHeading : (compassReady ? compass.getAzimuth() : 0.0f));
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
        float v = atof(cmd.c_str() + 16);
        settings.set("StepMaxSpd", v);
        settings.save();
        stepperControl.loadFromSettings();
    } else if (cmd.rfind("SET_STEP_ACCEL:",0) == 0) {
        float v = atof(cmd.c_str() + 15);
        settings.set("StepAccel", v);
        settings.save();
        stepperControl.loadFromSettings();
    } else if (cmd.rfind("MANUAL:",0) == 0) {
        bool newMode = atoi(cmd.c_str() + 7) != 0;
        if (newMode) {
            stepperControl.cancelMove();
        } else {
            stepperControl.stopManual();
            motor.stop();
            manualSpeed = 0;
            manualDir = -1;
        }
        manualMode = newMode;
    } else if (cmd.rfind("MANUAL_DIR:",0) == 0) {
        manualDir = atoi(cmd.c_str() + 11);
    } else if (cmd.rfind("MANUAL_SPEED:",0) == 0) {
        manualSpeed = atoi(cmd.c_str() + 12);
    } else if (cmd == "EXPORT_LOG") {
        exportRouteLog();
    } else if (cmd == "CLEAR_LOG") {
        clearRouteLog();
    } else {
        logMessage("[BLE] Unhandled command: %s\n", cmd.c_str());
    }
}
