#pragma once

#include <Arduino_GFX_Library.h>

Arduino_GFX *display_gfx();
bool display_init();
void display_clear();
void display_draw_ui(bool gpsFix, bool force = false);
void display_draw_debug(const String &msg, int y = 48);
