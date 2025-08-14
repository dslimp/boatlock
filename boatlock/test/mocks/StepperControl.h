#pragma once
class StepperControl {
public:
  bool cancelCalled = false;
  bool stopManualCalled = false;
  bool loadCalled = false;
  void cancelMove() { cancelCalled = true; }
  void stopManual() { stopManualCalled = true; }
  void loadFromSettings() { loadCalled = true; }
};
