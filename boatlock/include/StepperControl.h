#pragma once
#include <Arduino.h>
#include <AccelStepper.h>
#include <math.h>
#include "Settings.h"

class StepperControl {
public:
    float stepsPerRev = 200.0f;
    AccelStepper stepper;
    Settings* settings;

    StepperControl(int stepPin, int dirPin)
        : stepper(AccelStepper::DRIVER, stepPin, dirPin), settings(nullptr) {}

    void attachSettings(Settings* s) { settings = s; }

    void loadFromSettings() {
        if (!settings) return;
        stepper.setMaxSpeed(settings->get("StepMaxSpd"));
        stepper.setAcceleration(settings->get("StepAccel"));
        stepsPerRev = settings->get("StepSpr");
    }

    void moveToBearing(float bearing, float heading) {
        float diff = bearing - heading;
        if (diff > 180) diff -= 360;
        if (diff < -180) diff += 360;
        long targetSteps = lround(diff / 360.0f * stepsPerRev);
        stepper.moveTo(targetSteps);
        stepper.run();
    }
};
