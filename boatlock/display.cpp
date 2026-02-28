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

  gfx->setRotation(1);
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
constexpr int TOP_BAR_H = 32;
constexpr int BOTTOM_BAR_H = 32;
constexpr int PANEL_RADIUS = 8;
} // namespace

struct Layout {
  int screenW;
  int screenH;
  int topBarH;
  int bottomBarH;
  int contentY;
  int contentH;
  int compassCx;
  int compassCy;
  int compassR;
  int panelX;
  int panelW;
  int panelH;
};

static Layout display_layout() {
  Layout layout;
  layout.screenW = gfx->width();
  layout.screenH = gfx->height();
  layout.topBarH = TOP_BAR_H;
  layout.bottomBarH = BOTTOM_BAR_H;
  layout.contentY = layout.topBarH;
  layout.contentH = layout.screenH - layout.topBarH - layout.bottomBarH;
  int minSide = min(layout.screenW / 2, layout.contentH);
  layout.compassR = minSide / 2 - 8;
  if (layout.compassR < 40) {
    layout.compassR = 40;
  }
  layout.compassCx = layout.compassR + 22;
  layout.compassCy = layout.contentY + (layout.contentH / 2);
  layout.panelX = layout.compassCx + layout.compassR + 16;
  layout.panelW = layout.screenW - layout.panelX - 12;
  layout.panelH = layout.contentH;
  return layout;
}

static void draw_arrow(float heading, int cx, int cy, int radius, uint16_t color) {
  float rad = (heading - 90.0f) * DEG_TO_RAD;
  float tipX = cx + cos(rad) * (radius - 10);
  float tipY = cy + sin(rad) * (radius - 10);
  float baseRad = rad + PI;
  float baseX = cx + cos(baseRad) * (radius - 34);
  float baseY = cy + sin(baseRad) * (radius - 34);
  float leftRad = rad + DEG_TO_RAD * 150.0f;
  float rightRad = rad - DEG_TO_RAD * 150.0f;
  int x1 = static_cast<int>(tipX);
  int y1 = static_cast<int>(tipY);
  int x2 = static_cast<int>(baseX + cos(leftRad) * 12);
  int y2 = static_cast<int>(baseY + sin(leftRad) * 12);
  int x3 = static_cast<int>(baseX + cos(rightRad) * 12);
  int y3 = static_cast<int>(baseY + sin(rightRad) * 12);
  gfx->fillTriangle(x1, y1, x2, y2, x3, y3, color);
  gfx->drawLine(cx, cy, x1, y1, color);
}

