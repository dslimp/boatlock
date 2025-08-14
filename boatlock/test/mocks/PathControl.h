#pragma once
class PathControl {
public:
  int numPoints = 0;
  void reset() { numPoints = 0; }
  void addPoint(float, float) { numPoints++; }
  void start() {}
  void stop() {}
};
