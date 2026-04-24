#pragma once

#include <Arduino.h>
#include <math.h>

#include <string>

#include "AnchorReasons.h"
#include "RuntimeControl.h"
#include "AnchorSupervisor.h"
#include "DriftMonitor.h"
#include "Logger.h"
#include "ManualControl.h"
#include "MotorControl.h"
#include "Settings.h"
#include "StepperControl.h"

class RuntimeMotion {
public:
  static constexpr unsigned long kStepperCheckIntervalMs = 250;
  static constexpr float kStepperDeadbandBaseDeg = 2.2f;
  static constexpr float kStepperDeadbandMaxDeg = 9.0f;
  static constexpr float kStepperDeadbandHystDeg = 0.9f;
  static constexpr float kMotorHeadingHystDeg = 2.0f;
  static constexpr unsigned long kSafetyReasonHoldMs = 15000;

  RuntimeMotion(Settings& settings,
                StepperControl& stepperControl,
                MotorControl& motor,
                DriftMonitor& driftMonitor)
      : settings_(settings),
        stepperControl_(stepperControl),
        motor_(motor),
        driftMonitor_(driftMonitor) {}

  void setSafetyReason(const char* reason, unsigned long nowMs) {
    safetyReason_ = reason ? reason : "";
    safetyReasonMs_ = nowMs;
  }

  void enterSafeHold() {
    safeHoldActive_ = true;
  }

  void clearSafeHold() {
    safeHoldActive_ = false;
  }

  bool safeHoldActive() const { return safeHoldActive_; }

  std::string activeSafetyReason(unsigned long nowMs) const {
    if (safetyReason_.empty()) {
      return "";
    }
    if (nowMs - safetyReasonMs_ > kSafetyReasonHoldMs) {
      return "";
    }
    return safetyReason_;
  }

  void updateDriftState(unsigned long nowMs,
                        CoreMode mode,
                        bool gpsFix,
                        float distanceM) {
    driftMonitor_.update(nowMs,
                         coreModeUsesAnchorControl(mode),
                         gpsFix,
                         distanceM,
                         settings_.get("DriftAlert"),
                         settings_.get("DriftFail"));
    driftAlertActive_ = driftMonitor_.alertActive();
    driftFailActive_ = driftMonitor_.failActive();
  }

  float currentStepperDeadbandDeg(bool compassReady,
                                  float rvAccuracyDeg,
                                  int rvQuality) const {
    float deadband = kStepperDeadbandBaseDeg;
    if (compassReady) {
      if (isfinite(rvAccuracyDeg) && rvAccuracyDeg > 0.0f) {
        deadband = max(deadband, 1.2f + rvAccuracyDeg * 1.3f);
      }
      if (rvQuality <= 1) {
        deadband += 0.8f;
      }
      if (rvQuality == 0) {
        deadband += 1.0f;
      }
    }
    return constrain(deadband, kStepperDeadbandBaseDeg, kStepperDeadbandMaxDeg);
  }

  void applyControl(const RuntimeControlInput& input) {
    if (input.mode == CoreMode::MANUAL) {
      stepperTrackingActive_ = false;
      motorHeadingAligned_ = false;
      if (input.manualDir == 0) {
        stepperControl_.startManual(-1);
      } else if (input.manualDir == 1) {
        stepperControl_.startManual(1);
      } else {
        stepperControl_.stopManual();
      }
      motor_.driveManual(input.manualSpeed);
      return;
    }

    stepperControl_.stopManual();

    const bool autoControlActive = coreModeUsesAnchorControl(input.mode);
    if (autoControlActive && (!input.controlGpsAvailable || driftFailActive_)) {
      quietAutoOutputs();
      return;
    }

    bool motorCanRun = false;
    if (autoControlActive) {
      if (input.hasHeading && input.hasBearing) {
        const float absDiff = fabsf(input.diffDeg);
        const float deadband =
            currentStepperDeadbandDeg(input.compassReady, input.rvAccuracyDeg, input.rvQuality);
        const float engageBand = deadband + kStepperDeadbandHystDeg;

        if (stepperTrackingActive_) {
          if (absDiff <= deadband) {
            stepperTrackingActive_ = false;
          }
        } else if (absDiff >= engageBand) {
          stepperTrackingActive_ = true;
        }

        if (stepperTrackingActive_ &&
            input.nowMs - lastStepperCheckMs_ >= kStepperCheckIntervalMs) {
          stepperControl_.pointToBearing(input.bearingDeg, input.headingDeg);
          lastStepperCheckMs_ = input.nowMs;
        } else if (!stepperTrackingActive_) {
          stepperControl_.cancelMove();
        }

        const float motorTol = max(2.0f, settings_.get("AngTol"));
        const float motorReleaseTol = motorTol + kMotorHeadingHystDeg;
        if (motorHeadingAligned_) {
          if (absDiff > motorReleaseTol) {
            motorHeadingAligned_ = false;
          }
        } else if (absDiff <= motorTol) {
          motorHeadingAligned_ = true;
        }
        motorCanRun = motorHeadingAligned_;
      } else {
        quietAutoOutputs();
      }
    } else {
      quietAutoOutputs();
    }

    const float holdRadiusMeters = max(0.5f, settings_.get("HoldRadius"));
    const float deadbandMeters = constrain(settings_.get("DeadbandM"), 0.2f, 10.0f);
    const int maxThrustPctAnchor = constrain((int)settings_.get("MaxThrustA"), 10, 100);
    const float thrustRampPctPerS = constrain(settings_.get("ThrRampA"), 1.0f, 100.0f);
    const bool allowThrust = autoControlActive && motorCanRun && !driftFailActive_;
    motor_.driveAnchorAuto(input.distanceM,
                           holdRadiusMeters,
                           allowThrust,
                           deadbandMeters,
                           maxThrustPctAnchor,
                           thrustRampPctPerS);
  }

