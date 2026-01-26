#pragma once

#include <Arduino_GFX_Library.h>

Arduino_GFX *display_gfx();
bool display_init();
void display_clear();
void display_draw_ui(bool gpsFix,
                     int satellites,
                     float speedKmh,
                     float heading,
                     float anchorBearing,
                     float distanceMeters,
                     const char* mode,
                     int batteryPercent,
                     bool force = false);
void display_draw_debug(const String &msg, int y = 48);
