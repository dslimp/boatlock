#pragma once

#include <Arduino.h>

class HoldButtonController {
public:
  struct Event {
    bool pressedEdge = false;
    bool holdFired = false;
    bool releasedEdge = false;
  };

  Event update(bool pressed, unsigned long nowMs, unsigned long holdMs) {
    Event event;
    if (!pressed_) {
      if (pressed) {
        pressed_ = true;
        downSinceMs_ = nowMs;
        holdFired_ = false;
        event.pressedEdge = true;
      }
      return event;
    }

    if (!pressed) {
      pressed_ = false;
      downSinceMs_ = 0;
      holdFired_ = false;
      event.releasedEdge = true;
      return event;
    }

    if (!holdFired_ && nowMs >= downSinceMs_ && nowMs - downSinceMs_ >= holdMs) {
      holdFired_ = true;
      event.holdFired = true;
    }
    return event;
  }

  void reset() {
    pressed_ = false;
    downSinceMs_ = 0;
    holdFired_ = false;
  }

private:
  bool pressed_ = false;
  unsigned long downSinceMs_ = 0;
  bool holdFired_ = false;
};
