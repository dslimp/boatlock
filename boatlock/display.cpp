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

  gfx->fillScreen(COLOR_BG_LIGHT);
  gfx->setTextColor(COLOR_TEXT_DARK);
  gfx->setTextSize(2);
  gfx->setCursor(16, 10);
  gfx->println("BoatLock");

  return true;
}

void display_clear() {
  gfx->fillScreen(COLOR_BG_LIGHT);
}

void display_draw_debug(const String &msg, int y) {
  gfx->fillScreen(COLOR_BG_LIGHT);
  gfx->setTextColor(COLOR_TEXT_DARK);
  gfx->setTextSize(1);
  gfx->setCursor(0, y);
  gfx->print("[D]");
  gfx->print(msg);
}

namespace {
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 320;
constexpr int TOP_BAR_H = 28;
constexpr int BOTTOM_BAR_H = 30;
constexpr int COMPASS_CX = SCREEN_W / 2;
constexpr int COMPASS_CY = 150;
constexpr int COMPASS_R = 84;
} // namespace

static void draw_compass(float heading, float anchorBearing) {
  gfx->fillCircle(COMPASS_CX, COMPASS_CY, COMPASS_R + 3, COLOR_PANEL);
  gfx->drawCircle(COMPASS_CX, COMPASS_CY, COMPASS_R, COLOR_TEXT_DARK);
  gfx->drawCircle(COMPASS_CX, COMPASS_CY, COMPASS_R - 22, COLOR_SILVER);
  for (int deg = 0; deg < 360; deg += 15) {
    float rad = (deg - 90.0f) * DEG_TO_RAD;
    int tickOuter = (deg % 90 == 0) ? COMPASS_R : COMPASS_R - 6;
    int tickInner = COMPASS_R - 10;
    int x1 = COMPASS_CX + static_cast<int>(tickOuter * cos(rad));
    int y1 = COMPASS_CY + static_cast<int>(tickOuter * sin(rad));
    int x2 = COMPASS_CX + static_cast<int>(tickInner * cos(rad));
    int y2 = COMPASS_CY + static_cast<int>(tickInner * sin(rad));
    gfx->drawLine(x1, y1, x2, y2, COLOR_SILVER);
  }
  gfx->setTextColor(COLOR_TEXT_DARK);
  gfx->setTextSize(1);
  gfx->setCursor(COMPASS_CX - 4, COMPASS_CY - COMPASS_R + 6);
  gfx->print("N");
  gfx->setCursor(COMPASS_CX - 4, COMPASS_CY + COMPASS_R - 14);
  gfx->print("S");
  gfx->setCursor(COMPASS_CX - COMPASS_R + 8, COMPASS_CY - 4);
  gfx->print("W");
  gfx->setCursor(COMPASS_CX + COMPASS_R - 12, COMPASS_CY - 4);
  gfx->print("E");

  float headRad = (heading - 90.0f) * DEG_TO_RAD;
  int hx = COMPASS_CX + static_cast<int>((COMPASS_R - 26) * cos(headRad));
  int hy = COMPASS_CY + static_cast<int>((COMPASS_R - 26) * sin(headRad));
  gfx->drawLine(COMPASS_CX, COMPASS_CY, hx, hy, COLOR_ACCENT);
  gfx->fillTriangle(hx, hy, hx - 6, hy + 10, hx + 6, hy + 10, COLOR_ACCENT);

  float anchorRad = (anchorBearing - 90.0f) * DEG_TO_RAD;
  int ax = COMPASS_CX + static_cast<int>((COMPASS_R - 10) * cos(anchorRad));
  int ay = COMPASS_CY + static_cast<int>((COMPASS_R - 10) * sin(anchorRad));
  gfx->drawLine(COMPASS_CX, COMPASS_CY, ax, ay, COLOR_WARNING);
  gfx->fillCircle(COMPASS_CX, COMPASS_CY, 4, COLOR_TEXT_DARK);
}

void display_draw_ui(bool gpsFix,
                     int satellites,
                     float speedKmh,
                     float heading,
                     float anchorBearing,
                     float distanceMeters,
                     float errorDeg,
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
  static float lastErrorDeg = -1.0f;
  static String lastMode;

  if (!ui_drawn || force) {
    gfx->fillScreen(COLOR_BG_LIGHT);
    gfx->fillRect(0, 0, SCREEN_W, TOP_BAR_H, COLOR_PANEL);
    gfx->fillRect(0, SCREEN_H - BOTTOM_BAR_H, SCREEN_W, BOTTOM_BAR_H, COLOR_PANEL);
    gfx->drawRect(0, 0, SCREEN_W, SCREEN_H, COLOR_SILVER);
    ui_drawn = true;
    lastMode = "";
    lastSatellites = -1;
    lastBattery = -1;
    lastSpeed = -1.0f;
    lastHeading = -1.0f;
    lastAnchorBearing = -1.0f;
    lastDistance = -1.0f;
    lastErrorDeg = -1.0f;
    lastGpsFix = !gpsFix;
  }

  if (batteryPercent != lastBattery) {
    gfx->fillRect(8, 4, 58, 20, COLOR_PANEL);
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
    gfx->fillRect(90, 4, 80, 20, COLOR_PANEL);
    gfx->setTextColor(gpsFix ? COLOR_TEXT_DARK : COLOR_WARNING);
    gfx->setTextSize(1);
    gfx->setCursor(92, 8);
    gfx->printf("%d sats", satellites);
    lastSatellites = satellites;
    lastGpsFix = gpsFix;
  }

  if (!lastMode.equals(mode)) {
    gfx->fillRect(170, 4, 66, 20, COLOR_PANEL);
    gfx->fillRoundRect(172, 6, 64, 16, 4, COLOR_ACCENT);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(176, 10);
    gfx->printf("MODE:%s", mode);
    lastMode = mode;
  }

  if (lastHeading < 0.0f || lastAnchorBearing < 0.0f ||
      fabs(heading - lastHeading) > 1.0f ||
      fabs(anchorBearing - lastAnchorBearing) > 1.0f) {
    gfx->fillRect(0, TOP_BAR_H, SCREEN_W,
                  SCREEN_H - TOP_BAR_H - BOTTOM_BAR_H, COLOR_BG_LIGHT);
    draw_compass(heading, anchorBearing);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(2);
    gfx->setCursor(COMPASS_CX - 24, TOP_BAR_H + 18);
    gfx->printf("%03d", static_cast<int>(heading));
    lastHeading = heading;
    lastAnchorBearing = anchorBearing;
  }

  if (fabs(speedKmh - lastSpeed) > 0.1f ||
      fabs(distanceMeters - lastDistance) > 0.1f ||
      fabs(errorDeg - lastErrorDeg) > 0.5f ||
      !lastMode.equals(mode)) {
    gfx->fillRect(0, SCREEN_H - BOTTOM_BAR_H, SCREEN_W, BOTTOM_BAR_H, COLOR_PANEL);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(1);
    gfx->setCursor(8, SCREEN_H - 20);
    gfx->printf("SPD %.1f km/h", speedKmh);
    gfx->setCursor(92, SCREEN_H - 20);
    gfx->printf("DIST %.1f m", distanceMeters);
    gfx->setCursor(174, SCREEN_H - 20);
    gfx->printf("ERR %.0f%c", errorDeg, 176);
    lastSpeed = speedKmh;
    lastDistance = distanceMeters;
    lastErrorDeg = errorDeg;
  }
}
