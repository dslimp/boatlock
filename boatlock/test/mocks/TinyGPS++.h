#pragma once
class TinyGPSPlus {
public:
  class Location {
  public:
    double lat() const { return 0; }
    double lng() const { return 0; }
  } location;
  static double distanceBetween(double, double, double, double) { return 0; }
  static double courseTo(double, double, double, double) { return 0; }
};
