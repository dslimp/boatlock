#pragma once
#include <Arduino.h>
#include <AccelStepper.h>
#include <math.h>
#include "Settings.h"
#include "Logger.h"

class StepperControl {
public:
    static constexpr int DEFAULT_STEPS_PER_REV = 200;
    AccelStepper stepper;
    Settings* settings;
    int stepsPerRev;
    bool busy = false;
    bool manual = false;
    float manualSpd = 0;

    StepperControl(int stepPin, int dirPin)
        : stepper(AccelStepper::DRIVER, stepPin, dirPin), settings(nullptr),
          stepsPerRev(DEFAULT_STEPS_PER_REV) {}

    void attachSettings(Settings* s) { settings = s; }

    void loadFromSettings() {
        if (!settings) return;
        stepper.setMaxSpeed(settings->get("StepMaxSpd"));
        stepper.setAcceleration(settings->get("StepAccel"));
        stepsPerRev = (int)settings->get("StepSpr");
        logMessage("[STEP] cfg maxSpd=%.0f accel=%.0f spr=%d\n",
                   settings->get("StepMaxSpd"),
                   settings->get("StepAccel"),
                   stepsPerRev);
    }

    void moveToBearing(float bearing, float heading) {
        if (!busy || stepper.distanceToGo() == 0) {
            float diff = bearing - heading;
            if (diff > 180) diff -= 360;
            if (diff < -180) diff += 360;
            long targetSteps = lround(diff / 360.0f * stepsPerRev);
            stepper.move(targetSteps);
            logMessage("[STEP] move diff=%.1f steps=%ld\n", diff, targetSteps);
            busy = true;
        };
        if (stepper.distanceToGo() == 0) {
            busy = false;
        }
    }

    void startManual(int dir) {
        manual = true;
        float spd = settings ? settings->get("StepMaxSpd") : 1000;
        manualSpd = dir < 0 ? -spd : spd;
        stepper.setSpeed(manualSpd);
    }

    void stopManual() {
        manual = false;
        manualSpd = 0;
    }

    void run() {
        if (manual) {
            stepper.runSpeed();
        } else {
            stepper.run();
            if (stepper.distanceToGo() == 0) busy = false;
        }
    }
};
