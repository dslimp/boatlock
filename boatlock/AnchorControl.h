#pragma once
#include <TinyGPSPlus.h>
#include "Settings.h"
#include "Logger.h"

class AnchorControl {
public:
    float anchorLat = 0, anchorLon = 0, anchorHeading = 0;
    Settings* settings = nullptr;

    void attachSettings(Settings* s) { settings = s; }

    void saveAnchor(float lat, float lon, float heading, bool enableAnchor = true) {
        anchorLat = lat;
        anchorLon = lon;
        anchorHeading = heading;
        if (settings) {
            settings->set("AnchorLat", lat);
            settings->set("AnchorLon", lon);
            settings->set("AnchorHead", heading);
            settings->set("AnchorEnabled", enableAnchor ? 1 : 0);
            settings->save();
        }
        logMessage("[ANCHOR] saved lat=%.6f lon=%.6f head=%.1f enabled=%d\n",
                   lat,
                   lon,
                   heading,
                   enableAnchor ? 1 : 0);
    }

    void loadAnchor() {
        if (settings) {
            anchorLat = settings->get("AnchorLat");
            anchorLon = settings->get("AnchorLon");
            anchorHeading = settings->get("AnchorHead");
        }
        logMessage("[ANCHOR] loaded lat=%.6f lon=%.6f head=%.1f\n", anchorLat, anchorLon, anchorHeading);
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
