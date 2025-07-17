#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "Settings.h"

class MotorControl {
public:
    int pwmChannel;
    int dirPin1;
    int dirPin2;

    MotorControl() : pwmChannel(0), dirPin1(-1), dirPin2(-1) {}

    // Ссылки на PID в Settings (инициализируются в setup())
    float Kp, Ki, Kd;
    float previous_error = 0, integral = 0;
    unsigned long lastPidTime = 0;
    float integralLimit = 100.0f;

    // Параметры адаптации PID
    float angleTolerance;
    float distanceThreshold;

    // Таймер для автосохранения коэффициентов
    unsigned long lastSaveTime = 0;
    const unsigned long saveInterval = 120000;

    void setupPWM(int pin, int channel, int freq, int res) {
        pwmChannel = channel;
        ledcSetup(channel, freq, res);
        ledcAttachPin(pin, channel);
        ledcWrite(channel, 0);
    }

    void setDirPin(int pin) {
        dirPin1 = pin;
        dirPin2 = -1;
        pinMode(pin, OUTPUT);
    }

    void setDirPins(int pin1, int pin2) {
        dirPin1 = pin1;
        dirPin2 = pin2;
        pinMode(pin1, OUTPUT);
        pinMode(pin2, OUTPUT);
    }

    void loadPIDfromSettings() {
        Kp = settings.get("Kp");
        Ki = settings.get("Ki");
        Kd = settings.get("Kd");
        angleTolerance    = settings.get("AngTol");
        distanceThreshold = settings.get("DistTh");
    }

    void savePIDtoSettings(bool force = false) {
        // Сохраняем только если значения реально поменялись, либо по таймеру (force=true)
        static float lastKp = -1, lastKi = -1, lastKd = -1;
        if (force || fabs(Kp - lastKp) > 0.0005 || fabs(Ki - lastKi) > 0.0001 || fabs(Kd - lastKd) > 0.0005) {
            settings.set("Kp", Kp);
            settings.set("Ki", Ki);
            settings.set("Kd", Kd);
            settings.save();
            lastKp = Kp; lastKi = Ki; lastKd = Kd;
        }
    }

    // --- Self-adaptive PID внутри applyPID ---
    void applyPID(float dist) {
        unsigned long now = millis();
        float dt = (lastPidTime == 0) ? 0.0f : (now - lastPidTime) / 1000.0f;
        lastPidTime = now;

        float error = dist;
        integral += error * dt;
        integral = constrain(integral, -integralLimit, integralLimit);
        float derivative = (dt > 0.0f) ? (error - previous_error) / dt : 0.0f;
        float output = Kp * error + Ki * integral + Kd * derivative;
        previous_error = error;

        // --- Self-adaptive tuning ---
        float error_threshold = 5.0;
        float derivative_threshold = 2.0;
        float dKp = 0.01;
        float dKd = 0.005;
        float dKi = 0.0005;

        // Корректировка Kp
        if (fabs(error) > error_threshold)        Kp = min(Kp + dKp,  settings.get("Kp") * 4.0f); // лимит вверх
        else if (fabs(error) < angleTolerance)    Kp = max(Kp - dKp,  0.01f); // лимит вниз

        // Корректировка Kd (чтобы не “колбасило”)
        if (fabs(derivative) > derivative_threshold)  Kd = min(Kd + dKd,  settings.get("Kd") * 4.0f);
        else                                         Kd = max(Kd - dKd,  0.01f);

        // Корректировка Ki для долгих ошибок
        if (fabs(integral) > 50.0)  Ki = min(Ki + dKi, 0.2f);
        else                        Ki = max(Ki - dKi, 0.0f);

        // Автосохранение коэффициентов раз в saveInterval или при сильном изменении
        if (millis() - lastSaveTime > saveInterval) {
            savePIDtoSettings(true);
            lastSaveTime = millis();
        } else {
            savePIDtoSettings();
        }

        // Управление мотором
        int pwmValue = constrain((int)fabs(output), 0, 255);
        bool forward = output >= 0;
        if (dirPin2 >= 0) {
            digitalWrite(dirPin1, forward ? HIGH : LOW);
            digitalWrite(dirPin2, forward ? LOW : HIGH);
        } else if (dirPin1 >= 0) {
            digitalWrite(dirPin1, forward ? HIGH : LOW);
        }
        ledcWrite(pwmChannel, pwmValue);
    }

    void stop() {
        ledcWrite(pwmChannel, 0);
    }
};
