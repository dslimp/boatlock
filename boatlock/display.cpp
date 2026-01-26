#include "display.h"
#include "colors.h"
#include "icons.h"
#include <Arduino.h>

// ===== LCD pins =====
#define LCD_BL   1
#define LCD_RST  0
#define LCD_CS   45
#define LCD_DC   42
#define LCD_MOSI 38
#define LCD_SCLK 39
#define LCD_MISO -1

static Arduino_DataBus *bus = new Arduino_ESP32SPI(
  LCD_DC,
  LCD_CS,
  LCD_SCLK,
  LCD_MOSI,
  LCD_MISO
);

static Arduino_GFX *gfx = new Arduino_ST7789(
  bus,
  LCD_RST,
  1,
  true,
  240,
  320
);

Arduino_GFX *display_gfx() {
  return gfx;
}

bool display_init() {
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  if (!gfx->begin()) {
    return false;
  }

  gfx->fillScreen(COLOR_BLACK);
  gfx->setTextColor(COLOR_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(16, 10);
  gfx->println("BoatLock");

  return true;
}

void display_clear() {
  gfx->fillScreen(COLOR_BLACK);
}

void display_draw_debug(const String &msg, int y) {
  gfx->fillScreen(COLOR_BLACK);
  gfx->setTextColor(COLOR_WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(0, y);
  gfx->print("[D]");
  gfx->print(msg);
}

void display_draw_ui(bool force) {
  static bool ui_drawn = false;
  if (ui_drawn && !force) {
    return;
  }
  ui_drawn = true;
  gfx->fillScreen(COLOR_BLACK);

  gfx->fillRect(0, 0, 240, 56, COLOR_NAVY);
  gfx->setTextColor(COLOR_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(16, 14);
  gfx->println("BoatLock");
  gfx->setTextSize(1);
  gfx->setCursor(16, 36);
  gfx->println("System ready");

  gfx->fillRoundRect(150, 14, 74, 28, 8, COLOR_TEAL);
  gfx->setTextColor(COLOR_WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(160, 22);
  gfx->println("ONLINE");

  gfx->fillRoundRect(16, 72, 208, 84, 12, COLOR_NAVY);
  gfx->setTextColor(COLOR_SILVER);
  gfx->setTextSize(1);
  gfx->setCursor(28, 84);
  gfx->println("Anchor");
  gfx->setCursor(28, 104);
  gfx->setTextColor(COLOR_WHITE);
  gfx->setTextSize(2);
  gfx->println("LOCKED");

  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_SILVER);
  gfx->setCursor(140, 84);
  gfx->println("GPS");
  gfx->setTextColor(COLOR_WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(140, 104);
  gfx->println("STABLE");

  gfx->fillRoundRect(16, 176, 64, 64, 14, COLOR_NAVY);
  gfx->fillRoundRect(88, 176, 64, 64, 14, COLOR_NAVY);
  gfx->fillRoundRect(160, 176, 64, 64, 14, COLOR_NAVY);

  draw_icon_anchor(gfx, 22, 184);
  draw_icon_gps(gfx,    94, 184);
  draw_icon_settings(gfx,166,184);

  gfx->setTextColor(COLOR_SILVER);
  gfx->setTextSize(1);
  gfx->setCursor(28, 246);
  gfx->println("Anchor");
  gfx->setCursor(102, 246);
  gfx->println("Locate");
  gfx->setCursor(176, 246);
  gfx->println("Settings");
}
