#include "display.h"
#include "colors.h"
#include <Arduino.h>
#include <cstring>
#include <math.h>

namespace {
struct DisplayProfile {
  int rotation;
  bool ips;
  int width;
  int height;
  int colOffset1;
  int rowOffset1;
  int colOffset2;
  int rowOffset2;
  uint32_t spiHz;
};

#if defined(BOATLOCK_BOARD_JC4832W535)
// JC48xxW535 boards use AXS15231B on a 4-wire QSPI bus.
constexpr int kLcdBacklightPin = 1;
constexpr int kLcdResetPin = -1;
constexpr int kLcdCsPin = 45;
constexpr int kLcdQspiSckPin = 47;
constexpr int kLcdQspiD0Pin = 21;
constexpr int kLcdQspiD1Pin = 48;
constexpr int kLcdQspiD2Pin = 40;
constexpr int kLcdQspiD3Pin = 39;
constexpr DisplayProfile kDisplayProfile = {0, false, 320, 480, 0, 0, 0, 0, 4000000};
#else
constexpr int kLcdBacklightPin = 1;
constexpr int kLcdResetPin = 0;
constexpr int kLcdCsPin = 45;
constexpr int kLcdDcPin = 42;
constexpr int kLcdMosiPin = 38;
constexpr int kLcdSclkPin = 39;
constexpr int kLcdMisoPin = -1;
constexpr DisplayProfile kDisplayProfile = {1, true, 240, 320, 0, 0, 0, 0, 16000000};
#endif

#if defined(BOATLOCK_BOARD_JC4832W535)
constexpr bool kUseCanvasCompositor = true;
#else
constexpr bool kUseCanvasCompositor = true;
#endif
constexpr bool kRotationSwapsAxes = (kDisplayProfile.rotation & 1) != 0;
constexpr int kCanvasWidth = kRotationSwapsAxes ? kDisplayProfile.height : kDisplayProfile.width;
constexpr int kCanvasHeight = kRotationSwapsAxes ? kDisplayProfile.width : kDisplayProfile.height;
} // namespace

#if defined(BOATLOCK_BOARD_JC4832W535)
static Arduino_ESP32QSPI bus(
  kLcdCsPin,
  kLcdQspiSckPin,
  kLcdQspiD0Pin,
  kLcdQspiD1Pin,
  kLcdQspiD2Pin,
  kLcdQspiD3Pin
);

static Arduino_AXS15231B panel(
  &bus,
  kLcdResetPin,
  kDisplayProfile.rotation,
  kDisplayProfile.ips,
  kDisplayProfile.width,
  kDisplayProfile.height,
  kDisplayProfile.colOffset1,
  kDisplayProfile.rowOffset1,
  kDisplayProfile.colOffset2,
  kDisplayProfile.rowOffset2
);
#else
static Arduino_ESP32SPI bus(
  kLcdDcPin,
  kLcdCsPin,
  kLcdSclkPin,
  kLcdMosiPin,
  kLcdMisoPin
);

static Arduino_ST7789 panel(
  &bus,
  kLcdResetPin,
  kDisplayProfile.rotation,
  kDisplayProfile.ips,
  kDisplayProfile.width,
  kDisplayProfile.height,
  kDisplayProfile.colOffset1,
  kDisplayProfile.rowOffset1,
  kDisplayProfile.colOffset2,
  kDisplayProfile.rowOffset2
);
#endif

static Arduino_Canvas canvas(kCanvasWidth, kCanvasHeight, &panel);
static Arduino_GFX *gfx = &panel;

static void display_flush() {
  if (gfx == &canvas) {
    canvas.flush();
  }
}

Arduino_GFX *display_gfx() {
  return gfx;
}

