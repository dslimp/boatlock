#pragma once
#include <cstdlib>
#include <cstdint>

class AccelStepper {
public:
  static const uint8_t HALF4WIRE = 8;

  AccelStepper(uint8_t, int, int, int, int) {}

  void setMaxSpeed(float v) { maxSpeed_ = v; }
  void setAcceleration(float v) { accel_ = v; }
  void setSpeed(float v) { speed_ = v; }

  long currentPosition() const { return currentPos_; }
  long distanceToGo() const { return targetPos_ - currentPos_; }

  void moveTo(long absolute) { targetPos_ = absolute; }

  void run() {
    if (currentPos_ == targetPos_) {
      return;
    }
    currentPos_ += (targetPos_ > currentPos_) ? 1 : -1;
  }

  void runSpeed() {
    if (speed_ > 0.0f) {
      currentPos_ += 1;
    } else if (speed_ < 0.0f) {
      currentPos_ -= 1;
    }
  }

  void disableOutputs() { outputsEnabled_ = false; }
  void enableOutputs() { outputsEnabled_ = true; }

  bool outputsEnabled() const { return outputsEnabled_; }
  float maxSpeed() const { return maxSpeed_; }
  float accel() const { return accel_; }

private:
  long currentPos_ = 0;
  long targetPos_ = 0;
  float speed_ = 0.0f;
  float maxSpeed_ = 0.0f;
  float accel_ = 0.0f;
  bool outputsEnabled_ = true;
};
