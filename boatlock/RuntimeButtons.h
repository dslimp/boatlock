#pragma once

#include <Arduino.h>

#include "AnchorReasons.h"
#include "HoldButtonController.h"

enum class RuntimeButtonActionType : uint8_t {
  NONE = 0,
  SAVE_ANCHOR_POINT,
  DENY_ANCHOR_POINT,
  EMERGENCY_STOP,
  OPEN_PAIRING_WINDOW,
};

struct RuntimeButtonAction {
  RuntimeButtonActionType type = RuntimeButtonActionType::NONE;
  AnchorDeniedReason deniedReason = AnchorDeniedReason::NONE;
};

class RuntimeButtons {
public:
  RuntimeButtonAction updateBoot(bool pressed,
                                 unsigned long nowMs,
                                 unsigned long holdMs,
                                 bool controlGpsAvailable,
                                 AnchorDeniedReason deniedReason) {
    const HoldButtonController::Event event =
        bootButton_.update(pressed, nowMs, holdMs);
    if (!event.holdFired) {
      return {};
    }

    RuntimeButtonAction action;
    if (controlGpsAvailable) {
      action.type = RuntimeButtonActionType::SAVE_ANCHOR_POINT;
      return action;
    }

    action.type = RuntimeButtonActionType::DENY_ANCHOR_POINT;
    action.deniedReason = deniedReason;
    return action;
  }

  RuntimeButtonAction updateStop(bool pressed,
                                 unsigned long nowMs,
                                 unsigned long holdMs) {
    const HoldButtonController::Event event =
        stopButton_.update(pressed, nowMs, holdMs);
    if (event.pressedEdge) {
      RuntimeButtonAction action;
      action.type = RuntimeButtonActionType::EMERGENCY_STOP;
      return action;
    }
    if (event.holdFired) {
      RuntimeButtonAction action;
      action.type = RuntimeButtonActionType::OPEN_PAIRING_WINDOW;
      return action;
    }
    return {};
  }

  void reset() {
    bootButton_.reset();
    stopButton_.reset();
  }

private:
  HoldButtonController bootButton_;
  HoldButtonController stopButton_;
};
