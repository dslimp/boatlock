#pragma once
#include <Arduino.h>
#include <AccelStepper.h>
#include <math.h>
#include "Settings.h"
#include "Logger.h"

class StepperControl {
public:
    static constexpr int STEPS_PER_REV_28BYJ = 4096;
    static constexpr int DIRECTION_SIGN = -1;
    static constexpr unsigned long COIL_RELEASE_DELAY_MS = 1200;
    static constexpr long MIN_COMMAND_STEPS = 4;
    static constexpr float MAX_RELATIVE_STEER_DEG = 90.0f;
    AccelStepper stepper;
    Settings* settings;
    int stepsPerRev;
    bool busy = false;
    bool manual = false;
    float manualSpd = 0;
    long bowZeroSteps = 0;
    bool outputsEnabled = true;
    unsigned long idleSinceMs = 0;
    unsigned long lastClampLogMs = 0;

    // 28BYJ-48 + ULN2003 (HALF4WIRE). Pin order IN1, IN3, IN2, IN4 for AccelStepper.
    StepperControl(int in1Pin, int in2Pin, int in3Pin, int in4Pin)
        : stepper(AccelStepper::HALF4WIRE, in1Pin, in3Pin, in2Pin, in4Pin),
          settings(nullptr),
          stepsPerRev(STEPS_PER_REV_28BYJ) {}

    void attachSettings(Settings* s) { settings = s; }

    long normalizeSteps(long steps) const {
        if (stepsPerRev <= 0) return 0;
        long normalized = steps % stepsPerRev;
        if (normalized < 0) {
            normalized += stepsPerRev;
        }
        return normalized;
    }

    static float normalize180(float deg) {
        while (deg > 180.0f) deg -= 360.0f;
        while (deg < -180.0f) deg += 360.0f;
        return deg;
    }

    static int signOf(long v) {
        if (v > 0) return 1;
        if (v < 0) return -1;
        return 0;
    }

    void loadFromSettings() {
        if (!settings) return;
        stepper.setMaxSpeed(settings->get("StepMaxSpd"));
        stepper.setAcceleration(settings->get("StepAccel"));
        // Legacy STEP/DIR support removed: 28BYJ is fixed at 4096 steps/rev.
        stepsPerRev = STEPS_PER_REV_28BYJ;
        ensureOutputsEnabled();

        bowZeroSteps = normalizeSteps(lroundf(settings->get("Encoder0")));
        logMessage("[STEP] cfg maxSpd=%.0f accel=%.0f spr=%d\n",
                   settings->get("StepMaxSpd"),
                   settings->get("StepAccel"),
                   stepsPerRev);
        logMessage("[STEP] bow zero=%ld\n", bowZeroSteps);
    }

    void captureBowZero() {
        bowZeroSteps = normalizeSteps(stepper.currentPosition());
        if (settings) {
            settings->set("Encoder0", bowZeroSteps);
            settings->save();
        }
        logMessage("[STEP] bow calibrated zero=%ld current=%ld\n",
                   bowZeroSteps,
                   stepper.currentPosition());
    }

