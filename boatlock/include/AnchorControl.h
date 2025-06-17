#pragma once
#include <TinyGPS++.h>
#include "Settings.h"

class AnchorControl {
public:
    float anchorLat = 0, anchorLng = 0, anchorHeading = 0;
    Settings* settings = nullptr;

    void attachSettings(Settings* s) { settings = s; }

    void saveAnchor(float lat, float lng, float heading) {
        anchorLat = lat;
        anchorLng = lng;
        anchorHeading = heading;
        if (settings) {
            settings->set("AnchorLat", lat);
            settings->set("AnchorLon", lng);
            settings->set("AnchorHead", heading);
            settings->set("AnchorEnabled", 1);
            settings->save();
        }
    }

    void loadAnchor() {
        if (settings) {
            anchorLat = settings->get("AnchorLat");
            anchorLng = settings->get("AnchorLon");
            anchorHeading = settings->get("AnchorHead");
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
