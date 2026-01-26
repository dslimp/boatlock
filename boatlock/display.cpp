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
constexpr int TOP_BAR_H = 28;
constexpr int BOTTOM_BAR_H = 30;
} // namespace

struct Layout {
  int screenW;
  int screenH;
  int topBarH;
  int bottomBarH;
  int compassCx;
  int compassCy;
  int compassR;
};

static Layout display_layout() {
  Layout layout;
  layout.screenW = gfx->width();
  layout.screenH = gfx->height();
  layout.topBarH = TOP_BAR_H;
  layout.bottomBarH = BOTTOM_BAR_H;
  int usableH = layout.screenH - layout.topBarH - layout.bottomBarH;
  int minSide = min(layout.screenW, usableH);
  layout.compassR = minSide / 2 - 6;
  if (layout.compassR < 40) {
    layout.compassR = 40;
  }
  layout.compassCx = layout.screenW / 2;
  layout.compassCy = layout.topBarH + (usableH / 2);
  return layout;
}

static void draw_compass(float heading, float anchorBearing, const Layout &layout) {
  gfx->fillCircle(layout.compassCx, layout.compassCy, layout.compassR + 3, COLOR_PANEL);
  gfx->drawCircle(layout.compassCx, layout.compassCy, layout.compassR, COLOR_TEXT_DARK);
  gfx->drawCircle(layout.compassCx, layout.compassCy, layout.compassR - 22, COLOR_SILVER);
  for (int deg = 0; deg < 360; deg += 15) {
    float rad = (deg - 90.0f) * DEG_TO_RAD;
    int tickOuter = (deg % 90 == 0) ? layout.compassR : layout.compassR - 6;
    int tickInner = layout.compassR - 10;
    int x1 = layout.compassCx + static_cast<int>(tickOuter * cos(rad));
    int y1 = layout.compassCy + static_cast<int>(tickOuter * sin(rad));
    int x2 = layout.compassCx + static_cast<int>(tickInner * cos(rad));
    int y2 = layout.compassCy + static_cast<int>(tickInner * sin(rad));
    gfx->drawLine(x1, y1, x2, y2, COLOR_SILVER);
  }
  gfx->setTextColor(COLOR_TEXT_DARK);
  gfx->setTextSize(1);
  gfx->setCursor(layout.compassCx - 4, layout.compassCy - layout.compassR + 6);
  gfx->print("N");
  gfx->setCursor(layout.compassCx - 4, layout.compassCy + layout.compassR - 14);
  gfx->print("S");
  gfx->setCursor(layout.compassCx - layout.compassR + 8, layout.compassCy - 4);
  gfx->print("W");
  gfx->setCursor(layout.compassCx + layout.compassR - 12, layout.compassCy - 4);
  gfx->print("E");

  float headRad = (heading - 90.0f) * DEG_TO_RAD;
  int hx = layout.compassCx + static_cast<int>((layout.compassR - 26) * cos(headRad));
  int hy = layout.compassCy + static_cast<int>((layout.compassR - 26) * sin(headRad));
  gfx->drawLine(layout.compassCx, layout.compassCy, hx, hy, COLOR_ACCENT);
  gfx->fillTriangle(hx, hy, hx - 6, hy + 10, hx + 6, hy + 10, COLOR_ACCENT);

  float anchorRad = (anchorBearing - 90.0f) * DEG_TO_RAD;
  int ax = layout.compassCx + static_cast<int>((layout.compassR - 10) * cos(anchorRad));
  int ay = layout.compassCy + static_cast<int>((layout.compassR - 10) * sin(anchorRad));
  gfx->drawLine(layout.compassCx, layout.compassCy, ax, ay, COLOR_WARNING);
  gfx->fillCircle(layout.compassCx, layout.compassCy, 4, COLOR_TEXT_DARK);
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

  Layout layout = display_layout();

  bool compassChanged = lastHeading < 0.0f || lastAnchorBearing < 0.0f ||
                        fabs(heading - lastHeading) > 1.0f ||
                        fabs(anchorBearing - lastAnchorBearing) > 1.0f;
  bool fullRedraw = !ui_drawn || force || compassChanged;

  if (fullRedraw) {
    gfx->fillScreen(COLOR_BG_LIGHT);
    gfx->fillRect(0, 0, layout.screenW, layout.topBarH, COLOR_PANEL);
    gfx->fillRect(0, layout.screenH - layout.bottomBarH,
                  layout.screenW, layout.bottomBarH, COLOR_PANEL);
    gfx->drawRect(0, 0, layout.screenW, layout.screenH, COLOR_SILVER);
    gfx->fillRect(0, layout.topBarH, layout.screenW,
                  layout.screenH - layout.topBarH - layout.bottomBarH, COLOR_BG_LIGHT);
    draw_compass(heading, anchorBearing, layout);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(2);
    gfx->setCursor(layout.compassCx - 24, layout.topBarH + 18);
    gfx->printf("%03d", static_cast<int>(heading));
    ui_drawn = true;
    lastMode = "";
    lastSatellites = -1;
    lastBattery = -1;
    lastSpeed = -1.0f;
    lastHeading = heading;
    lastAnchorBearing = anchorBearing;
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

  if (fabs(speedKmh - lastSpeed) > 0.1f ||
      fabs(distanceMeters - lastDistance) > 0.1f ||
      fabs(errorDeg - lastErrorDeg) > 0.5f ||
      !lastMode.equals(mode)) {
    gfx->fillRect(0, layout.screenH - layout.bottomBarH,
                  layout.screenW, layout.bottomBarH, COLOR_PANEL);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(1);
    gfx->setCursor(8, layout.screenH - 20);
    gfx->printf("SPD %.1f km/h", speedKmh);
    gfx->setCursor(92, layout.screenH - 20);
    gfx->printf("DIST %.1f m", distanceMeters);
    gfx->setCursor(174, layout.screenH - 20);
    gfx->printf("ERR %.0f%c", errorDeg, 176);
    lastSpeed = speedKmh;
    lastDistance = distanceMeters;
    lastErrorDeg = errorDeg;
  }
}
