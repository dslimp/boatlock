#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include <math.h>
#include "Settings.h"

class MotorControl {
public:
    int pwmChannel;
    int dirPin1;
    int dirPin2;
    int pwmRaw = 0;
    bool autoThrustActive = false;

    static constexpr float AUTO_DEADBAND_M = 1.5f;
    static constexpr float AUTO_RAMP_SPAN_M = 12.0f;
    static constexpr int AUTO_MIN_PWM = 35;
    static constexpr int AUTO_MAX_PWM_PCT = 75;
    static constexpr float AUTO_RAMP_PCT_PER_SEC = 35.0f;
    static constexpr float AUTO_DIST_FILTER_ALPHA = 0.22f;
    static constexpr float AUTO_DIST_RATE_FILTER_ALPHA = 0.35f;
    static constexpr float AUTO_APPROACH_DAMP_MPS = 1.2f;
    static constexpr float AUTO_APPROACH_DAMP_MAX = 0.7f;
    static constexpr unsigned long AUTO_MIN_ON_MS = 1200;
    static constexpr unsigned long AUTO_MIN_OFF_MS = 900;

    MotorControl() : pwmChannel(0), dirPin1(-1), dirPin2(-1), pwmRaw(0) {}

    // Ссылки на PID в Settings (инициализируются в setup())
    float Kp = 0.0f;
    float Ki = 0.0f;
    float Kd = 0.0f;
    float previous_error = 0, integral = 0;
    unsigned long lastPidTime = 0;
    float integralLimit = 100.0f;
    float filteredPidError = 0.0f;
    float filteredPidDerivative = 0.0f;
    bool filteredPidErrorValid = false;
    bool filteredPidDerivativeValid = false;
    static constexpr float PID_ERROR_FILTER_ALPHA = 0.22f;
    static constexpr float PID_DERIV_FILTER_ALPHA = 0.25f;

    // Параметры адаптации PID
    float angleTolerance = 0.0f;
    float distanceThreshold = 0.0f;

    // Таймер для автосохранения коэффициентов
    unsigned long lastSaveTime = 0;
    const unsigned long saveInterval = 120000;
    float filteredAnchorDistance = 0.0f;
    bool filteredAnchorDistanceValid = false;
    float filteredDistanceRateMps = 0.0f;
    bool filteredDistanceRateValid = false;
    unsigned long filteredDistanceUpdatedMs = 0;
    unsigned long autoThrustStateChangedMs = 0;
    unsigned long autoRampUpdatedMs = 0;
    int autoPwmRaw = 0;

    void setupPWM(int pin, int channel, int freq, int res) {
        pwmChannel = channel;
        ledcSetup(pwmChannel, freq, res);
        ledcAttachPin(pin, pwmChannel);
        ledcWrite(pwmChannel, 0);
        pwmRaw = 0;
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
        distanceThreshold = settings.get("HoldRadius");
    }

    void savePIDtoSettings(bool force = false) {
        // Сохраняем только если значения реально поменялись и прошло достаточно времени
        static float lastKp = -1, lastKi = -1, lastKd = -1;
        static bool initialized = false;
        unsigned long now = millis();

        if (!initialized) {
            lastKp = Kp;
            lastKi = Ki;
            lastKd = Kd;
            lastSaveTime = now;
            initialized = true;
        }

        bool changed =
            fabs(Kp - lastKp) > 0.0005 ||
            fabs(Ki - lastKi) > 0.0001 ||
            fabs(Kd - lastKd) > 0.0005;

        bool shouldPersist = changed && (force || (now - lastSaveTime >= saveInterval));

        if (shouldPersist) {
            settings.set("Kp", Kp);
            settings.set("Ki", Ki);
            settings.set("Kd", Kd);
            settings.save();
            lastKp = Kp; lastKi = Ki; lastKd = Kd;
            lastSaveTime = now;
        }
    }