static void draw_compass(float heading, float anchorBearing, const Layout &layout) {
  gfx->fillCircle(layout.compassCx, layout.compassCy, layout.compassR + 6, COLOR_PANEL);
  gfx->drawCircle(layout.compassCx, layout.compassCy, layout.compassR + 2, COLOR_SILVER);
  gfx->drawCircle(layout.compassCx, layout.compassCy, layout.compassR - 18, COLOR_SILVER);
  for (int deg = 0; deg < 360; deg += 15) {
    float rad = (deg - 90.0f) * DEG_TO_RAD;
    int tickOuter = (deg % 90 == 0) ? layout.compassR + 2 : layout.compassR - 6;
    int tickInner = layout.compassR - 12;
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

  draw_arrow(heading, layout.compassCx, layout.compassCy, layout.compassR, COLOR_ACCENT);

  float anchorRad = (anchorBearing - 90.0f) * DEG_TO_RAD;
  int ax = layout.compassCx + static_cast<int>((layout.compassR - 8) * cos(anchorRad));
  int ay = layout.compassCy + static_cast<int>((layout.compassR - 8) * sin(anchorRad));
  gfx->drawLine(layout.compassCx, layout.compassCy, ax, ay, COLOR_WARNING);
  gfx->fillCircle(ax, ay, 4, COLOR_WARNING);
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
  bool fullRedraw = !ui_drawn || force;

  if (fullRedraw) {
    gfx->fillScreen(COLOR_BG_LIGHT);
    gfx->fillRoundRect(8, 4, layout.screenW - 16, layout.topBarH - 6,
                       PANEL_RADIUS, COLOR_PANEL);
    gfx->fillRoundRect(8, layout.screenH - layout.bottomBarH + 4,
                       layout.screenW - 16, layout.bottomBarH - 8,
                       PANEL_RADIUS, COLOR_PANEL);
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

  if (fullRedraw || compassChanged) {
    gfx->fillScreen(COLOR_BG_LIGHT);
    gfx->fillRect(0, layout.contentY, layout.screenW, layout.contentH, COLOR_BG_LIGHT);
    draw_compass(heading, anchorBearing, layout);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(2);
    gfx->setCursor(layout.compassCx - 24, layout.contentY + 6);
    gfx->printf("%03d%c", static_cast<int>(heading), 176);
    lastHeading = heading;
    lastAnchorBearing = anchorBearing;
  }

  if (batteryPercent != lastBattery) {
    gfx->fillRect(18, 8, 64, 18, COLOR_PANEL);
    gfx->drawRect(18, 10, 40, 12, COLOR_TEXT_DARK);
    gfx->fillRect(58, 12, 4, 8, COLOR_TEXT_DARK);
    int fillW = static_cast<int>(36 * constrain(batteryPercent, 0, 100) / 100.0f);
    gfx->fillRect(20, 12, fillW, 8, COLOR_ACCENT);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(1);
    gfx->setCursor(66, 12);
    gfx->printf("%d%%", constrain(batteryPercent, 0, 100));
    lastBattery = batteryPercent;
  }

  if (satellites != lastSatellites || gpsFix != lastGpsFix) {
    gfx->fillRect(layout.panelX, layout.contentY + 4, layout.panelW, 44, COLOR_PANEL);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(1);
    gfx->setCursor(layout.panelX + 8, layout.contentY + 8);
    gfx->print("GPS");
    gfx->setTextColor(gpsFix ? COLOR_TEXT_DARK : COLOR_WARNING);
    gfx->setTextSize(2);
    gfx->setCursor(layout.panelX + 8, layout.contentY + 22);
    gfx->printf("%d", satellites);
    gfx->setTextSize(1);
    gfx->setCursor(layout.panelX + 40, layout.contentY + 26);
    gfx->print(gpsFix ? "FIX" : "NO FIX");
    lastSatellites = satellites;
    lastGpsFix = gpsFix;
  }

  if (!lastMode.equals(mode)) {
    gfx->fillRect(layout.screenW - 94, 8, 76, 18, COLOR_PANEL);
    gfx->fillRoundRect(layout.screenW - 94, 8, 76, 18, 6, COLOR_ACCENT);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(layout.screenW - 88, 12);
    gfx->printf("%s", mode);
    lastMode = mode;
  }

  if (fabs(speedKmh - lastSpeed) > 0.1f ||
      fabs(distanceMeters - lastDistance) > 0.1f ||
      fabs(errorDeg - lastErrorDeg) > 0.5f ||
      !lastMode.equals(mode)) {
    int rowY = layout.contentY + 52;
    int availableH = layout.contentY + layout.contentH - rowY;
    int rowGap = 8;
    int rowH = (availableH - rowGap * 2) / 3;
    gfx->fillRect(layout.panelX, rowY, layout.panelW, rowH, COLOR_PANEL);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(1);
    gfx->setCursor(layout.panelX + 8, rowY + 8);
    gfx->print("SPEED");
    gfx->setTextSize(2);
    gfx->setCursor(layout.panelX + 8, rowY + 20);
    gfx->printf("%.1f", speedKmh);
    gfx->setTextSize(1);
    gfx->setCursor(layout.panelX + 74, rowY + 26);
    gfx->print("km/h");

    int row2Y = rowY + rowH + rowGap;
    gfx->fillRect(layout.panelX, row2Y, layout.panelW, rowH, COLOR_PANEL);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(1);
    gfx->setCursor(layout.panelX + 8, row2Y + 8);
    gfx->print("ANCHOR");
    gfx->setTextSize(2);
    gfx->setCursor(layout.panelX + 8, row2Y + 20);
    gfx->printf("%.1f", distanceMeters);
    gfx->setTextSize(1);
    gfx->setCursor(layout.panelX + 74, row2Y + 26);
    gfx->print("m");

    int row3Y = row2Y + rowH + rowGap;
    gfx->fillRect(layout.panelX, row3Y, layout.panelW, rowH, COLOR_PANEL);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(1);
    gfx->setCursor(layout.panelX + 8, row3Y + 8);
    gfx->print("ERROR");
    gfx->setTextSize(2);
    gfx->setCursor(layout.panelX + 8, row3Y + 20);
    gfx->printf("%.0f%c", errorDeg, 176);

    gfx->fillRoundRect(16, layout.screenH - layout.bottomBarH + 6,
                       layout.screenW - 32, layout.bottomBarH - 12,
                       PANEL_RADIUS, COLOR_PANEL);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(1);
    gfx->setCursor(24, layout.screenH - 20);
    gfx->printf("HDG %03d%c", static_cast<int>(heading), 176);
    gfx->setCursor(110, layout.screenH - 20);
    gfx->printf("BRG %03d%c", static_cast<int>(anchorBearing), 176);
    gfx->setCursor(200, layout.screenH - 20);
    gfx->printf("SAT %d", satellites);
    lastSpeed = speedKmh;
    lastDistance = distanceMeters;
    lastErrorDeg = errorDeg;
  }
}
