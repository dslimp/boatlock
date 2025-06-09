
#pragma once
#include <math.h>  // для cos/sin



void drawArrow(float bearing) {
  float rad = (bearing - 90) * DEG_TO_RAD; // -90 чтобы 0° был вверх
  int x2 = CX + R * cos(rad);
  int y2 = CY + R * sin(rad);
  display.drawLine(CX, CY, x2, y2, SSD1306_WHITE);
  int x_head1 = CX + (R - 3) * cos(rad + 0.3);
  int y_head1 = CY + (R - 3) * sin(rad + 0.3);
  int x_head2 = CX + (R - 3) * cos(rad - 0.3);
  int y_head2 = CY + (R - 3) * sin(rad - 0.3);
  display.drawLine(x2, y2, x_head1, y_head1, SSD1306_WHITE);
  display.drawLine(x2, y2, x_head2, y_head2, SSD1306_WHITE);
}