    // --- Self-adaptive PID внутри applyPID ---
    void applyPID(float dist) {
        unsigned long now = millis();
        float dt = (lastPidTime == 0) ? 0.0f : (now - lastPidTime) / 1000.0f;
        lastPidTime = now;

        float errorRaw = dist;
        if (!filteredPidErrorValid) {
            filteredPidError = errorRaw;
            filteredPidErrorValid = true;
        } else {
            filteredPidError =
                filteredPidError + (errorRaw - filteredPidError) * PID_ERROR_FILTER_ALPHA;
        }
        const float error = filteredPidError;

        float derivativeRaw = (dt > 0.0f) ? (error - previous_error) / dt : 0.0f;
        if (!filteredPidDerivativeValid) {
            filteredPidDerivative = derivativeRaw;
            filteredPidDerivativeValid = true;
        } else {
            filteredPidDerivative =
                filteredPidDerivative +
                (derivativeRaw - filteredPidDerivative) * PID_DERIV_FILTER_ALPHA;
        }
        const float derivative = filteredPidDerivative;
        previous_error = error;

        const float pTerm = Kp * error;
        const float dTerm = Kd * derivative;
        const float unsatCurrent = pTerm + Ki * integral + dTerm;
        const float satCurrent = constrain(unsatCurrent, -255.0f, 255.0f);
        const bool satHigh = satCurrent >= 254.5f;
        const bool satLow = satCurrent <= -254.5f;
        const bool pushesFurtherSaturation = (satHigh && error > 0.0f) || (satLow && error < 0.0f);
        if (!pushesFurtherSaturation && dt > 0.0f && isfinite(dt)) {
            integral += error * dt;
            integral = constrain(integral, -integralLimit, integralLimit);
        }

        float output = pTerm + Ki * integral + dTerm;
        output = constrain(output, -255.0f, 255.0f);

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

        // Автосохранение коэффициентов раз в saveInterval, даже при частых изменениях
        savePIDtoSettings(millis() - lastSaveTime > saveInterval);

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
        pwmRaw = pwmValue;
    }

    void stop() {
        ledcWrite(pwmChannel, 0);
        pwmRaw = 0;
        autoThrustActive = false;
        filteredAnchorDistance = 0.0f;
        filteredAnchorDistanceValid = false;
        autoPwmRaw = 0;
        autoRampUpdatedMs = 0;
        autoThrustStateChangedMs = millis();
        filteredDistanceRateValid = false;
        filteredDistanceUpdatedMs = 0;
        filteredDistanceRateMps = 0.0f;
    }

    void resetPidState() {
        previous_error = 0.0f;
        integral = 0.0f;
        lastPidTime = 0;
        filteredPidErrorValid = false;
        filteredPidDerivativeValid = false;
        filteredPidError = 0.0f;
        filteredPidDerivative = 0.0f;
    }

    int pwmPercent() const {
        return constrain((pwmRaw * 100 + 127) / 255, 0, 100);
    }

