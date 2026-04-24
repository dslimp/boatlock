#pragma once

#include <Arduino.h>

class HoldButtonController {
public:
  struct Event {
    bool pressedEdge = false;
    bool holdFired = false;
    bool releasedEdge = false;
  };

  Event update(bool rawPressed,
               unsigned long nowMs,
               unsigned long holdMs,
               unsigned long debounceMs = 40) {
    Event event;

    if (rawPressed != rawPressed_) {
      rawPressed_ = rawPressed;
      rawChangedMs_ = nowMs;
    }

    if (rawPressed_ != pressed_ && nowMs - rawChangedMs_ >= debounceMs) {
      pressed_ = rawPressed_;
      if (pressed_) {
        downSinceMs_ = nowMs;
        holdFired_ = false;
        event.pressedEdge = true;
      } else {
        downSinceMs_ = 0;
        holdFired_ = false;
        event.releasedEdge = true;
      }
    }

    if (!pressed_) {
      return event;
    }

    if (!holdFired_ && nowMs - downSinceMs_ >= holdMs) {
      holdFired_ = true;
      event.holdFired = true;
    }
    return event;
  }

  void reset() {
    rawPressed_ = false;
    rawChangedMs_ = 0;
    pressed_ = false;
    downSinceMs_ = 0;
    holdFired_ = false;
  }

private:
  bool rawPressed_ = false;
  unsigned long rawChangedMs_ = 0;
  bool pressed_ = false;
  unsigned long downSinceMs_ = 0;
  bool holdFired_ = false;
};
