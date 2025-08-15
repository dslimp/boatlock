#pragma once
class MotorControl {
public:
  bool stopCalled = false;
  void stop() { stopCalled = true; }
};
