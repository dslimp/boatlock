#pragma once

#include <Arduino_GFX_Library.h>
#include "colors.h"

inline void draw_icon_anchor(Arduino_GFX *gfx, int x, int y) {
  gfx->drawCircle(x + 16, y + 18, 10, COLOR_WHITE);
  gfx->drawLine(x + 16, y + 4, x + 16, y + 28, COLOR_WHITE);
  gfx->drawLine(x + 6, y + 22, x + 26, y + 22, COLOR_WHITE);
  gfx->drawLine(x + 6, y + 22, x + 10, y + 30, COLOR_WHITE);
  gfx->drawLine(x + 26, y + 22, x + 22, y + 30, COLOR_WHITE);
}

inline void draw_icon_gps(Arduino_GFX *gfx, int x, int y) {
  gfx->drawCircle(x + 16, y + 18, 10, COLOR_WHITE);
  gfx->drawCircle(x + 16, y + 18, 4, COLOR_WHITE);
  gfx->drawLine(x + 16, y + 2, x + 16, y + 10, COLOR_WHITE);
  gfx->drawLine(x + 16, y + 26, x + 16, y + 34, COLOR_WHITE);
  gfx->drawLine(x + 2, y + 18, x + 10, y + 18, COLOR_WHITE);
  gfx->drawLine(x + 22, y + 18, x + 30, y + 18, COLOR_WHITE);
}

inline void draw_icon_settings(Arduino_GFX *gfx, int x, int y) {
  gfx->drawCircle(x + 16, y + 18, 12, COLOR_WHITE);
  gfx->drawCircle(x + 16, y + 18, 4, COLOR_WHITE);
  for (int i = 0; i < 8; ++i) {
    float angle = i * 45.0f * DEG_TO_RAD;
    int x1 = x + 16 + static_cast<int>(12 * cos(angle));
    int y1 = y + 18 + static_cast<int>(12 * sin(angle));
    int x2 = x + 16 + static_cast<int>(16 * cos(angle));
    int y2 = y + 18 + static_cast<int>(16 * sin(angle));
    gfx->drawLine(x1, y1, x2, y2, COLOR_WHITE);
  }
}
