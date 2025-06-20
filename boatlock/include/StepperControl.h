#pragma once
#include <Arduino.h>
#include <AccelStepper.h>
#include <math.h>
#include "Settings.h"

class StepperControl {
public:
    static constexpr int STEPS_PER_REV = 200;
    AccelStepper stepper;
    Settings* settings;
    bool busy = false;

    StepperControl(int stepPin, int dirPin)
        : stepper(AccelStepper::DRIVER, stepPin, dirPin), settings(nullptr) {}

    void attachSettings(Settings* s) { settings = s; }

    void loadFromSettings() {
        if (!settings) return;
        stepper.setMaxSpeed(settings->get("StepMaxSpd"));
        stepper.setAcceleration(settings->get("StepAccel"));
    }

    void moveToBearing(float bearing, float heading) {
        if (!busy || stepper.distanceToGo() == 0) {
            float diff = bearing - heading;
            if (diff > 180) diff -= 360;
            if (diff < -180) diff += 360;
            long targetSteps = lround(diff / 360.0f * STEPS_PER_REV);
            stepper.move(targetSteps);
            busy = true;
        }
    }

    void run() {
        stepper.run();
        if (stepper.distanceToGo() == 0) busy = false;
    }
};
