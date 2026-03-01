#pragma once
class BNO08xCompass {
public:
  float az = 0;
  float headingOffset = 0;
  float getAzimuth() { return az; }
  void setHeadingOffsetDeg(float v) { headingOffset = v; }
  float getHeadingOffsetDeg() const { return headingOffset; }
};
