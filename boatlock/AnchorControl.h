#pragma once
#include <math.h>
#include <TinyGPSPlus.h>
#include "Settings.h"
#include "Logger.h"

class AnchorControl {
public:
    float anchorLat = 0, anchorLon = 0, anchorHeading = 0;
    Settings* settings = nullptr;

    void attachSettings(Settings* s) { settings = s; }

    bool saveAnchor(float lat, float lon, float heading, bool enableAnchor) {
        if (!validAnchorPoint(lat, lon) || !isfinite(heading)) {
            logMessage("[EVENT] ANCHOR_REJECTED reason=RANGE lat=%.6f lon=%.6f head=%.1f\n",
                       lat,
                       lon,
                       heading);
            return false;
        }

        heading = normalizeHeading(heading);
        if (settings) {
            const bool ok = settings->setStrict("AnchorLat", lat) &&
                            settings->setStrict("AnchorLon", lon) &&
                            settings->setStrict("AnchorHead", heading) &&
                            settings->setStrict("AnchorEnabled", enableAnchor ? 1.0f : 0.0f);
            if (!ok) {
                logMessage("[EVENT] ANCHOR_REJECTED reason=SETTINGS lat=%.6f lon=%.6f head=%.1f\n",
                           lat,
                           lon,
                           heading);
                return false;
            }
            settings->save();
        }
        anchorLat = lat;
        anchorLon = lon;
        anchorHeading = heading;
        logMessage("[ANCHOR] saved lat=%.6f lon=%.6f head=%.1f enabled=%d\n",
                   lat,
                   lon,
                   heading,
                   enableAnchor ? 1 : 0);
        return true;
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

    static bool validAnchorPoint(float lat, float lon) {
        return isfinite(lat) &&
               isfinite(lon) &&
               lat >= -90.0f &&
               lat <= 90.0f &&
               lon >= -180.0f &&
               lon <= 180.0f &&
               !(lat == 0.0f && lon == 0.0f);
    }

    static float normalizeHeading(float heading) {
        while (heading < 0.0f) heading += 360.0f;
        while (heading >= 360.0f) heading -= 360.0f;
        return heading;
    }
};
