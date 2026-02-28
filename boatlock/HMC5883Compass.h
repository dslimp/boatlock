#pragma once
#include <Wire.h>
#include <Arduino.h>
#include <Adafruit_HMC5883_U.h>
#include "Settings.h"

class HMC5883Compass {
public:
    Settings* settings = nullptr;

    bool init() {
        Wire.begin();
        Wire.beginTransmission(0x1E);
        if (Wire.endTransmission() != 0) {
            return false;
        }
        return mag.begin();
    }

    void attachSettings(Settings* s) { settings = s; }

    void loadCalibrationFromSettings() {
        if (!settings) return;
        setCalibrationOffsets(
            settings->get("MagOffX"),
            settings->get("MagOffY"),
            settings->get("MagOffZ"));
        setCalibrationScales(
            settings->get("MagScaleX"),
            settings->get("MagScaleY"),
            settings->get("MagScaleZ"));
    }

    void read() {
        sensors_event_t event;
        mag.getEvent(&event);

        float x = (event.magnetic.x - offsets[0]) * scales[0];
        float y = (event.magnetic.y - offsets[1]) * scales[1];

        float hdg = atan2(y, x);
        if(hdg < 0) hdg += 2 * PI;
        heading = hdg * 180.0f / PI;
    }

    float getAzimuth() const { return heading; }

    void calibrate() {
        float minX =  1e6, minY =  1e6, minZ =  1e6;
        float maxX = -1e6, maxY = -1e6, maxZ = -1e6;

        unsigned long start = millis();
        sensors_event_t event;
        while (millis() - start < 10000) { // 10 секунд на вращение
            mag.getEvent(&event);
            if (event.magnetic.x < minX) minX = event.magnetic.x;
            if (event.magnetic.x > maxX) maxX = event.magnetic.x;
            if (event.magnetic.y < minY) minY = event.magnetic.y;
            if (event.magnetic.y > maxY) maxY = event.magnetic.y;
            if (event.magnetic.z < minZ) minZ = event.magnetic.z;
            if (event.magnetic.z > maxZ) maxZ = event.magnetic.z;
            delay(20);
        }

        offsets[0] = (maxX + minX) / 2.0f;
        offsets[1] = (maxY + minY) / 2.0f;
        offsets[2] = (maxZ + minZ) / 2.0f;

        float scaleX = (maxX - minX) / 2.0f;
        float scaleY = (maxY - minY) / 2.0f;
        float scaleZ = (maxZ - minZ) / 2.0f;
        float avg = (scaleX + scaleY + scaleZ) / 3.0f;

        scales[0] = avg / scaleX;
        scales[1] = avg / scaleY;
        scales[2] = avg / scaleZ;

        saveCalibrationToSettings();
    }

    void setCalibrationOffsets(float x, float y, float z) {
        offsets[0] = x;
        offsets[1] = y;
        offsets[2] = z;
    }

    void setCalibrationScales(float x, float y, float z) {
        scales[0] = x;
        scales[1] = y;
        scales[2] = z;
    }

    void saveCalibrationToSettings() {
        if (!settings) return;
        settings->set("MagOffX", offsets[0]);
        settings->set("MagOffY", offsets[1]);
        settings->set("MagOffZ", offsets[2]);
        settings->set("MagScaleX", scales[0]);
        settings->set("MagScaleY", scales[1]);
        settings->set("MagScaleZ", scales[2]);
        settings->save();
    }

    float getCalibrationOffset(uint8_t idx) const { return offsets[idx]; }
    float getCalibrationScale(uint8_t idx) const { return scales[idx]; }

private:
    Adafruit_HMC5883_Unified mag{12345};
    float heading = 0.0f;
    float offsets[3] = {0.0f, 0.0f, 0.0f};
    float scales[3]  = {1.0f, 1.0f, 1.0f};
};
