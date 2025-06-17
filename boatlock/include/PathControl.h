#pragma once
#include <TinyGPSPlus.h>

struct Waypoint {
    float lat;
    float lon;
};

class PathControl {
public:
    static const int MAX_POINTS = 20;
    Waypoint points[MAX_POINTS];
    int numPoints = 0;
    int currentIndex = 0;
    bool active = false;

    void reset() {
        numPoints = 0;
        currentIndex = 0;
        active = false;
    }

    void addPoint(float lat, float lon) {
        if (numPoints < MAX_POINTS) {
            points[numPoints++] = {lat, lon};
        }
    }

    void start() {
        if (numPoints > 0) {
            currentIndex = 0;
            active = true;
        }
    }

    void stop() { active = false; }

    bool finished() const { return active && currentIndex >= numPoints; }

    void update(TinyGPSPlus& gps, float threshold = 5.0f) {
        if (!active || currentIndex >= numPoints) return;
        float d = distanceToCurrent(gps);
        if (d < threshold) {
            currentIndex++;
            if (currentIndex >= numPoints) {
                active = false;
            }
        }
    }

    float distanceToCurrent(TinyGPSPlus& gps) const {
        if (currentIndex >= numPoints) return 0.0f;
        return TinyGPSPlus::distanceBetween(
            gps.location.lat(), gps.location.lng(),
            points[currentIndex].lat, points[currentIndex].lon);
    }

    float bearingToCurrent(TinyGPSPlus& gps) const {
        if (currentIndex >= numPoints) return 0.0f;
        return TinyGPSPlus::courseTo(
            gps.location.lat(), gps.location.lng(),
            points[currentIndex].lat, points[currentIndex].lon);
    }
};
