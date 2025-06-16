#pragma once
#include <TinyGPS++.h>
#include "Settings.h"

class AnchorControl {
public:
    float anchorLat = 0, anchorLng = 0;
    Settings* settings = nullptr;

    void attachSettings(Settings* s) { settings = s; }

    void saveAnchor(float lat, float lng) {
        anchorLat = lat;
        anchorLng = lng;
        if (settings) {
            settings->set("AnchorLat", lat);
            settings->set("AnchorLon", lng);
            settings->set("AnchorEnabled", 1);
            settings->save();
        }
    }

    void loadAnchor() {
        if (settings) {
            anchorLat = settings->get("AnchorLat");
            anchorLng = settings->get("AnchorLon");
        }
    }

    float distanceToAnchor(TinyGPSPlus &gps) {
        return TinyGPSPlus::distanceBetween(
            gps.location.lat(), gps.location.lng(), anchorLat, anchorLng);
    }

    float bearingToAnchor(TinyGPSPlus &gps) {
        return TinyGPSPlus::courseTo(
            gps.location.lat(), gps.location.lng(), anchorLat, anchorLng);
    }
};