bool display_init() {
  pinMode(kLcdBacklightPin, OUTPUT);
  digitalWrite(kLcdBacklightPin, HIGH);

  if (!panel.begin(kDisplayProfile.spiHz)) {
    return false;
  }

  panel.setRotation(kDisplayProfile.rotation);

  if (kUseCanvasCompositor) {
    if (canvas.begin(GFX_SKIP_OUTPUT_BEGIN)) {
      canvas.setRotation(0);
      gfx = &canvas;
    } else {
      gfx = &panel;
    }
  } else {
    gfx = &panel;
  }

  gfx->fillScreen(COLOR_BG_LIGHT);
  gfx->setTextColor(COLOR_TEXT_DARK);
  gfx->setTextSize(2);
  gfx->setCursor(16, 10);
  gfx->println("BoatLock");
  display_flush();
  return true;
}

void display_clear() {
  gfx->fillScreen(COLOR_BG_LIGHT);
  display_flush();
}

void display_draw_debug(const String &msg, int y) {
  gfx->fillScreen(COLOR_BG_LIGHT);
  gfx->setTextColor(COLOR_TEXT_DARK);
  gfx->setTextSize(1);
  gfx->setCursor(0, y);
  gfx->print("[D]");
  gfx->print(msg);
  display_flush();
}

