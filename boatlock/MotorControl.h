#pragma once
#include <Arduino.h>
#include <math.h>

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

    void stop() {
        writeIdleOutput();
        resetAutoState();
        autoThrustStateChangedMs = millis();
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
            const unsigned long dtMs = elapsedMs(now, filteredDistanceUpdatedMs);
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
        const bool canToggleState = elapsedMs(now, autoThrustStateChangedMs) >= minStateMs;
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
        float dtSec = elapsedMs(now, autoRampUpdatedMs) / 1000.0f;
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
        resetAutoState();
        autoThrustStateChangedMs = millis();
        int pwmValue = constrain(abs(speed), 0, 255);
        if (pwmValue == 0) {
            writeIdleOutput();
            return;
        }
        bool forward = speed >= 0;
        if (dirPin2 >= 0) {
            digitalWrite(dirPin1, forward ? HIGH : LOW);
            digitalWrite(dirPin2, forward ? LOW : HIGH);
        } else if (dirPin1 >= 0) {
            digitalWrite(dirPin1, forward ? HIGH : LOW);
        }
        ledcWrite(pwmChannel, pwmValue);
        pwmRaw = pwmValue;
    }

private:
    static unsigned long elapsedMs(unsigned long now, unsigned long then) {
        return now - then;
    }

    void resetAutoState() {
        autoThrustActive = false;
        filteredAnchorDistance = 0.0f;
        filteredAnchorDistanceValid = false;
        filteredDistanceRateValid = false;
        filteredDistanceRateMps = 0.0f;
        filteredDistanceUpdatedMs = 0;
        autoPwmRaw = 0;
        autoRampUpdatedMs = 0;
    }

    void writeIdleOutput() {
        if (dirPin2 >= 0) {
            digitalWrite(dirPin1, LOW);
            digitalWrite(dirPin2, LOW);
        } else if (dirPin1 >= 0) {
            digitalWrite(dirPin1, LOW);
        }
        ledcWrite(pwmChannel, 0);
        pwmRaw = 0;
    }

    void stopAnchorOutput() {
        writeIdleOutput();
        autoPwmRaw = 0;
        autoRampUpdatedMs = 0;
    }
};
