#pragma once
#include <Arduino.h>
#include "Settings.h"

class UltrasonicAlert {
public:
    int trigPin, echoPin;
    Settings* settings;
    UltrasonicAlert() : trigPin(-1), echoPin(-1), settings(nullptr) {}

    void setPins(int trig, int echo) {
        trigPin = trig;
        echoPin = echo;
    }

    void setSettings(Settings* s) { settings = s; }

    void begin() {
        if (trigPin < 0 || echoPin < 0) return;
        pinMode(trigPin, OUTPUT);
        pinMode(echoPin, INPUT);
    }

    // Получить дистанцию в метрах
    float getDistance() {
        if (trigPin < 0 || echoPin < 0) return -1;
        digitalWrite(trigPin, LOW); delayMicroseconds(2);
        digitalWrite(trigPin, HIGH); delayMicroseconds(10);
        digitalWrite(trigPin, LOW);
        long duration = pulseIn(echoPin, HIGH, 30000); // 30 ms max
        if (duration == 0) return -1; // нет эха
        float distance = duration * 0.0343f / 2.0f / 100.0f; // метры
        return distance;
    }

    // Тревога, если ближе порога на текущем угле
    bool isAlert() {
        if (!settings) return false;
        float threshold = settings->get("USThresh");
        float angleDeg = settings->get("USAngle");
        float d = getDistance();
        if (d < 0) return false;
        // Пересчёт "прямой" дистанции в горизонтальную, если датчик под углом
        float projected = d * cos(radians(angleDeg));
        return projected < threshold;
    }

    // Для вывода на экран
    float lastDistance() {
        return getDistance();
    }
};