    void driveAnchorAuto(float distanceMeters,
                         float holdRadiusMeters,
                         bool allowThrust,
                         float deadbandMeters = AUTO_DEADBAND_M,
                         int maxThrustPct = AUTO_MAX_PWM_PCT,
                         float rampPctPerSec = AUTO_RAMP_PCT_PER_SEC) {
        const unsigned long now = millis();
        const float holdRadius = max(0.5f, holdRadiusMeters);
        const float deadband = constrain(deadbandMeters, 0.2f, 10.0f);
        const float idleRadius = holdRadius + deadband;

        if (!allowThrust || !isfinite(distanceMeters) || distanceMeters <= 0.0f) {
            stop();
            return;
        }

        if (!filteredAnchorDistanceValid) {
            filteredAnchorDistance = distanceMeters;
            filteredAnchorDistanceValid = true;
            filteredDistanceRateValid = false;
            filteredDistanceRateMps = 0.0f;
            filteredDistanceUpdatedMs = now;
        } else {
            const unsigned long dtMs =
                (filteredDistanceUpdatedMs == 0 || now < filteredDistanceUpdatedMs)
                    ? 0
                    : (now - filteredDistanceUpdatedMs);
            filteredDistanceUpdatedMs = now;
            const float prevFilteredDistance = filteredAnchorDistance;
            filteredAnchorDistance =
                filteredAnchorDistance +
                (distanceMeters - filteredAnchorDistance) * AUTO_DIST_FILTER_ALPHA;
            if (dtMs > 0) {
                const float dt = dtMs / 1000.0f;
                const float rawRate = (filteredAnchorDistance - prevFilteredDistance) / dt;
                if (!filteredDistanceRateValid) {
                    filteredDistanceRateMps = rawRate;
                    filteredDistanceRateValid = true;
                } else {
                    filteredDistanceRateMps =
                        filteredDistanceRateMps +
                        (rawRate - filteredDistanceRateMps) * AUTO_DIST_RATE_FILTER_ALPHA;
                }
            }
        }

        const bool shouldBeActive = filteredAnchorDistance >= idleRadius;
        const unsigned long minStateMs = autoThrustActive ? AUTO_MIN_ON_MS : AUTO_MIN_OFF_MS;
        const bool canToggleState = (now - autoThrustStateChangedMs) >= minStateMs;
        if (shouldBeActive != autoThrustActive && canToggleState) {
            autoThrustActive = shouldBeActive;
            autoThrustStateChangedMs = now;
            if (!autoThrustActive) {
                stopAnchorOutput();
                return;
            }
        }

        if (!autoThrustActive) {
            stopAnchorOutput();
            return;
        }

        const float over = max(0.0f, filteredAnchorDistance - idleRadius);
        const float ratio = constrain(over / AUTO_RAMP_SPAN_M, 0.0f, 1.0f);
        const int safeMaxPct = constrain(maxThrustPct, 10, 100);
        const int maxPwm = constrain((safeMaxPct * 255 + 50) / 100, AUTO_MIN_PWM, 255);
        int targetPwm = AUTO_MIN_PWM + (int)lroundf(ratio * (maxPwm - AUTO_MIN_PWM));
        targetPwm = constrain(targetPwm, AUTO_MIN_PWM, maxPwm);
        if (filteredDistanceRateValid) {
            const float approachMps = max(0.0f, -filteredDistanceRateMps);
            const float nearFactor = 1.0f - ratio;
            const float damp =
                constrain((approachMps / AUTO_APPROACH_DAMP_MPS) * nearFactor, 0.0f, AUTO_APPROACH_DAMP_MAX);
            targetPwm = (int)lroundf(targetPwm * (1.0f - damp));
            targetPwm = constrain(targetPwm, 0, maxPwm);
        }

        if (autoRampUpdatedMs == 0) {
            autoRampUpdatedMs = now;
            autoPwmRaw = 0;
        }
        float dtSec = (now - autoRampUpdatedMs) / 1000.0f;
        if (!isfinite(dtSec) || dtSec < 0.0f) {
            dtSec = 0.0f;
        }
        if (dtSec > 1.0f) {
            dtSec = 1.0f;
        }
        autoRampUpdatedMs = now;

        const float safeRampPctPerSec = constrain(rampPctPerSec, 1.0f, 100.0f);
        const float rampRawPerSec = (safeRampPctPerSec / 100.0f) * 255.0f;
        int maxDelta = (int)lroundf(rampRawPerSec * dtSec);
        if (maxDelta < 1) {
            maxDelta = 1;
        }
        if (autoPwmRaw < targetPwm) {
            autoPwmRaw = min(autoPwmRaw + maxDelta, targetPwm);
        } else if (autoPwmRaw > targetPwm) {
            autoPwmRaw = max(autoPwmRaw - maxDelta, targetPwm);
        }

        if (dirPin2 >= 0) {
            digitalWrite(dirPin1, HIGH);
            digitalWrite(dirPin2, LOW);
        } else if (dirPin1 >= 0) {
            digitalWrite(dirPin1, HIGH);
        }

        ledcWrite(pwmChannel, autoPwmRaw);
        pwmRaw = autoPwmRaw;
    }

    void driveManual(int speed) {
        autoThrustActive = false;
        filteredAnchorDistanceValid = false;
        filteredDistanceRateValid = false;
        filteredDistanceRateMps = 0.0f;
        filteredDistanceUpdatedMs = 0;
        bool forward = speed >= 0;
        int pwmValue = constrain(abs(speed), 0, 255);
        if (dirPin2 >= 0) {
            digitalWrite(dirPin1, forward ? HIGH : LOW);
            digitalWrite(dirPin2, forward ? LOW : HIGH);
        } else if (dirPin1 >= 0) {
            digitalWrite(dirPin1, forward ? HIGH : LOW);
        }
        ledcWrite(pwmChannel, pwmValue);
        pwmRaw = pwmValue;
        autoPwmRaw = pwmValue;
        autoRampUpdatedMs = millis();
    }

private:
    void stopAnchorOutput() {
        ledcWrite(pwmChannel, 0);
        pwmRaw = 0;
        autoPwmRaw = 0;
        autoRampUpdatedMs = 0;
    }
};
