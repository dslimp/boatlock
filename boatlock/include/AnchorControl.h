#pragma once
#include <TinyGPS++.h>
#include "Settings.h"

class AnchorControl {
public:
    float anchorLat = 0, anchorLon = 0, anchorHeading = 0;
    Settings* settings = nullptr;

    void attachSettings(Settings* s) { settings = s; }

    void saveAnchor(float lat, float lon, float heading) {
        anchorLat = lat;
        anchorLon = lon;
        anchorHeading = heading;
        if (settings) {
            settings->set("AnchorLat", lat);
            settings->set("AnchorLon", lon);
            settings->set("AnchorHead", heading);
            settings->set("AnchorEnabled", 1);
            settings->save();
        }
    }

    void loadAnchor() {
        if (settings) {
            anchorLat = settings->get("AnchorLat");
            anchorLon = settings->get("AnchorLon");
            anchorHeading = settings->get("AnchorHead");
        }
    }

    float distanceToAnchor(TinyGPSPlus &gps) {
        return TinyGPSPlus::distanceBetween(
            gps.location.lat(), gps.location.lng(), anchorLat, anchorLon);
    }

    float bearingToAnchor(TinyGPSPlus &gps) {
        return TinyGPSPlus::courseTo(
            gps.location.lat(), gps.location.lng(), anchorLat, anchorLon);
    }
};