namespace {
constexpr int PANEL_RADIUS = 8;
constexpr int PANEL_RADIUS_COMPACT = 5;

struct Layout {
  int screenW;
  int screenH;
  bool compact;
  int margin;
  int gap;
  int panelRadius;
  int headerX;
  int headerY;
  int headerW;
  int headerH;
  int contentY;
  int contentH;
  int rightX;
  int rightY;
  int rightW;
  int rightH;
  int cardH;
  int compassCx;
  int compassCy;
  int compassR;
  int footerX;
  int footerY;
  int footerW;
  int footerH;
};

static int normalize_deg(float deg) {
  int value = static_cast<int>(roundf(deg));
  value %= 360;
  if (value < 0) {
    value += 360;
  }
  return value;
}

static float sanitize_non_negative(float value) {
  if (!isfinite(value) || value < 0.0f) {
    return 0.0f;
  }
  return value;
}

static float sanitize_signed(float value) {
  if (!isfinite(value)) {
    return 0.0f;
  }
  return value;
}

static Layout display_layout() {
  Layout layout{};
  layout.screenW = gfx->width();
  layout.screenH = gfx->height();
  layout.compact = (layout.screenH <= 170 || layout.screenW <= 260);
  layout.margin = layout.compact ? 3 : 6;
  layout.gap = layout.compact ? 3 : 6;
  layout.panelRadius = layout.compact ? PANEL_RADIUS_COMPACT : PANEL_RADIUS;

  layout.headerX = layout.margin;
  layout.headerY = layout.margin;
  layout.headerW = layout.screenW - layout.margin * 2;
  layout.headerH = layout.compact ? 20 : 28;

  layout.footerX = layout.margin;
  layout.footerH = layout.compact ? 16 : 28;
  layout.footerY = layout.screenH - layout.margin - layout.footerH;
  layout.footerW = layout.screenW - layout.margin * 2;

  layout.contentY = layout.headerY + layout.headerH + layout.gap;
  layout.contentH = layout.footerY - layout.gap - layout.contentY;

  layout.rightW = layout.compact
                      ? constrain(layout.screenW * 36 / 100, 82, 96)
                      : constrain(layout.screenW * 34 / 100, 104, 124);
  layout.rightX = layout.screenW - layout.margin - layout.rightW;
  layout.rightY = layout.contentY;
  layout.rightH = layout.contentH;
  layout.cardH = (layout.rightH - layout.gap * 2) / 3;

  int leftAreaX = layout.margin;
  int leftAreaW = layout.rightX - layout.gap - leftAreaX;
  layout.compassCx = leftAreaX + leftAreaW / 2;
  layout.compassCy = layout.contentY + layout.contentH / 2;
  int rByW = (leftAreaW - 12) / 2;
  int rByH = (layout.contentH - 12) / 2;
  int minRadius = layout.compact ? 24 : 44;
  layout.compassR = max(minRadius, min(rByW, rByH));
  return layout;
}

static void draw_panel(int x, int y, int w, int h, int radius, uint16_t fillColor) {
  gfx->fillRoundRect(x, y, w, h, radius, fillColor);
  gfx->drawRoundRect(x, y, w, h, radius, COLOR_SILVER);
}

static uint16_t mode_color(const char *mode) {
  if (strcmp(mode, "MANUAL") == 0) {
    return COLOR_WARNING;
  }
  if (strcmp(mode, "ANCHOR") == 0) {
    return COLOR_ACCENT;
  }
  if (strcmp(mode, "ROUTE") == 0) {
    return COLOR_TEAL;
  }
  return COLOR_SILVER;
}

static void draw_arrow(float heading, int cx, int cy, int radius, uint16_t color) {
  float rad = (heading - 90.0f) * DEG_TO_RAD;
  int tipDist = max(radius - 8, radius / 2);
  int tailDist = max(radius - 20, radius / 3);
  int wing = max(4, radius / 6);

  int x1 = cx + static_cast<int>(tipDist * cos(rad));
  int y1 = cy + static_cast<int>(tipDist * sin(rad));
  int xBase = cx + static_cast<int>(tailDist * cos(rad + PI));
  int yBase = cy + static_cast<int>(tailDist * sin(rad + PI));
  int x2 = xBase + static_cast<int>(wing * cos(rad + DEG_TO_RAD * 140.0f));
  int y2 = yBase + static_cast<int>(wing * sin(rad + DEG_TO_RAD * 140.0f));
  int x3 = xBase + static_cast<int>(wing * cos(rad - DEG_TO_RAD * 140.0f));
  int y3 = yBase + static_cast<int>(wing * sin(rad - DEG_TO_RAD * 140.0f));

  gfx->fillTriangle(x1, y1, x2, y2, x3, y3, color);
  gfx->drawLine(cx, cy, x1, y1, color);
}

static int compass_dynamic_radius(const Layout &layout) {
  int innerR = layout.compassR - (layout.compact ? 10 : 16);
  if (innerR < 10) {
    innerR = 10;
  }
  return innerR - 2;
}

static void draw_compass_static(const Layout &layout) {
  gfx->fillCircle(layout.compassCx, layout.compassCy, layout.compassR + 3, COLOR_PANEL);
  gfx->drawCircle(layout.compassCx, layout.compassCy, layout.compassR + 1, COLOR_SILVER);
  int innerR = layout.compassR - (layout.compact ? 10 : 16);
  if (innerR > 6) {
    gfx->drawCircle(layout.compassCx, layout.compassCy, innerR, COLOR_SILVER);
  }

  // Fixed bow marker: the boat nose is always "up" on this dial.
  int bowY = layout.compassCy - layout.compassR - (layout.compact ? 1 : 2);
  gfx->fillTriangle(layout.compassCx,
                    bowY - (layout.compact ? 4 : 6),
                    layout.compassCx - (layout.compact ? 4 : 5),
                    bowY + (layout.compact ? 2 : 3),
                    layout.compassCx + (layout.compact ? 4 : 5),
                    bowY + (layout.compact ? 2 : 3),
                    COLOR_ACCENT);
  gfx->drawLine(layout.compassCx,
                layout.compassCy - layout.compassR + 3,
                layout.compassCx,
                layout.compassCy - layout.compassR + 10,
                COLOR_ACCENT);
}

static void draw_rotating_compass_card(float headingDeg,
                                       const Layout &layout,
                                       uint16_t tickColor,
                                       uint16_t labelColor) {
  int tickStep = layout.compact ? 15 : 10;
  int majorStep = layout.compact ? 45 : 30;
  for (int worldDeg = 0; worldDeg < 360; worldDeg += tickStep) {
    float screenDeg = worldDeg - headingDeg;
    float rad = (screenDeg - 90.0f) * DEG_TO_RAD;
    int outer = (worldDeg % majorStep == 0) ? layout.compassR + 1 : layout.compassR - 3;
    int inner = (worldDeg % majorStep == 0) ? layout.compassR - 9 : layout.compassR - 6;
    int x1 = layout.compassCx + static_cast<int>(outer * cos(rad));
    int y1 = layout.compassCy + static_cast<int>(outer * sin(rad));
    int x2 = layout.compassCx + static_cast<int>(inner * cos(rad));
    int y2 = layout.compassCy + static_cast<int>(inner * sin(rad));
    gfx->drawLine(x1, y1, x2, y2, tickColor);
  }

  static const int kCardDeg[4] = {0, 90, 180, 270};
  static const char* kCardLabel[4] = {"N", "E", "S", "W"};
  int labelRadius = layout.compassR - (layout.compact ? 12 : 14);
  gfx->setTextSize(1);
  gfx->setTextColor(labelColor);
  for (int i = 0; i < 4; ++i) {
    float screenDeg = kCardDeg[i] - headingDeg;
    float rad = (screenDeg - 90.0f) * DEG_TO_RAD;
    int lx = layout.compassCx + static_cast<int>(labelRadius * cos(rad)) - 3;
    int ly = layout.compassCy + static_cast<int>(labelRadius * sin(rad)) - 3;
    gfx->setCursor(lx, ly);
    gfx->print(kCardLabel[i]);
  }
}

static void draw_compass_dynamic(float heading,
                                 float anchorBearing,
                                 bool headingValid,
                                 const Layout &layout,
                                 uint16_t tickColor,
                                 uint16_t labelColor,
                                 uint16_t anchorColor) {
  int dynR = compass_dynamic_radius(layout);
  float cardHeading = headingValid ? heading : 0.0f;
  draw_rotating_compass_card(cardHeading, layout, tickColor, labelColor);
  if (headingValid) {
    // Course-up logic: bow is fixed at top (0 deg), arrow shows anchor direction relative to bow.
    float rel = anchorBearing - heading;
    while (rel < 0.0f) rel += 360.0f;
    while (rel >= 360.0f) rel -= 360.0f;
    draw_arrow(rel, layout.compassCx, layout.compassCy, dynR, anchorColor);
  }

  int markerR = layout.compact ? 3 : 4;
  gfx->fillCircle(layout.compassCx, layout.compassCy, markerR, COLOR_TEXT_DARK);
}
} // namespace

