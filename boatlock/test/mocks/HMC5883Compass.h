#pragma once
class HMC5883Compass {
public:
  float az = 0;
  float getAzimuth() { return az; }
};
