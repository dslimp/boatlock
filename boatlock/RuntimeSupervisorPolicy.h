#pragma once

#include <Arduino.h>

#include "AnchorSupervisor.h"
#include "Settings.h"

inline float runtimeSupervisorFiniteClamp(float value, float low, float high);

inline AnchorSupervisor::Config buildRuntimeSupervisorConfig(Settings& settings) {
  AnchorSupervisor::Config config;
  config.commTimeoutMs =
      static_cast<unsigned long>(runtimeSupervisorFiniteClamp(settings.get("CommToutMs"), 3000.0f, 60000.0f));
  config.controlLoopTimeoutMs =
      static_cast<unsigned long>(runtimeSupervisorFiniteClamp(settings.get("CtrlLoopMs"), 100.0f, 10000.0f));
  config.sensorTimeoutMs =
      static_cast<unsigned long>(runtimeSupervisorFiniteClamp(settings.get("SensorTout"), 300.0f, 30000.0f));
  config.gpsWeakGraceMs =
      static_cast<unsigned long>(runtimeSupervisorFiniteClamp(settings.get("GpsWeakHys"), 0.5f, 60.0f) * 1000.0f);
  config.maxCommandThrustPct = 100;
  return config;
}

inline float runtimeSupervisorFiniteClamp(float value, float low, float high) {
  if (!isfinite(value)) {
    return low;
  }
  return constrain(value, low, high);
}

inline AnchorSupervisor::Input buildRuntimeSupervisorInput(unsigned long nowMs,
                                                           bool anchorActive,
                                                           bool gpsQualityOk,
                                                           bool linkOk,
                                                           bool sensorsOk,
                                                           bool hasNaN,
                                                           bool containmentBreached,
                                                           unsigned long controlLoopDtMs,
                                                           int commandThrustPct) {
  AnchorSupervisor::Input input;
  input.nowMs = nowMs;
  input.anchorActive = anchorActive;
  input.gpsQualityOk = gpsQualityOk;
  input.linkOk = linkOk;
  input.sensorsOk = sensorsOk;
  input.hasNaN = hasNaN;
  input.containmentBreached = containmentBreached;
  input.controlLoopDtMs = controlLoopDtMs;
  input.commandThrustPct = commandThrustPct;
  return input;
}
