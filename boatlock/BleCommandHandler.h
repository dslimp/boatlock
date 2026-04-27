#pragma once
#include <string>
#include "Settings.h"
#include "AnchorControl.h"
#include "StepperControl.h"
#include "MotorControl.h"
#include "AnchorProfiles.h"
#include "AnchorReasons.h"
#include "Logger.h"
#include "BNO08xCompass.h"
#include "BleOtaUpdate.h"
#include "RuntimeBleCommandScope.h"
#include "RuntimeBleOtaCommand.h"

extern AnchorControl anchor;
extern StepperControl stepperControl;
extern MotorControl motor;
extern Settings settings;
extern BNO08xCompass compass;
extern BleOtaUpdate bleOta;
void setPhoneGpsFix(float lat, float lon, float speedKmh, int satellites);
void captureStepperBowZero();
bool canEnableAnchorNow();
bool hasAnchorPoint();
void stopAllMotionNow();
void noteControlActivityNow();
bool nudgeAnchorCardinal(const char* dir);
bool nudgeAnchorBearing(float bearingDeg);
const char* currentGnssFailReason();
AnchorDeniedReason currentAnchorEnableDeniedReason();
AnchorDeniedReason currentGnssDeniedReason();
void setLastAnchorDeniedReason(AnchorDeniedReason reason);
void setLastFailsafeReason(FailsafeReason reason);
void clearSafeHold();
bool setManualControlFromBle(float angleDeg, int throttlePct, unsigned long ttlMs);
void stopManualControlFromBle();
bool preprocessSecureCommand(const std::string& incoming, std::string* effective);
bool handleSimCommand(const std::string& command);
bool headingAvailable();