    void pointToBearing(float bearing, float heading) {
        if (!isfinite(bearing) || !isfinite(heading) || stepsPerRev <= 0) {
            return;
        }

        const float relativeDegRaw = normalize180(bearing - heading);
        const float relativeDeg =
            constrain(relativeDegRaw, -MAX_RELATIVE_STEER_DEG, MAX_RELATIVE_STEER_DEG);
        if (fabsf(relativeDegRaw - relativeDeg) > 0.01f) {
            const unsigned long nowMs = millis();
            if (lastClampLogMs == 0 || nowMs - lastClampLogMs >= 1000UL) {
                logMessage("[EVENT] STEER_CLAMP raw=%.1f clamped=%.1f limit=%.1f\n",
                           relativeDegRaw,
                           relativeDeg,
                           MAX_RELATIVE_STEER_DEG);
                lastClampLogMs = nowMs;
            }
        }
        const long relativeSteps =
            lroundf(relativeDeg / 360.0f * stepsPerRev) * DIRECTION_SIGN;
        const long targetNorm = normalizeSteps(bowZeroSteps + relativeSteps);
        const long currentAbs = stepper.currentPosition();
        const long currentNorm = normalizeSteps(currentAbs);

        long delta = targetNorm - currentNorm;
        const long half = stepsPerRev / 2;
        if (delta > half) {
            delta -= stepsPerRev;
        } else if (delta < -half) {
            delta += stepsPerRev;
        }

        const long pending = stepper.distanceToGo();
        if (llabs(delta) < MIN_COMMAND_STEPS) {
            // New target is essentially current heading: drop stale queued move.
            if (pending != 0) {
                const long holdPos = stepper.currentPosition();
                stepper.moveTo(holdPos);
            }
            busy = false;
            if (idleSinceMs == 0) {
                idleSinceMs = millis();
            }
            return;
        }

        // If target direction flipped, preempt old target immediately.
        if (pending != 0 && signOf(pending) != signOf(delta)) {
            const long holdPos = stepper.currentPosition();
            stepper.moveTo(holdPos);
        }

        ensureOutputsEnabled();
        const long basePos = stepper.currentPosition();
        const long targetAbs = basePos + delta;
        stepper.moveTo(targetAbs);
        busy = (stepper.distanceToGo() != 0);
        idleSinceMs = 0;
    }

    void startManual(int dir) {
        manual = true;
        ensureOutputsEnabled();
        float spd = settings ? settings->get("StepMaxSpd") : 1000;
        manualSpd = (dir < 0 ? -spd : spd) * DIRECTION_SIGN;
        stepper.setSpeed(manualSpd);
        idleSinceMs = 0;
    }

    void stopManual() {
        if (!manual) {
            return;
        }
        manual = false;
        manualSpd = 0;
        // Manual release must be an immediate hold, without trailing deceleration.
        const long nowPos = stepper.currentPosition();
        stepper.moveTo(nowPos);
        busy = false;
        idleSinceMs = millis();
    }

    void cancelMove() {
        const bool hadPendingTarget = (stepper.distanceToGo() != 0);
        // Hard-cancel any pending target (stepper.stop() decelerates and can keep moving).
        const long nowPos = stepper.currentPosition();
        stepper.moveTo(nowPos);
        busy = false;
        if (hadPendingTarget && idleSinceMs == 0) {
            idleSinceMs = millis();
        }
    }

    void run() {
        if (manual) {
            ensureOutputsEnabled();
            stepper.runSpeed();
            idleSinceMs = 0;
        } else {
            if (stepper.distanceToGo() != 0) {
                ensureOutputsEnabled();
                stepper.run();
                if (stepper.distanceToGo() != 0) {
                    busy = true;
                    idleSinceMs = 0;
                    return;
                }
                busy = false;
                if (idleSinceMs == 0) {
                    idleSinceMs = millis();
                }
                return;
            }

            busy = false;
            if (idleSinceMs == 0) {
                idleSinceMs = millis();
            }
            if (outputsEnabled && (millis() - idleSinceMs >= COIL_RELEASE_DELAY_MS)) {
                stepper.disableOutputs();
                outputsEnabled = false;
            }
        }
    }

    float relativeSteerDeg() {
        if (stepsPerRev <= 0) return 0.0f;
        const long currentNorm = normalizeSteps(stepper.currentPosition());
        long delta = currentNorm - bowZeroSteps;
        const long half = stepsPerRev / 2;
        if (delta > half) {
            delta -= stepsPerRev;
        } else if (delta < -half) {
            delta += stepsPerRev;
        }
        const float deg = ((float)delta * 360.0f / (float)stepsPerRev) / (float)DIRECTION_SIGN;
        return constrain(deg, -MAX_RELATIVE_STEER_DEG, MAX_RELATIVE_STEER_DEG);
    }

private:
    void ensureOutputsEnabled() {
        if (!outputsEnabled) {
            stepper.enableOutputs();
            outputsEnabled = true;
        }
    }
};
