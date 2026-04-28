#pragma once

#include <Arduino.h>

#include "AnchorReasons.h"
#include "HoldButtonController.h"

enum class RuntimeButtonActionType : uint8_t {
  NONE = 0,
  SAVE_ANCHOR_POINT,
  DENY_ANCHOR_POINT,
  EMERGENCY_STOP,
};

struct RuntimeButtonAction {
  RuntimeButtonActionType type = RuntimeButtonActionType::NONE;
  AnchorDeniedReason deniedReason = AnchorDeniedReason::NONE;
};

class RuntimeButtons {
public:
  static constexpr unsigned long kDebounceMs = 40;

  RuntimeButtonAction updateBoot(bool pressed,
                                 unsigned long nowMs,
                                 unsigned long holdMs,
                                 bool controlGpsAvailable,
                                 AnchorDeniedReason deniedReason) {
    const HoldButtonController::Event event =
        bootButton_.update(pressed, nowMs, holdMs, kDebounceMs);
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

  RuntimeButtonAction updateStop(bool pressed, unsigned long nowMs) {
    const HoldButtonController::Event event =
        stopButton_.update(pressed, nowMs, 0, kDebounceMs);
    if (event.pressedEdge) {
      RuntimeButtonAction action;
      action.type = RuntimeButtonActionType::EMERGENCY_STOP;
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
