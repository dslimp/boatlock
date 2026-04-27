#pragma once
#include <Arduino.h>
#include <AccelStepper.h>
#include <math.h>
#include "Settings.h"
#include "Logger.h"

class StepperControl {
public:
    static constexpr int MOTOR_STEPS_PER_REV = 200;
    static constexpr int GEAR_RATIO = 36;
    static constexpr int STEPS_PER_REV = MOTOR_STEPS_PER_REV * GEAR_RATIO;
    static constexpr int DIRECTION_SIGN = -1;
    static constexpr unsigned long COIL_RELEASE_DELAY_MS = 1200;
    static constexpr long MIN_COMMAND_STEPS = 4;
    static constexpr unsigned int DRV8825_MIN_PULSE_WIDTH_US = 2;
    AccelStepper stepper;
    Settings* settings;
    int stepsPerRev;
    bool busy = false;
    bool manual = false;
    float manualSpd = 0;
    long bowZeroSteps = 0;
    bool outputsEnabled = true;
    unsigned long idleSinceMs = 0;
    bool idleTimerActive = false;

    StepperControl(int stepPin, int dirPin)
        : stepper(AccelStepper::DRIVER, stepPin, dirPin),
          settings(nullptr),
          stepsPerRev(STEPS_PER_REV) {
        stepper.setMinPulseWidth(DRV8825_MIN_PULSE_WIDTH_US);
    }

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
        const float normalized = fmodf(deg, 360.0f);
        if (normalized > 180.0f) return normalized - 360.0f;
        if (normalized < -180.0f) return normalized + 360.0f;
        return normalized;
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
        stepsPerRev = STEPS_PER_REV;
        ensureOutputsEnabled();
        clearIdleTimer();

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

        const float relativeDeg = normalize180(bearing - heading);
        pointToRelativeAngle(relativeDeg);
    }

    void pointToRelativeAngle(float relativeDeg) {
        if (!isfinite(relativeDeg) || stepsPerRev <= 0) {
            return;
        }

        manual = false;
        manualSpd = 0;
        const long relativeSteps =
            lroundf(normalize180(relativeDeg) / 360.0f * stepsPerRev) * DIRECTION_SIGN;
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
            startIdleTimerIfNeeded();
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
        clearIdleTimer();
    }

    void startManual(int dir) {
        if (dir == 0) {
            stopManual();
            return;
        }
        manual = true;
        ensureOutputsEnabled();
        float spd = settings ? settings->get("StepMaxSpd") : 1000;
        manualSpd = (dir < 0 ? -spd : spd) * DIRECTION_SIGN;
        stepper.setSpeed(manualSpd);
        clearIdleTimer();
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
        startIdleTimer();
    }

    void cancelMove() {
        const bool hadPendingTarget = (stepper.distanceToGo() != 0);
        const long nowPos = stepper.currentPosition();
        stepper.moveTo(nowPos);
        busy = false;
        if (hadPendingTarget || outputsEnabled) {
            startIdleTimerIfNeeded();
        }
    }

    void run() {
        if (manual) {
            ensureOutputsEnabled();
            stepper.runSpeed();
            clearIdleTimer();
        } else {
            if (stepper.distanceToGo() != 0) {
                ensureOutputsEnabled();
                stepper.run();
                if (stepper.distanceToGo() != 0) {
                    busy = true;
                    clearIdleTimer();
                    return;
                }
                busy = false;
                startIdleTimerIfNeeded();
                return;
            }

            busy = false;
            startIdleTimerIfNeeded();
            if (outputsEnabled && (millis() - idleSinceMs >= COIL_RELEASE_DELAY_MS)) {
                stepper.disableOutputs();
                outputsEnabled = false;
            }
        }
    }

private:
    void ensureOutputsEnabled() {
        if (!outputsEnabled) {
            stepper.enableOutputs();
            outputsEnabled = true;
        }
    }

    void startIdleTimer() {
        idleSinceMs = millis();
        idleTimerActive = true;
    }

    void startIdleTimerIfNeeded() {
        if (!idleTimerActive) {
            startIdleTimer();
        }
    }

    void clearIdleTimer() {
        idleSinceMs = 0;
        idleTimerActive = false;
    }
};
