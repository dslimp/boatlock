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
                   bool anchorSet, float dist, float bearing,
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
        if (anchorSet) {
            _display->printf("Dst:%.1fm\n", dist);
            _display->printf("Brg:%.0f%c\n", bearing, 176);
            drawArrow(bearing);
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

    void drawArrow(float bearing) {
        constexpr int CX = 64;
        constexpr int CY = 54; 
        constexpr int R  = 10; 
        float rad = (bearing - 90) * DEG_TO_RAD; 
        int x2 = CX + R * cos(rad);
        int y2 = CY + R * sin(rad);
        _display->drawLine(CX, CY, x2, y2, SSD1306_WHITE);
        int x_head1 = CX + (R - 3) * cos(rad + 0.3);
        int y_head1 = CY + (R - 3) * sin(rad + 0.3);
        int x_head2 = CX + (R - 3) * cos(rad - 0.3);
        int y_head2 = CY + (R - 3) * sin(rad - 0.3);
        _display->drawLine(x2, y2, x_head1, y_head1, SSD1306_WHITE);
        _display->drawLine(x2, y2, x_head2, y_head2, SSD1306_WHITE);
    }

private:
    Adafruit_SSD1306* _display;
};
