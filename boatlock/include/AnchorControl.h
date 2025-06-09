#pragma once
#include <TinyGPS++.h>
#include <EEPROM.h>

class AnchorControl {
public:
    float anchorLat = 0, anchorLng = 0;
    const int eepromAddr = 0;

    void saveAnchor(float lat, float lng) {
        anchorLat = lat;
        anchorLng = lng;
        EEPROM.put(eepromAddr, anchorLat);
        EEPROM.put(eepromAddr + sizeof(float), anchorLng);
        EEPROM.commit();
    }

    void loadAnchor() {
        EEPROM.get(eepromAddr, anchorLat);
        EEPROM.get(eepromAddr + sizeof(float), anchorLng);
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
