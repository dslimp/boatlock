#pragma once
#include <AccelStepper.h>
#include <Arduino.h>

class MotorControl {
public:
    int pwmChannel;
    int dirPin;
    float Kp, Ki, Kd, previous_error = 0, integral = 0;
    float angleTolerance = 3.0;
    float distanceThreshold = 2.0;

    void setupPWM(int pin, int channel, int freq, int res) {
        pwmChannel = channel;
        ledcSetup(channel, freq, res);
        ledcAttachPin(pin, channel);
        ledcWrite(channel, 0);
    }

    void setDirPin(int pin) {
        dirPin = pin;
        pinMode(pin, OUTPUT);
    }

    void applyPID(float dist) {
        float error = dist;
        integral += error;
        float derivative = error - previous_error;
        float output = Kp * error + Ki * integral + Kd * derivative;
        previous_error = error;
        int pwmValue = constrain((int)output, 0, 255);
        digitalWrite(dirPin, HIGH);
        ledcWrite(pwmChannel, pwmValue);
    }

    void stop() {
        ledcWrite(pwmChannel, 0);
    }
};
