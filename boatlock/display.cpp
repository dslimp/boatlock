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

namespace {
constexpr int TOP_BAR_H = 28;
constexpr int BOTTOM_BAR_H = 28;
constexpr int COMPASS_CX = 120;
constexpr int COMPASS_CY = 132;
constexpr int COMPASS_R = 80;
} // namespace

static void draw_compass(float heading, float anchorBearing) {
  gfx->fillCircle(COMPASS_CX, COMPASS_CY, COMPASS_R + 2, COLOR_PANEL);
  gfx->drawCircle(COMPASS_CX, COMPASS_CY, COMPASS_R, COLOR_TEXT_DARK);
  gfx->drawCircle(COMPASS_CX, COMPASS_CY, COMPASS_R - 18, COLOR_SILVER);
  gfx->setTextColor(COLOR_TEXT_DARK);
  gfx->setTextSize(1);
  gfx->setCursor(COMPASS_CX - 4, COMPASS_CY - COMPASS_R + 6);
  gfx->print("N");
  gfx->setCursor(COMPASS_CX - 4, COMPASS_CY + COMPASS_R - 12);
  gfx->print("S");
  gfx->setCursor(COMPASS_CX - COMPASS_R + 6, COMPASS_CY - 4);
  gfx->print("W");
  gfx->setCursor(COMPASS_CX + COMPASS_R - 10, COMPASS_CY - 4);
  gfx->print("E");

  float headRad = (heading - 90.0f) * DEG_TO_RAD;
  int hx = COMPASS_CX + static_cast<int>((COMPASS_R - 24) * cos(headRad));
  int hy = COMPASS_CY + static_cast<int>((COMPASS_R - 24) * sin(headRad));
  gfx->drawLine(COMPASS_CX, COMPASS_CY, hx, hy, COLOR_ACCENT);
  gfx->fillTriangle(hx, hy, hx - 5, hy + 8, hx + 5, hy + 8, COLOR_ACCENT);

  float anchorRad = (anchorBearing - 90.0f) * DEG_TO_RAD;
  int ax = COMPASS_CX + static_cast<int>((COMPASS_R - 10) * cos(anchorRad));
  int ay = COMPASS_CY + static_cast<int>((COMPASS_R - 10) * sin(anchorRad));
  gfx->drawLine(COMPASS_CX, COMPASS_CY, ax, ay, COLOR_WARNING);
}

void display_draw_ui(bool gpsFix,
                     int satellites,
                     float speedKmh,
                     float heading,
                     float anchorBearing,
                     float distanceMeters,
                     const char* mode,
                     int batteryPercent,
                     bool force) {
  static bool ui_drawn = false;
  static bool lastGpsFix = false;
  static int lastSatellites = -1;
  static int lastBattery = -1;
  static float lastSpeed = -1.0f;
  static float lastHeading = -1.0f;
  static float lastAnchorBearing = -1.0f;
  static float lastDistance = -1.0f;
  static String lastMode;

  if (!ui_drawn || force) {
    gfx->fillScreen(COLOR_BG_LIGHT);
    gfx->fillRect(0, 0, 240, TOP_BAR_H, COLOR_PANEL);
    gfx->fillRect(0, 240 - BOTTOM_BAR_H, 240, BOTTOM_BAR_H, COLOR_PANEL);
    gfx->drawRect(0, 0, 240, 240, COLOR_SILVER);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(1);
    gfx->setCursor(8, 8);
    gfx->println("BoatLock");
    ui_drawn = true;
    lastMode = "";
    lastSatellites = -1;
    lastBattery = -1;
    lastSpeed = -1.0f;
    lastHeading = -1.0f;
    lastAnchorBearing = -1.0f;
    lastDistance = -1.0f;
    lastGpsFix = !gpsFix;
  }

  if (batteryPercent != lastBattery) {
    gfx->fillRect(8, 4, 50, 20, COLOR_PANEL);
    gfx->drawRect(8, 4, 40, 16, COLOR_TEXT_DARK);
    gfx->fillRect(48, 8, 4, 8, COLOR_TEXT_DARK);
    int fillW = static_cast<int>(36 * constrain(batteryPercent, 0, 100) / 100.0f);
    gfx->fillRect(10, 6, fillW, 12, COLOR_ACCENT);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(1);
    gfx->setCursor(56, 8);
    gfx->printf("%d%%", constrain(batteryPercent, 0, 100));
    lastBattery = batteryPercent;
  }

  if (satellites != lastSatellites || gpsFix != lastGpsFix) {
    gfx->fillRect(110, 4, 70, 20, COLOR_PANEL);
    gfx->setTextColor(gpsFix ? COLOR_TEXT_DARK : COLOR_WARNING);
    gfx->setTextSize(1);
    gfx->setCursor(112, 8);
    gfx->printf("%d sats", satellites);
    lastSatellites = satellites;
    lastGpsFix = gpsFix;
  }

  if (!lastMode.equals(mode)) {
    gfx->fillRect(180, 4, 56, 20, COLOR_PANEL);
    gfx->fillRoundRect(182, 6, 54, 16, 4, COLOR_ACCENT);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(186, 10);
    gfx->print(mode);
    lastMode = mode;
  }

  if (fabs(heading - lastHeading) > 1.0f ||
      fabs(anchorBearing - lastAnchorBearing) > 1.0f) {
    gfx->fillRect(20, 40, 200, 185, COLOR_BG_LIGHT);
    draw_compass(heading, anchorBearing);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(2);
    gfx->setCursor(90, 50);
    gfx->printf("%03d", static_cast<int>(heading));
    lastHeading = heading;
    lastAnchorBearing = anchorBearing;
  }

  if (fabs(speedKmh - lastSpeed) > 0.1f ||
      fabs(distanceMeters - lastDistance) > 0.1f ||
      !lastMode.equals(mode)) {
    gfx->fillRect(0, 240 - BOTTOM_BAR_H, 240, BOTTOM_BAR_H, COLOR_PANEL);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(1);
    gfx->setCursor(8, 240 - 20);
    gfx->printf("SPD %.1f km/h", speedKmh);
    gfx->setCursor(96, 240 - 20);
    gfx->printf("DST %.1f m", distanceMeters);
    gfx->setCursor(176, 240 - 20);
    gfx->print(mode);
    lastSpeed = speedKmh;
    lastDistance = distanceMeters;
  }
}
