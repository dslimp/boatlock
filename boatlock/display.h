#pragma once

#include <Arduino_GFX_Library.h>

Arduino_GFX *display_gfx();
bool display_init();
void display_flush_now();
void display_clear();
void display_draw_ui(bool gpsFix,
                     int satellites,
                     bool gpsFromPhone,
                     float speedKmh,
                     float heading,
                     bool headingValid,
                     bool headingFromPhone,
                     int compassQuality,
                     float anchorBearing,
                     float distanceMeters,
                     float errorDeg,
                     const char* mode,
                     int batteryPercent,
                     bool force = false);
void display_draw_debug(const String &msg, int y = 48);
