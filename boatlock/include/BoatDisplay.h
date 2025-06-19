// BoatDisplay.h
#pragma once
#include <Adafruit_SSD1306.h>
#include <TinyGPSPlus.h>

class BoatDisplay {
public:
    BoatDisplay(Adafruit_SSD1306* display)
        : _display(display) {}

    void showStatus(TinyGPSPlus& gps,
                   float anchorLat, float anchorLon,
                   int AnchorEnabled, float dist, float bearing,
                   float heading, bool holding,
                   float encoderAngle = 0)
    {
        _display->clearDisplay();
        _display->setTextSize(1);
        _display->setTextColor(SSD1306_WHITE);
        _display->setCursor(0, 0);
        _display->printf("L:%.5f\nN:%.5f\n", gps.location.lat(), gps.location.lng());
        _display->printf("Sat:%d", gps.satellites.value());
        _display->print(" HDOP: ");
        _display->print(gps.hdop.value() * 0.01);

        _display->setCursor(50, 30);

        _display->printf("V:%.1f km/h", gps.speed.kmph());

        _display->setCursor(0, 30);
        if (AnchorEnabled==1) {
            _display->printf("Dst:%.1fm\n", dist);
            _display->printf("Brg:%.0f%c\n", bearing, 176);
            drawArrowAt(bearing, 44, 54);          // direction to anchor (GPS)

            float relative = bearing - heading;    // anchor relative to heading
            if (relative < 0) relative += 360.0f;
            drawArrowAt(relative, 64, 54);

            float northAngle = 360.0f - heading;
            if (northAngle < 0) northAngle += 360.0f;
            drawArrowAt(northAngle, 84, 54); // north arrow
        } else {
            _display->println("Press BOOT to anchor");
        }
        _display->display();
    }

    void showDebug(const String &msg, int y = 48) {
        _display->clearDisplay();
        _display->setTextSize(1);
        _display->setTextColor(SSD1306_WHITE);
        _display->setCursor(0, y);
        _display->print("[D]");
        _display->print(msg);
        _display->display();
    }

    void drawArrowAt(float bearing, int cx, int cy) {
        constexpr int R  = 10;
        float rad = (bearing - 90) * DEG_TO_RAD;
        int x2 = cx + R * cos(rad);
        int y2 = cy + R * sin(rad);
        _display->drawLine(cx, cy, x2, y2, SSD1306_WHITE);
        int x_head1 = cx + (R - 3) * cos(rad + 0.3);
        int y_head1 = cy + (R - 3) * sin(rad + 0.3);
        int x_head2 = cx + (R - 3) * cos(rad - 0.3);
        int y_head2 = cy + (R - 3) * sin(rad - 0.3);
        _display->drawLine(x2, y2, x_head1, y_head1, SSD1306_WHITE);
        _display->drawLine(x2, y2, x_head2, y_head2, SSD1306_WHITE);
    }

    void drawArrow(float bearing) {
        drawArrowAt(bearing, 64, 54);
    }

private:
    Adafruit_SSD1306* _display;
};