inline void handleBleCommand(const std::string& cmd) {
    std::string effectiveCmd;
    if (!preprocessSecureCommand(cmd, &effectiveCmd)) {
        return;
    }
    const std::string& command = effectiveCmd;

    const RuntimeBleCommandScope commandScope = runtimeBleClassifyCommand(command);
    const RuntimeBleCommandProfile commandProfile = runtimeBleActiveCommandProfile();
    if (!runtimeBleCommandScopeAllowedInProfile(commandScope, commandProfile)) {
        logMessage("[BLE] command rejected reason=profile profile=%s scope=%s command=%s\n",
                   runtimeBleCommandProfileName(commandProfile),
                   runtimeBleCommandScopeName(commandScope),
                   command.c_str());
        return;
    }

    noteControlActivityNow();

    if (command == "HEARTBEAT") {
        return;
    }

    RuntimeBleOtaBeginCommand otaBegin;
    if (runtimeBleParseOtaBeginCommand(command, &otaBegin)) {
        setLastFailsafeReason(FailsafeReason::STOP_CMD);
        settings.set("AnchorEnabled", 0);
        settings.save();
        stopManualControlFromBle();
        stepperControl.cancelMove();
        motor.stop();
        stopAllMotionNow();
        logMessage("[EVENT] OTA_BEGIN source=BLE size=%lu\n",
                   static_cast<unsigned long>(otaBegin.imageSize));
        bleOta.begin(otaBegin.imageSize, otaBegin.sha256Hex);
        return;
    }

    if (command.rfind("OTA_BEGIN:", 0) == 0) {
        logMessage("[OTA] begin rejected reason=bad_payload\n");
        return;
    }

    if (command == "OTA_FINISH") {
        bleOta.finish();
        return;
    }

    if (command == "OTA_ABORT") {
        bleOta.abort("ble_command");
        return;
    }

    if (bleOta.active()) {
        logMessage("[BLE] command rejected reason=ota_active command=%s\n", command.c_str());
        return;
    }

    if (handleSimCommand(command)) {
        return;
    }

    if (command == "STOP") {
        setLastFailsafeReason(FailsafeReason::STOP_CMD);
        stopAllMotionNow();
        logMessage("[EVENT] FAILSAFE_TRIGGERED reason=STOP_CMD\n");
        logMessage("[BLE] STOP accepted\n");
        return;
    }

    if (command == "ANCHOR_OFF") {
        setLastFailsafeReason(FailsafeReason::NONE);
        setLastAnchorDeniedReason(AnchorDeniedReason::NONE);
        clearSafeHold();
        settings.set("AnchorEnabled", 0);
        settings.save();
        stopManualControlFromBle();
        stepperControl.cancelMove();
        motor.stop();
        logMessage("[EVENT] ANCHOR_OFF reason=BLE_CMD\n");
        logMessage("[BLE] ANCHOR_OFF accepted\n");
        return;
    }

    if (command == "ANCHOR_ON") {
        const AnchorDeniedReason denyReason = currentAnchorEnableDeniedReason();
        if (denyReason != AnchorDeniedReason::NONE) {
            setLastAnchorDeniedReason(denyReason);
            logMessage("[EVENT] ANCHOR_DENIED reason=%s\n", anchorDeniedReasonString(denyReason));
            logMessage("[BLE] ANCHOR_ON rejected: safety gate blocked\n");
        } else {
            setLastAnchorDeniedReason(AnchorDeniedReason::NONE);
            setLastFailsafeReason(FailsafeReason::NONE);
            clearSafeHold();
            stopManualControlFromBle();
            settings.set("AnchorEnabled", 1);
            settings.save();
            logMessage("[EVENT] ANCHOR_ON source=BLE\n");
            logMessage("[BLE] ANCHOR_ON accepted\n");
        }
        return;
    }

    if (command.rfind("SET_ANCHOR:", 0) == 0) {
        float lat = 0, lon = 0;
        sscanf(command.c_str() + 11, "%f,%f", &lat, &lon);
        if (lat < -90.0f || lat > 90.0f || lon < -180.0f || lon > 180.0f) {
            logMessage("[BLE] Invalid anchor range: %.6f, %.6f\n", lat, lon);
            return;
        }
        // Save anchor point only; arm Anchor mode via explicit ANCHOR_ON.
        if (anchor.saveAnchor(lat, lon, headingAvailable() ? compass.getAzimuth() : 0.0f, false)) {
            logMessage("[BLE] Anchor point saved via BLE: %.6f, %.6f\n", lat, lon);
        } else {
            logMessage("[BLE] Anchor point rejected via BLE: %.6f, %.6f\n", lat, lon);
        }
    } else if (command.rfind("NUDGE_DIR:", 0) == 0) {
        char dir[12] = {0};
        char extra = 0;
        if (sscanf(command.c_str() + 10, "%11[^, \t\r\n] %c", dir, &extra) == 1) {
            if (nudgeAnchorCardinal(dir)) {
                logMessage("[BLE] NUDGE_DIR accepted: %s\n", dir);
            } else {
                logMessage("[BLE] NUDGE_DIR rejected: %s\n", dir);
            }
        } else {
            logMessage("[BLE] Invalid NUDGE_DIR payload: %s\n", command.c_str());
        }
    } else if (command.rfind("NUDGE_BRG:", 0) == 0) {
        float bearingDeg = 0.0f;
        char extra = 0;
        if (sscanf(command.c_str() + 10, "%f %c", &bearingDeg, &extra) == 1) {
            if (nudgeAnchorBearing(bearingDeg)) {
                logMessage("[BLE] NUDGE_BRG accepted: %.1f\n", bearingDeg);
            } else {
                logMessage("[BLE] NUDGE_BRG rejected: %.1f\n", bearingDeg);
            }
        } else {
            logMessage("[BLE] Invalid NUDGE_BRG payload: %s\n", command.c_str());
        }
    } else if (command.rfind("SET_HOLD_HEADING:", 0) == 0) {
        int val = atoi(command.c_str() + 17);
        settings.set("HoldHeading", val);
        settings.save();
        logMessage("[BLE] HoldHeading set to %d\n", val);
    } else if (command.rfind("SET_ANCHOR_PROFILE:", 0) == 0) {
        AnchorProfileId profile = AnchorProfileId::NORMAL;
        const char* payload = command.c_str() + 19;
        if (!parseAnchorProfile(payload, &profile)) {
            logMessage("[BLE] SET_ANCHOR_PROFILE rejected: invalid payload '%s'\n", payload);
            return;
        }
        if (!applyAnchorProfile(settings, profile)) {
            logMessage("[EVENT] PROFILE_REJECTED profile=%s reason=apply_failed\n", payload);
            logMessage("[BLE] SET_ANCHOR_PROFILE rejected: apply failed\n");
            return;
        }
        logMessage("[EVENT] PROFILE_APPLIED profile=%s source=BLE\n", anchorProfileName(profile));
        logMessage("[BLE] SET_ANCHOR_PROFILE accepted: %s\n", anchorProfileName(profile));
    } else if (command.rfind("SET_COMPASS_OFFSET:",0) == 0) {
        float off = atof(command.c_str() + 19);
        compass.setHeadingOffsetDeg(off);
        settings.set("MagOffX", compass.getHeadingOffsetDeg());
        settings.save();
        logMessage("[BLE] CompassOffset set to %.1f deg\n", compass.getHeadingOffsetDeg());
    } else if (command == "RESET_COMPASS_OFFSET") {
        compass.setHeadingOffsetDeg(0.0f);
        settings.set("MagOffX", 0.0f);
        settings.save();
        logMessage("[BLE] CompassOffset reset\n");
    } else if (command == "COMPASS_CAL_START") {
        const bool ok = compass.startDynamicCalibration(false);
        logMessage("[EVENT] COMPASS_CAL_START ok=%d source=%s\n", ok ? 1 : 0, compass.sourceName());
    } else if (command == "COMPASS_DCD_SAVE") {
        const bool ok = compass.saveDynamicCalibration();
        logMessage("[EVENT] COMPASS_DCD_SAVE ok=%d source=%s\n", ok ? 1 : 0, compass.sourceName());
    } else if (command == "COMPASS_DCD_AUTOSAVE_ON") {
        const bool ok = compass.setDcdAutoSave(true);
        logMessage("[EVENT] COMPASS_DCD_AUTOSAVE enabled=1 ok=%d source=%s\n",
                   ok ? 1 : 0,
                   compass.sourceName());
    } else if (command == "COMPASS_DCD_AUTOSAVE_OFF") {
        const bool ok = compass.setDcdAutoSave(false);
        logMessage("[EVENT] COMPASS_DCD_AUTOSAVE enabled=0 ok=%d source=%s\n",
                   ok ? 1 : 0,
                   compass.sourceName());
    } else if (command == "COMPASS_TARE_Z") {
        const bool ok = compass.tareHeadingNow();
        logMessage("[EVENT] COMPASS_TARE_Z ok=%d source=%s\n", ok ? 1 : 0, compass.sourceName());
    } else if (command == "COMPASS_TARE_SAVE") {
        const bool ok = compass.saveTare();
        logMessage("[EVENT] COMPASS_TARE_SAVE ok=%d source=%s\n", ok ? 1 : 0, compass.sourceName());
    } else if (command == "COMPASS_TARE_CLEAR") {
        const bool ok = compass.clearTare();
        logMessage("[EVENT] COMPASS_TARE_CLEAR ok=%d source=%s\n", ok ? 1 : 0, compass.sourceName());
    } else if (command.rfind("SET_PHONE_GPS:", 0) == 0) {
        float lat = 0.0f, lon = 0.0f, speedKmh = 0.0f;
        int satellites = 0;
        int parsed = sscanf(command.c_str() + 14, "%f,%f,%f,%d", &lat, &lon, &speedKmh, &satellites);
        if (parsed >= 2) {
            if (parsed < 3) {
                speedKmh = 0.0f;
            }
            if (parsed < 4) {
                satellites = 0;
            }
            if (lat >= -90.0f && lat <= 90.0f && lon >= -180.0f && lon <= 180.0f) {
                setPhoneGpsFix(lat, lon, speedKmh, satellites);
            } else {
                logMessage("[BLE] Invalid phone GPS range: %.6f, %.6f\n", lat, lon);
            }
        } else {
            logMessage("[BLE] Invalid phone GPS payload: %s\n", command.c_str());
        }
    } else if (command.rfind("SET_STEP_MAXSPD:",0) == 0) {
        float v = atof(command.c_str() + 16);
        settings.set("StepMaxSpd", v);
        settings.save();
        stepperControl.loadFromSettings();
    } else if (command.rfind("SET_STEP_ACCEL:",0) == 0) {
        float v = atof(command.c_str() + 15);
        settings.set("StepAccel", v);
        settings.save();
        stepperControl.loadFromSettings();
    } else if (command == "SET_STEPPER_BOW") {
        captureStepperBowZero();
    } else if (command.rfind("MANUAL_TARGET:", 0) == 0) {
        float angleDeg = 0.0f;
        int throttlePct = 0;
        unsigned long ttlMs = 0;
        if (sscanf(command.c_str() + 14, "%f,%d,%lu", &angleDeg, &throttlePct, &ttlMs) == 3 &&
            setManualControlFromBle(angleDeg, throttlePct, ttlMs)) {
        } else {
            logMessage("[BLE] MANUAL_TARGET rejected: %s\n", command.c_str());
        }
    } else if (command == "MANUAL_OFF") {
        stopManualControlFromBle();
        logMessage("[BLE] MANUAL_OFF accepted\n");
    } else {
        logMessage("[BLE] Unhandled command: %s\n", command.c_str());
    }
}
