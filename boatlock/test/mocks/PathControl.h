#pragma once
class PathControl {
public:
  int numPoints = 0;
  bool startCalled = false;
  bool stopCalled = false;
  void reset() { numPoints = 0; }
  void addPoint(float, float) { numPoints++; }
  void start() { startCalled = true; }
  void stop() { stopCalled = true; }
};
