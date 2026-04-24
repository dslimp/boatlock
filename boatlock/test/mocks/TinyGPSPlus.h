#pragma once

#include <cmath>

class TinyGPSPlus {
public:
  class Location {
  public:
    double lat() const { return 0; }
    double lng() const { return 0; }
  } location;

  static double distanceBetween(double lat1, double lon1, double lat2, double lon2) {
    constexpr double kEarthRadiusM = 6371000.0;
    const double dLat = radians(lat2 - lat1);
    const double dLon = radians(lon2 - lon1);
    const double a =
        std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
        std::cos(radians(lat1)) * std::cos(radians(lat2)) *
            std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return kEarthRadiusM * c;
  }

  static double courseTo(double lat1, double lon1, double lat2, double lon2) {
    const double y = std::sin(radians(lon2 - lon1)) * std::cos(radians(lat2));
    const double x =
        std::cos(radians(lat1)) * std::sin(radians(lat2)) -
        std::sin(radians(lat1)) * std::cos(radians(lat2)) *
            std::cos(radians(lon2 - lon1));
    double bearing = degrees(std::atan2(y, x));
    while (bearing < 0.0) bearing += 360.0;
    while (bearing >= 360.0) bearing -= 360.0;
    return bearing;
  }

private:
  static double radians(double deg) {
    return deg * 3.14159265358979323846 / 180.0;
  }

  static double degrees(double rad) {
    return rad * 180.0 / 3.14159265358979323846;
  }
};