  void stopAllMotionNow(ManualControl& manualControl) {
    const bool wasAnchorOn = settings_.get("AnchorEnabled") == 1;
    settings_.set("AnchorEnabled", 0);
    settings_.save();
    enterSafeHold();
    manualControl.stop();
    quietAllOutputs();
    if (wasAnchorOn) {
      logMessage("[EVENT] ANCHOR_OFF reason=STOP\n");
    }
  }

  void disableAnchorAndStop(const char* reason, unsigned long nowMs) {
    settings_.set("AnchorEnabled", 0);
    settings_.save();
    enterSafeHold();
    quietAllOutputs();
    setSafetyReason(reason, nowMs);
    logMessage("[EVENT] ANCHOR_OFF reason=%s\n", reason ? reason : "UNKNOWN");
  }

  void applySupervisorDecision(const AnchorSupervisor::Decision& decision,
                               ManualControl& manualControl,
                               unsigned long nowMs) {
    if (decision.action == AnchorSupervisor::SafeAction::NONE) {
      return;
    }

    FailsafeReason failsafeReason = FailsafeReason::NONE;
    switch (decision.reason) {
      case AnchorSupervisor::Reason::GPS_WEAK:
        failsafeReason = FailsafeReason::GPS_WEAK;
        break;
      case AnchorSupervisor::Reason::CONTAINMENT_BREACH:
        failsafeReason = FailsafeReason::CONTAINMENT_BREACH;
        break;
      case AnchorSupervisor::Reason::COMM_TIMEOUT:
        failsafeReason = FailsafeReason::COMM_TIMEOUT;
        break;
      case AnchorSupervisor::Reason::CONTROL_LOOP_TIMEOUT:
        failsafeReason = FailsafeReason::CONTROL_LOOP_TIMEOUT;
        break;
      case AnchorSupervisor::Reason::SENSOR_TIMEOUT:
        failsafeReason = FailsafeReason::SENSOR_TIMEOUT;
        break;
      case AnchorSupervisor::Reason::INTERNAL_ERROR_NAN:
        failsafeReason = FailsafeReason::INTERNAL_ERROR_NAN;
        break;
      case AnchorSupervisor::Reason::COMMAND_OUT_OF_RANGE:
        failsafeReason = FailsafeReason::COMMAND_OUT_OF_RANGE;
        break;
      case AnchorSupervisor::Reason::NONE:
      default:
        failsafeReason = FailsafeReason::NONE;
        break;
    }

    const char* reason = failsafeReasonString(failsafeReason);
    setLastFailsafeReason(failsafeReason);
    if (decision.action == AnchorSupervisor::SafeAction::EXIT_ANCHOR) {
      manualControl.stop();
      disableAnchorAndStop(reason, nowMs);
      return;
    }

    settings_.set("AnchorEnabled", 0);
    settings_.save();

    stopAllMotionNow(manualControl);
    setSafetyReason(reason, nowMs);
    logMessage("[EVENT] FAILSAFE_TRIGGERED reason=%s action=STOP\n", reason);
  }

  void setLastAnchorDeniedReason(AnchorDeniedReason reason) {
    lastAnchorDeniedReason_ = reason;
  }

  void setLastFailsafeReason(FailsafeReason reason) {
    lastFailsafeReason_ = reason;
  }

  AnchorDeniedReason lastAnchorDeniedReason() const {
    return lastAnchorDeniedReason_;
  }

  FailsafeReason lastFailsafeReason() const {
    return lastFailsafeReason_;
  }

  const char* currentAnchorDeniedReason() const {
    return anchorDeniedReasonString(lastAnchorDeniedReason_);
  }

  const char* currentFailsafeReason() const {
    return failsafeReasonString(lastFailsafeReason_);
  }

  bool driftAlertActive() const { return driftAlertActive_; }
  bool driftFailActive() const { return driftFailActive_; }

private:
  Settings& settings_;
  StepperControl& stepperControl_;
  MotorControl& motor_;
  DriftMonitor& driftMonitor_;
  bool driftAlertActive_ = false;
  bool driftFailActive_ = false;
  std::string safetyReason_;
  unsigned long safetyReasonMs_ = 0;
  unsigned long lastStepperCheckMs_ = 0;
  bool stepperTrackingActive_ = false;
  bool motorHeadingAligned_ = false;
  bool safeHoldActive_ = false;
  AnchorDeniedReason lastAnchorDeniedReason_ = AnchorDeniedReason::NONE;
  FailsafeReason lastFailsafeReason_ = FailsafeReason::NONE;

  void quietAutoOutputs() {
    stepperTrackingActive_ = false;
    motorHeadingAligned_ = false;
    stepperControl_.cancelMove();
    motor_.stop();
  }

  void quietAllOutputs() {
    stepperControl_.stopManual();
    quietAutoOutputs();
  }
};