void display_draw_ui(bool gpsFix,
                     int satellites,
                     bool gpsFromPhone,
                     float speedKmh,
                     float heading,
                     bool headingValid,
                     bool headingFromPhone,
                     float anchorBearing,
                     float distanceMeters,
                     float errorDeg,
                     const char* mode,
                     int batteryPercent,
                     bool force) {
  static bool uiInitialized = false;
  static int lastScreenW = -1;
  static int lastScreenH = -1;
  static int lastHdgDeg = -1000;
  static int lastBrgDeg = -1000;
  static float lastCompassHeading = -1000.0f;
  static float lastCompassBearing = -1000.0f;
  static int lastSatCount = -1;
  static int lastBattery = -1;
  static bool lastGpsFix = false;
  static bool lastGpsFromPhone = false;
  static float lastSpeed = -1000.0f;
  static float lastDist = -1000.0f;
  static float lastErr = -1000.0f;
  static bool lastHeadingValid = false;
  static bool lastHeadingFromPhone = false;
  static String lastMode;

  const char *modeLabel = mode ? mode : "IDLE";
  int satCount = max(0, satellites);
  int battery = constrain(batteryPercent, 0, 100);
  float speed = sanitize_non_negative(speedKmh);
  float hdg = sanitize_signed(heading);
  float brg = sanitize_signed(anchorBearing);
  float dist = sanitize_non_negative(distanceMeters);
  float err = fabsf(sanitize_signed(errorDeg));
  int hdgDeg = normalize_deg(hdg);
  int brgDeg = normalize_deg(brg);

  Layout layout = display_layout();
  int card1Y = layout.rightY;
  int card2Y = card1Y + layout.cardH + layout.gap;
  int card3Y = card2Y + layout.cardH + layout.gap;

  bool fullInit = force || !uiInitialized ||
                  layout.screenW != lastScreenW ||
                  layout.screenH != lastScreenH;
  bool dirty = false;

  if (fullInit) {
    gfx->fillScreen(COLOR_BG_LIGHT);
    draw_panel(layout.headerX, layout.headerY, layout.headerW, layout.headerH, layout.panelRadius, COLOR_PANEL);
    draw_panel(layout.rightX, card1Y, layout.rightW, layout.cardH, layout.panelRadius, COLOR_PANEL);
    draw_panel(layout.rightX, card2Y, layout.rightW, layout.cardH, layout.panelRadius, COLOR_PANEL);
    draw_panel(layout.rightX, card3Y, layout.rightW, layout.cardH, layout.panelRadius, COLOR_PANEL);
    draw_panel(layout.footerX, layout.footerY, layout.footerW, layout.footerH, layout.panelRadius, COLOR_PANEL);

    uiInitialized = true;
    lastScreenW = layout.screenW;
    lastScreenH = layout.screenH;
    lastHdgDeg = -1000;
    lastBrgDeg = -1000;
    lastCompassHeading = -1000.0f;
    lastCompassBearing = -1000.0f;
    lastSatCount = -1;
    lastBattery = -1;
    lastGpsFix = !gpsFix;
    lastGpsFromPhone = !gpsFromPhone;
    lastSpeed = -1000.0f;
    lastDist = -1000.0f;
    lastErr = -1000.0f;
    lastHeadingValid = !headingValid;
    lastHeadingFromPhone = !headingFromPhone;
    lastMode = "";
    dirty = true;
  }

  bool modeChanged = fullInit || !lastMode.equals(modeLabel);
  bool headingChanged = fullInit || lastHdgDeg != hdgDeg;
  bool bearingChanged = fullInit || lastBrgDeg != brgDeg || modeChanged;
  int hdgX = layout.headerX + (layout.compact ? 30 : 38);
  int hdgY = layout.headerY + (layout.compact ? 6 : 8);
  int hdgW = layout.compact ? 34 : 56;
  int hdgH = layout.compact ? 8 : 16;

  if (fullInit) {
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(1);
    gfx->setCursor(layout.headerX + 6, layout.headerY + (layout.compact ? 6 : 9));
    gfx->print("HDG");
  }

  if (headingChanged) {
    gfx->fillRect(hdgX, hdgY, hdgW, hdgH, COLOR_PANEL);
    gfx->setTextColor(COLOR_TEXT_DARK);
    gfx->setTextSize(layout.compact ? 1 : 2);
    gfx->setCursor(hdgX, hdgY);
    gfx->printf("%03d%c", hdgDeg, 176);
    dirty = true;
  }

  bool headingSourceChanged = fullInit ||
                              headingValid != lastHeadingValid ||
                              headingFromPhone != lastHeadingFromPhone;
  if (headingSourceChanged) {
    int srcX = hdgX + hdgW + 2;
    int srcY = layout.headerY + (layout.compact ? 6 : 9);
    int srcW = layout.compact ? 18 : 24;
    int srcH = layout.compact ? 8 : 10;
    gfx->fillRect(srcX, srcY, srcW, srcH, COLOR_PANEL);
    gfx->setTextColor(headingValid ? COLOR_TEXT_DARK : COLOR_WARNING);
    gfx->setTextSize(1);
    gfx->setCursor(srcX, srcY);
    if (!headingValid) {
      gfx->print("--");
    } else {
      gfx->print(headingFromPhone ? "PH" : "HW");
    }
    dirty = true;
  }

  if (bearingChanged) {
    int rightZoneX = layout.headerX + (layout.compact ? 84 : 118);
    int rightZoneW = layout.headerW - (rightZoneX - layout.headerX) - 4;
    if (rightZoneW > 4) {
      gfx->fillRect(rightZoneX, layout.headerY + 2, rightZoneW, layout.headerH - 4, COLOR_PANEL);
    }

    int modeLen = static_cast<int>(strlen(modeLabel));
    int badgePadX = layout.compact ? 6 : 8;
    int minBadgeW = layout.compact ? 40 : 64;
    int reserveLeft = layout.compact ? 124 : 120;
    int maxBadgeW = max(minBadgeW, layout.headerW - reserveLeft);
    int badgeW = constrain(modeLen * 6 + badgePadX * 2, minBadgeW, maxBadgeW);
    int badgeH = layout.compact ? 12 : 18;
    int badgeX = layout.headerX + layout.headerW - badgeW - 8;
    int badgeY = layout.headerY + (layout.headerH - badgeH) / 2;
    uint16_t badgeColor = mode_color(modeLabel);
    gfx->fillRoundRect(badgeX, badgeY, badgeW, badgeH, 6, badgeColor);
    gfx->setTextColor((badgeColor == COLOR_SILVER) ? COLOR_TEXT_DARK : COLOR_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(badgeX + badgePadX, badgeY + (layout.compact ? 3 : 6));
    gfx->print(modeLabel);

    int brgTextX = layout.headerX + (layout.compact ? 88 : 124);
    int brgWidth = layout.compact ? 34 : 64;
    if (brgTextX + brgWidth < badgeX) {
      gfx->setTextColor(COLOR_TEXT_DARK);
      gfx->setTextSize(1);
      gfx->setCursor(brgTextX, layout.headerY + (layout.compact ? 6 : 10));
      if (layout.compact) {
        gfx->printf("B%03d", brgDeg);
      } else {
        gfx->printf("BRG %03d%c", brgDeg, 176);
      }
    }
    dirty = true;
  }

  bool compassChanged = fullInit ||
                        lastHdgDeg != hdgDeg ||
                        lastBrgDeg != brgDeg ||
                        headingValid != lastHeadingValid;
  if (compassChanged) {
    if (fullInit) {
      draw_compass_static(layout);
    } else if (lastCompassHeading > -900.0f && lastCompassBearing > -900.0f) {
      // Erase only previous dynamic vectors to avoid full-area blink.
      draw_compass_dynamic(lastCompassHeading,
                           lastCompassBearing,
                           lastHeadingValid,
                           layout,
                           COLOR_PANEL,
                           COLOR_PANEL,
                           COLOR_PANEL);
    }

    draw_compass_dynamic(hdg,
                         brg,
                         headingValid,
                         layout,
                         COLOR_SILVER,
                         COLOR_TEXT_DARK,
                         COLOR_WARNING);
    lastCompassHeading = hdg;
    lastCompassBearing = brg;
    dirty = true;
  }

  bool speedChanged = fullInit || fabsf(speed - lastSpeed) > 0.05f;
  if (speedChanged) {
    if (fullInit) {
      gfx->setTextColor(COLOR_TEXT_DARK);
      gfx->setTextSize(1);
      gfx->setCursor(layout.rightX + 6, card1Y + (layout.compact ? 4 : 8));
      gfx->print("SPD");
    }
    if (layout.compact) {
      gfx->fillRect(layout.rightX + 6, card1Y + 11, layout.rightW - 12, 10, COLOR_PANEL);
      gfx->setTextColor(COLOR_TEXT_DARK);
      gfx->setTextSize(1);
      gfx->setCursor(layout.rightX + 6, card1Y + 15);
      gfx->printf("%.1f", speed);
      gfx->setCursor(layout.rightX + layout.rightW - 12, card1Y + 15);
      gfx->print("k");
    } else {
      gfx->fillRect(layout.rightX + 8, card1Y + 18, layout.rightW - 14, 18, COLOR_PANEL);
      gfx->setTextColor(COLOR_TEXT_DARK);
      gfx->setTextSize(2);
      gfx->setCursor(layout.rightX + 8, card1Y + 22);
      gfx->printf("%.1f", speed);
      gfx->setTextSize(1);
      gfx->setCursor(layout.rightX + layout.rightW - 30, card1Y + 26);
      gfx->print("km/h");
    }
    dirty = true;
  }

  bool gpsChanged = fullInit ||
                    satCount != lastSatCount ||
                    gpsFix != lastGpsFix ||
                    gpsFromPhone != lastGpsFromPhone;
  if (gpsChanged) {
    if (fullInit) {
      gfx->setTextColor(COLOR_TEXT_DARK);
      gfx->setTextSize(1);
      gfx->setCursor(layout.rightX + 6, card2Y + (layout.compact ? 4 : 8));
      gfx->print("GPS");
    }
    if (layout.compact) {
      gfx->fillRect(layout.rightX + 6, card2Y + 11, layout.rightW - 12, 10, COLOR_PANEL);
      gfx->setTextColor(gpsFix ? COLOR_TEXT_DARK : COLOR_WARNING);
      gfx->setTextSize(1);
      gfx->setCursor(layout.rightX + 6, card2Y + 15);
      gfx->printf("%d", satCount);
      gfx->setCursor(layout.rightX + 20, card2Y + 15);
      gfx->print(gpsFix ? "FIX" : "NO");
      if (gpsFix) {
        gfx->setCursor(layout.rightX + layout.rightW - 18, card2Y + 15);
        gfx->print(gpsFromPhone ? "PH" : "HW");
      }
    } else {
      gfx->fillRect(layout.rightX + 8, card2Y + 18, layout.rightW - 14, 18, COLOR_PANEL);
      gfx->setTextColor(gpsFix ? COLOR_TEXT_DARK : COLOR_WARNING);
      gfx->setTextSize(2);
      gfx->setCursor(layout.rightX + 8, card2Y + 22);
      gfx->printf("%d", satCount);
      gfx->setTextSize(1);
      gfx->setCursor(layout.rightX + 44, card2Y + 26);
      if (gpsFix) {
        gfx->print(gpsFromPhone ? "FIX PH" : "FIX HW");
      } else {
        gfx->print("NO FIX");
      }
    }
    dirty = true;
  }

  bool card3Changed = fullInit;
  if (layout.compact) {
    card3Changed = card3Changed || fabsf(dist - lastDist) > 0.05f;
  } else {
    card3Changed = card3Changed || battery != lastBattery;
  }
  if (card3Changed) {
    if (layout.compact) {
      if (fullInit) {
        gfx->setTextColor(COLOR_TEXT_DARK);
        gfx->setTextSize(1);
        gfx->setCursor(layout.rightX + 6, card3Y + 4);
        gfx->print("DIST");
      }
      gfx->fillRect(layout.rightX + 6, card3Y + 11, layout.rightW - 12, 10, COLOR_PANEL);
      gfx->setTextColor(COLOR_TEXT_DARK);
      gfx->setTextSize(1);
      gfx->setCursor(layout.rightX + 6, card3Y + 4);
      gfx->setCursor(layout.rightX + 6, card3Y + 15);
      gfx->printf("%.1fm", dist);
    } else {
      int bodyX = layout.rightX + 8;
      int bodyY = card3Y + 22;
      int bodyW = layout.rightW - 28;
      if (bodyW < 32) {
        bodyW = 32;
      }
      if (fullInit) {
        gfx->setTextColor(COLOR_TEXT_DARK);
        gfx->setTextSize(1);
        gfx->setCursor(layout.rightX + 8, card3Y + 8);
        gfx->print("PWR");
        gfx->drawRect(bodyX, bodyY, bodyW, 12, COLOR_TEXT_DARK);
        gfx->fillRect(bodyX + bodyW, bodyY + 3, 3, 6, COLOR_TEXT_DARK);
      }
      gfx->fillRect(bodyX + 1, bodyY + 1, bodyW - 2, 10, COLOR_PANEL);
      int fillW = ((bodyW - 4) * battery) / 100;
      uint16_t batteryColor = (battery >= 25) ? COLOR_ACCENT : COLOR_WARNING;
      gfx->fillRect(bodyX + 2, bodyY + 2, fillW, 8, batteryColor);
      gfx->fillRect(layout.rightX + 8, card3Y + layout.cardH - 12, 44, 10, COLOR_PANEL);
      gfx->setTextColor(COLOR_TEXT_DARK);
      gfx->setTextSize(1);
      gfx->setCursor(layout.rightX + 8, card3Y + layout.cardH - 10);
      gfx->printf("%d%%", battery);
    }
    dirty = true;
  }

  bool footerChanged = fullInit ||
                       fabsf(dist - lastDist) > 0.05f ||
                       fabsf(err - lastErr) > 0.2f ||
                       brgDeg != lastBrgDeg ||
                       gpsFix != lastGpsFix;
  if (footerChanged) {
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_TEXT_DARK);
    int col1 = layout.footerX + (layout.compact ? 4 : 8);
    int col2 = layout.footerX + layout.footerW / 3;
    int col3 = layout.footerX + (layout.footerW * 2) / 3;
    int footerY = layout.footerY + (layout.compact ? 5 : 10);
    int footerValueH = layout.compact ? 9 : 11;
    int footerValueY = layout.footerY + (layout.compact ? 3 : 8);

    if (layout.compact) {
      gfx->fillRect(col1, footerValueY, 52, footerValueH, COLOR_PANEL);
      gfx->fillRect(col2, footerValueY, 52, footerValueH, COLOR_PANEL);
      gfx->fillRect(col3, footerValueY, 72, footerValueH, COLOR_PANEL);
      gfx->setCursor(col1, footerY);
      gfx->printf("D%.1f", dist);
      gfx->setTextColor(err > 20.0f ? COLOR_WARNING : COLOR_TEXT_DARK);
      gfx->setCursor(col2, footerY);
      gfx->printf("E%.0f", err);
      gfx->setTextColor(COLOR_TEXT_DARK);
      gfx->setCursor(col3, footerY);
      if (gpsFix) {
        gfx->printf("B%03d", brgDeg);
      } else {
        gfx->print("NO GPS");
      }
    } else {
      gfx->fillRect(col1, footerValueY, 76, footerValueH, COLOR_PANEL);
      gfx->fillRect(col2, footerValueY, 74, footerValueH, COLOR_PANEL);
      gfx->fillRect(col3, footerValueY, 84, footerValueH, COLOR_PANEL);
      gfx->setCursor(col1, footerY);
      gfx->printf("DIST %.1fm", dist);

      gfx->setTextColor(err > 20.0f ? COLOR_WARNING : COLOR_TEXT_DARK);
      gfx->setCursor(col2, footerY);
      gfx->printf("ERR %.0f%c", err, 176);

      gfx->setTextColor(COLOR_TEXT_DARK);
      gfx->setCursor(col3, footerY);
      gfx->printf("BRG %03d%c %s", brgDeg, 176, gpsFix ? "OK" : "NO");
    }
    dirty = true;
  }

  lastHdgDeg = hdgDeg;
  lastBrgDeg = brgDeg;
  lastSatCount = satCount;
  lastBattery = battery;
  lastGpsFix = gpsFix;
  lastGpsFromPhone = gpsFromPhone;
  lastSpeed = speed;
  lastDist = dist;
  lastErr = err;
  lastHeadingValid = headingValid;
  lastHeadingFromPhone = headingFromPhone;
  lastMode = modeLabel;

  if (dirty) {
    display_flush();
  }
}
