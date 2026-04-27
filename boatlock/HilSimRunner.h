#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "AnchorControlLoop.h"
#include "ControlInterfaces.h"
#include "HilSimActuator.h"
#include "HilSimClock.h"
#include "HilSimEvents.h"
#include "HilSimExpect.h"
#include "HilSimMetrics.h"
#include "HilSimRandom.h"
#include "HilSimReport.h"
#include "HilSimSensors.h"
#include "HilSimStatus.h"
#include "HilSimTime.h"
#include "HilSimWorld.h"

namespace hilsim {

struct TimedCurrent {
  unsigned long atMs = 0;
  unsigned long durationMs = 0;
  Vec2 currentMps;
  std::string eventCode;
};

enum class WaveWakeKind : uint8_t {
  WAKE = 0,
  CHOP = 1,
};

struct TimedWaveWakePacket {
  WaveWakeKind kind = WaveWakeKind::WAKE;
  unsigned long atMs = 0;
  unsigned long durationMs = 0;
  float heightM = 0.0f;
  float periodS = 1.0f;
  float directionDeg = 0.0f;
};

struct TimedLoopStall {
  unsigned long atMs = 0;
  unsigned long durationMs = 0;
  unsigned long dtMs = 500;
};

struct TimedPowerLoss {
  unsigned long atMs = 0;
  unsigned long durationMs = 0;
};

struct TimedDisplayLoss {
  unsigned long atMs = 0;
  unsigned long durationMs = 0;
};

struct TimedActuatorDerate {
  unsigned long atMs = 0;
  unsigned long durationMs = 0;
  float thrustScale = 1.0f;
};

struct EnvironmentConfig {
  Vec2 baseCurrentMps;
  std::vector<TimedCurrent> gusts;
  std::vector<TimedWaveWakePacket> waveWakePackets;
};

struct SimScenario {
  std::string id;
  unsigned long durationMs = 600000;
  unsigned long dtMs = 50;
  uint32_t seed = 1;
  float initialXM = 6.0f;
  float initialYM = -2.0f;
  float initialHeadingDeg = 20.0f;
  float anchorXM = 0.0f;
  float anchorYM = 0.0f;
  // Optional controller override for scenario-specific physical load cases.
  float maxThrustOverride = -1.0f;
  EnvironmentConfig env;
  SensorConfig sensors;
  ScenarioExpect expect;
  std::vector<TimedLoopStall> loopStalls;
  std::vector<TimedPowerLoss> powerLosses;
  std::vector<TimedDisplayLoss> displayLosses;
  std::vector<TimedActuatorDerate> actuatorDerates;
};

class HilScenarioRunner : public simctl::IControlEventSink {
public:
  enum class State : uint8_t {
    IDLE = 0,
    RUNNING = 1,
    DONE = 2,
    ABORTED = 3,
  };

  struct Result {
    bool pass = false;
    std::string reason;
    SimMetrics metrics;
    std::vector<SimEvent> events;
  };

  struct LiveTelemetry {
    bool valid = false;
    unsigned long simMs = 0;
    GnssFix gnss;
    HeadingSample heading;
    BoatWorldState world;
    ActuatorCmd command;
    bool anchorActive = false;
    bool gnssGood = false;
    float anchorXM = 0.0f;
    float anchorYM = 0.0f;
    float errTrueM = 0.0f;
    float bearingDeg = 0.0f;
    float speedMps = 0.0f;
  };

  HilScenarioRunner()
      : controller_(&sensors_, &sensors_, &actuator_, &clock_) {
    controller_.setEventSink(this);
  }

  void onControlEvent(unsigned long atMs,
                      const char* code,
                      const char* details) override {
    eventLog_.record(atMs, code, details);
  }

  bool start(const SimScenario& scenario) {
    if (state_ == State::RUNNING) {
      return false;
    }
    scenario_ = scenario;
    state_ = State::RUNNING;
    done_ = false;
    progressPct_ = 0.0f;
    clock_.set(0);
    world_.reset(scenario.initialXM, scenario.initialYM, scenario.initialHeadingDeg);
    world_.setConfig(BoatWorldConfig());
    sensors_.configure(scenario.sensors);
    sensors_.reset();
    rng_ = XorShift32(scenario.seed);
    actuator_.apply(ActuatorCmd());
    simctl::ControllerConfig cfg;
    if (scenario.maxThrustOverride > 0.0f) {
      cfg.maxThrust = clampf(scenario.maxThrustOverride, 0.1f, 1.0f);
    }
    controller_.setConfig(cfg);
    controller_.reset();
    controller_.requestAnchor(true);

    errorHist_.clear();
    eventLog_.clear();
    currentEventActive_.assign(scenario_.env.gusts.size(), false);
    waveWakePacketActive_.assign(scenario_.env.waveWakePackets.size(), false);
    compassLossActive_ = false;
    powerLossActive_ = false;
    displayLossActive_ = false;
    actuatorDerateActive_ = false;
    result_ = {};
    samples_ = 0;
    deadbandCount_ = 0;
    saturatedCount_ = 0;
    signChanges_ = 0;
    prevSteerSign_ = 0;
    badGnssAnchorSinceMs_ = 0;
    maxBadGnssAnchorMs_ = 0;
    simMs_ = 0;
    lastAppliedDtMs_ = scenario.dtMs;
    sensors_.update(0, world_.state(), &rng_);
    live_ = {};
    {
      const BoatWorldState& st = world_.state();
      const GnssFix fix = sensors_.getFix();
      const HeadingSample hdg = sensors_.getHeading();
      const Vec2 current = currentAtMs(0);
      live_.valid = true;
      live_.simMs = 0;
      live_.gnss = fix;
      live_.heading = hdg;
      live_.world = st;
      live_.command = actuator_.last();
      live_.anchorActive = false;
      live_.gnssGood = fix.valid;
      live_.anchorXM = scenario_.anchorXM;
      live_.anchorYM = scenario_.anchorYM;
      live_.errTrueM = hypotf(st.xM - scenario_.anchorXM, st.yM - scenario_.anchorYM);
      live_.bearingDeg = simctl::bearingDeg(st.xM, st.yM, scenario_.anchorXM, scenario_.anchorYM);
      live_.speedMps = hypotf(st.vxBody + current.x, st.vyBody + current.y);
    }
    return true;
  }

  bool isRunning() const { return state_ == State::RUNNING; }
  bool isDone() const { return done_; }
  State state() const { return state_; }
  float progressPct() const { return progressPct_; }
  const SimScenario& scenario() const { return scenario_; }
  const Result& result() const { return result_; }
  const LiveTelemetry& live() const { return live_; }

  void abort() {
    if (state_ != State::RUNNING) {
      return;
    }
    done_ = true;
    state_ = State::ABORTED;
    result_.pass = false;
    result_.reason = "ABORTED";
    result_.events = eventLog_.takeEvents();
  }

  // Returns false when run is finished.
  bool stepOnce() {
    if (state_ != State::RUNNING) {
      return false;
    }
    if (simMs_ >= scenario_.durationMs) {
      finalize();
      return false;
    }

    const unsigned long dtMs = effectiveDtMs(simMs_, scenario_.dtMs);
    const float dtS = dtMs / 1000.0f;
    lastAppliedDtMs_ = dtMs;

    const bool compassLostNow = inHeadingDropout(simMs_);
    if (compassLostNow && !compassLossActive_) {
      onControlEvent(simMs_, "COMPASS_LOST_EMU", "HEADING_UNAVAILABLE");
    } else if (!compassLostNow && compassLossActive_) {
      onControlEvent(simMs_, "COMPASS_RESTORED_EMU", "HEADING_AVAILABLE");
    }
    compassLossActive_ = compassLostNow;

    const bool displayLostNow = inDisplayLoss(simMs_);
    if (displayLostNow && !displayLossActive_) {
      onControlEvent(simMs_, "DISPLAY_LOST_EMU", "DISPLAY_OFFLINE");
    } else if (!displayLostNow && displayLossActive_) {
      onControlEvent(simMs_, "DISPLAY_RESTORED_EMU", "DISPLAY_ONLINE");
    }
    displayLossActive_ = displayLostNow;

    const bool powerLossNow = inPowerLoss(simMs_);
    if (powerLossNow && !powerLossActive_) {
      onControlEvent(simMs_, "POWER_LOSS_EMU", "POWER_OFF");
      onControlEvent(simMs_, "FAILSAFE_TRIGGERED", "POWER_LOSS_EMU");
      sensors_.reset();
      actuator_.apply(ActuatorCmd());
    } else if (!powerLossNow && powerLossActive_) {
      onControlEvent(simMs_, "POWER_RESTORED_EMU", "POWER_ON");
      sensors_.reset();
      controller_.reset();
      controller_.requestAnchor(true);
    }
    powerLossActive_ = powerLossNow;

    const GnssFix inputFix = sensors_.getFix();
    const HeadingSample inputHeading = sensors_.getHeading();

    simctl::ControllerTelemetry tele;
    ActuatorCmd cmd;
    if (powerLossNow) {
      cmd = ActuatorCmd();
      actuator_.apply(cmd);
      tele.anchorActive = false;
      tele.gnssGood = false;
      tele.safeStop = true;
    } else {
      controller_.step(scenario_.anchorXM, scenario_.anchorYM);
      tele = controller_.telemetry();
      cmd = actuator_.last();
    }

    const float thrustScale = actuatorScaleAtMs(simMs_);
    const bool derateNow = thrustScale < 0.999f;
    if (derateNow && !actuatorDerateActive_) {
      char details[32];
      snprintf(details, sizeof(details), "THRUST_SCALE_%.2f", thrustScale);
      onControlEvent(simMs_, "ACTUATOR_DERATE_EMU", details);
    } else if (!derateNow && actuatorDerateActive_) {
      onControlEvent(simMs_, "ACTUATOR_DERATE_CLEAR_EMU", "THRUST_SCALE_1.00");
    }
    actuatorDerateActive_ = derateNow;
    if (derateNow && !powerLossNow) {
      cmd.thrust = clampf(cmd.thrust * thrustScale, 0.0f, 1.0f);
      cmd.stop = (cmd.thrust <= 0.0f);
      actuator_.apply(cmd);
    }

    const Vec2 current = currentAtMs(simMs_);
    updateCurrentEventLog(simMs_);
    updateWaveWakePacketEventLog(simMs_);
    world_.step(dtS, cmd, current);
    applyWaveWakeYawDisturbance(simMs_, dtS);

    const BoatWorldState& st = world_.state();
    const float errTrue = hypotf(st.xM - scenario_.anchorXM, st.yM - scenario_.anchorYM);
    if (!isfinite(errTrue)) {
      result_.metrics.hardFailure = true;
      result_.metrics.hardFailureReason = "ERR_NAN";
    }
    errorHist_.add(errTrue);
    samples_++;
    result_.metrics.maxErrorM = std::max(result_.metrics.maxErrorM, errTrue);
    if (errTrue <= controller_.config().deadbandM) {
      deadbandCount_++;
    }
    if (cmd.thrust >= controller_.config().maxThrust - 0.01f) {
      saturatedCount_++;
    }
    if (tele.rampViolation) {
      result_.metrics.rampViolations++;
    }
    if (tele.nanDetected) {
      result_.metrics.nanDetected = true;
    }
    if (tele.commandOutOfRange) {
      result_.metrics.outOfRangeCommand = true;
    }
    // Count only meaningful direction flips, not low-thrust jitter from sensor noise.
    if (fabsf(cmd.steerDeg) > 20.0f && cmd.thrust > 0.20f) {
      const int sign = (cmd.steerDeg > 0.0f) ? 1 : -1;
      if (prevSteerSign_ != 0 && sign != prevSteerSign_) {
        signChanges_++;
      }
      prevSteerSign_ = sign;
    }

    if (tele.anchorActive && !tele.gnssGood) {
      if (badGnssAnchorSinceMs_ == 0) {
        badGnssAnchorSinceMs_ = simMs_;
      } else {
        const unsigned long badMs = simMs_ - badGnssAnchorSinceMs_;
        maxBadGnssAnchorMs_ = std::max(maxBadGnssAnchorMs_, badMs);
      }
    } else {
      badGnssAnchorSinceMs_ = 0;
    }

    live_.valid = true;
    live_.simMs = simMs_;
    live_.gnss = inputFix;
    live_.heading = inputHeading;
    live_.world = st;
    live_.command = cmd;
    live_.anchorActive = tele.anchorActive;
    live_.gnssGood = tele.gnssGood;
    live_.anchorXM = scenario_.anchorXM;
    live_.anchorYM = scenario_.anchorYM;
    live_.errTrueM = errTrue;
    live_.bearingDeg = simctl::bearingDeg(st.xM, st.yM, scenario_.anchorXM, scenario_.anchorYM);
    live_.speedMps = hypotf(st.vxBody + current.x, st.vyBody + current.y);

    clock_.advance(dtMs);
    simMs_ += dtMs;
    if (!powerLossNow) {
      sensors_.update(clock_.now_ms(), world_.state(), &rng_);
    }

    progressPct_ = 100.0f * ((float)simMs_ / (float)scenario_.durationMs);
    if (progressPct_ > 100.0f) {
      progressPct_ = 100.0f;
    }

    if (simMs_ >= scenario_.durationMs) {
      finalize();
      return false;
    }
    return true;
  }

  std::string statusJson() const {
    return buildSimStatusJson(scenario_.id,
                              stateStr(state_),
                              progressPct_,
                              simMs_,
                              scenario_.durationMs,
                              lastAppliedDtMs_);
  }

  std::string reportJson() const {
    return buildSimReportJson(scenario_.id,
                              stateStr(state_),
                              result_.pass,
                              result_.reason,
                              result_.metrics,
                              result_.events,
                              maxEventsInReport_);
  }

  static const char* stateStr(State s) {
    switch (s) {
      case State::RUNNING:
        return "RUNNING";
      case State::DONE:
        return "DONE";
      case State::ABORTED:
        return "ABORTED";
      case State::IDLE:
      default:
        return "IDLE";
    }
  }

private:
  Vec2 currentAtMs(unsigned long nowMs) const {
    Vec2 current = scenario_.env.baseCurrentMps;
    for (const TimedCurrent& g : scenario_.env.gusts) {
      if (simTimeWindowContains(nowMs, g.atMs, g.durationMs)) {
        current = g.currentMps;
      }
    }
    const Vec2 waveWake = waveWakeCurrentAtMs(nowMs);
    current.x += waveWake.x;
    current.y += waveWake.y;
    return current;
  }

  static const char* waveWakeEventCode(WaveWakeKind kind) {
    switch (kind) {
      case WaveWakeKind::CHOP:
        return "CHOP_PACKET_EMU";
      case WaveWakeKind::WAKE:
      default:
        return "WAKE_PACKET_EMU";
    }
  }

  static float waveWakeEnvelope(const TimedWaveWakePacket& packet,
                                unsigned long elapsedMs) {
    if (packet.durationMs == 0) {
      return 0.0f;
    }
    const float elapsedS = elapsedMs / 1000.0f;
    const float durationS = packet.durationMs / 1000.0f;
    const float rampS = std::max(0.5f, std::min(5.0f, durationS * 0.20f));
    const float attack = clampf(elapsedS / rampS, 0.0f, 1.0f);
    const float release = clampf((durationS - elapsedS) / rampS, 0.0f, 1.0f);
    return std::min(attack, release);
  }

  Vec2 waveWakeCurrentAtMs(unsigned long nowMs) const {
    Vec2 out;
    for (const TimedWaveWakePacket& packet : scenario_.env.waveWakePackets) {
      if (!simTimeWindowContains(nowMs, packet.atMs, packet.durationMs)) {
        continue;
      }
      const unsigned long elapsedMs = nowMs - packet.atMs;
      const float periodS = std::max(0.5f, packet.periodS);
      const float phase = 6.283185307f * ((elapsedMs / 1000.0f) / periodS);
      const float envelope = waveWakeEnvelope(packet, elapsedMs);
      const float scale = (packet.kind == WaveWakeKind::CHOP) ? 0.22f : 0.16f;
      const float speedMps = packet.heightM * scale * envelope * sinf(phase);
      const float dirRad = packet.directionDeg * 0.0174532925f;
      out.x += speedMps * sinf(dirRad);
      out.y += speedMps * cosf(dirRad);
    }
    return out;
  }

  float waveWakeYawRateDegSAtMs(unsigned long nowMs) const {
    float yawRate = 0.0f;
    for (const TimedWaveWakePacket& packet : scenario_.env.waveWakePackets) {
      if (!simTimeWindowContains(nowMs, packet.atMs, packet.durationMs)) {
        continue;
      }
      const unsigned long elapsedMs = nowMs - packet.atMs;
      const float periodS = std::max(0.5f, packet.periodS);
      const float phase = 6.283185307f * ((elapsedMs / 1000.0f) / periodS);
      const float envelope = waveWakeEnvelope(packet, elapsedMs);
      const float scale = (packet.kind == WaveWakeKind::CHOP) ? 18.0f : 11.0f;
      yawRate += (packet.heightM * scale / periodS) * envelope * sinf(phase);
    }
    return clampf(yawRate, -45.0f, 45.0f);
  }

  void applyWaveWakeYawDisturbance(unsigned long nowMs, float dtS) {
    const float yawRate = waveWakeYawRateDegSAtMs(nowMs);
    if (fabsf(yawRate) <= 0.001f) {
      return;
    }
    BoatWorldState& st = world_.state();
    st.psiDeg = simctl::wrap360f(st.psiDeg + yawRate * dtS);
  }

  bool inHeadingDropout(unsigned long nowMs) const {
    for (const TimedHeadingDropout& d : scenario_.sensors.headingDropouts) {
      if (simTimeWindowContains(nowMs, d.atMs, d.durationMs)) {
        return true;
      }
    }
    return false;
  }

  bool inPowerLoss(unsigned long nowMs) const {
    for (const TimedPowerLoss& p : scenario_.powerLosses) {
      if (simTimeWindowContains(nowMs, p.atMs, p.durationMs)) {
        return true;
      }
    }
    return false;
  }

  bool inDisplayLoss(unsigned long nowMs) const {
    for (const TimedDisplayLoss& d : scenario_.displayLosses) {
      if (simTimeWindowContains(nowMs, d.atMs, d.durationMs)) {
        return true;
      }
    }
    return false;
  }

  float actuatorScaleAtMs(unsigned long nowMs) const {
    float scale = 1.0f;
    for (const TimedActuatorDerate& d : scenario_.actuatorDerates) {
      if (simTimeWindowContains(nowMs, d.atMs, d.durationMs)) {
        scale = std::min(scale, clampf(d.thrustScale, 0.0f, 1.0f));
      }
    }
    return scale;
  }

  unsigned long effectiveDtMs(unsigned long nowMs, unsigned long defaultDtMs) const {
    for (const TimedLoopStall& s : scenario_.loopStalls) {
      if (simTimeWindowContains(nowMs, s.atMs, s.durationMs)) {
        return s.dtMs;
      }
    }
    return defaultDtMs;
  }

  void updateCurrentEventLog(unsigned long nowMs) {
    if (currentEventActive_.size() != scenario_.env.gusts.size()) {
      currentEventActive_.assign(scenario_.env.gusts.size(), false);
    }
    for (size_t i = 0; i < scenario_.env.gusts.size(); ++i) {
      const TimedCurrent& current = scenario_.env.gusts[i];
      if (current.eventCode.empty()) {
        continue;
      }
      const bool active = simTimeWindowContains(nowMs, current.atMs, current.durationMs);
      if (active == currentEventActive_[i]) {
        continue;
      }
      currentEventActive_[i] = active;
      if (active) {
        onControlEvent(nowMs, current.eventCode.c_str(), "ACTIVE");
      } else {
        const std::string clearCode = current.eventCode + "_CLEAR";
        onControlEvent(nowMs, clearCode.c_str(), "CLEAR");
      }
    }
  }

  void updateWaveWakePacketEventLog(unsigned long nowMs) {
    if (waveWakePacketActive_.size() != scenario_.env.waveWakePackets.size()) {
      waveWakePacketActive_.assign(scenario_.env.waveWakePackets.size(), false);
    }
    for (size_t i = 0; i < scenario_.env.waveWakePackets.size(); ++i) {
      const TimedWaveWakePacket& packet = scenario_.env.waveWakePackets[i];
      const bool active = simTimeWindowContains(nowMs, packet.atMs, packet.durationMs);
      if (active == waveWakePacketActive_[i]) {
        continue;
      }
      waveWakePacketActive_[i] = active;
      const std::string code = active
          ? waveWakeEventCode(packet.kind)
          : std::string(waveWakeEventCode(packet.kind)) + "_CLEAR";
      onControlEvent(nowMs, code.c_str(), active ? "ACTIVE" : "CLEAR");
    }
  }

  void finalize() {
    done_ = true;
    state_ = State::DONE;
    result_.events = eventLog_.takeEvents();

    if (samples_ == 0) {
      result_.pass = false;
      result_.reason = "NO_SAMPLES";
      return;
    }

    result_.metrics.p95ErrorM = errorHist_.p95(result_.metrics.maxErrorM);
    result_.metrics.timeInDeadbandPct = 100.0f * ((float)deadbandCount_ / (float)samples_);
    result_.metrics.timeSaturatedPct = 100.0f * ((float)saturatedCount_ / (float)samples_);
    const float minutes = (scenario_.durationMs / 1000.0f) / 60.0f;
    result_.metrics.dirChangesPerMin = (minutes > 0.0f) ? (signChanges_ / minutes) : 0.0f;
    result_.metrics.maxBadGnssInAnchorS = maxBadGnssAnchorMs_ / 1000.0f;

    const ScenarioExpect& ex = scenario_.expect;
    SimExpectationEval eval = evaluateSimMetrics(ex, result_.metrics);
    bool pass = eval.pass;
    std::string reason = eval.reason;
    if (pass) {
      for (const std::string& required : ex.requiredEvents) {
        if (!wasEventSeen(required)) {
          pass = false;
          reason = std::string("MISSING_EVENT_") + required;
          break;
        }
      }
    }

    result_.pass = pass;
    result_.reason = reason;
  }

private:
  bool wasEventSeen(const std::string& token) const {
    return eventLog_.wasSeen(token);
  }

  static constexpr size_t maxEventsInReport_ = 64;

  SimScenario scenario_;
  State state_ = State::IDLE;
  bool done_ = false;
  float progressPct_ = 0.0f;
  unsigned long simMs_ = 0;
  unsigned long lastAppliedDtMs_ = 0;

  VirtualClock clock_;
  BoatSim2D world_;
  SimSensorHub sensors_;
  ActuatorCapture actuator_;
  simctl::AnchorControlLoop controller_;
  XorShift32 rng_;

  SimErrorHistogram errorHist_;
  SimEventLog eventLog_;
  Result result_;
  LiveTelemetry live_;
  bool compassLossActive_ = false;
  bool powerLossActive_ = false;
  bool displayLossActive_ = false;
  bool actuatorDerateActive_ = false;
  std::vector<bool> currentEventActive_;
  std::vector<bool> waveWakePacketActive_;

  uint32_t samples_ = 0;
  uint32_t deadbandCount_ = 0;
  uint32_t saturatedCount_ = 0;
  float signChanges_ = 0.0f;
  int prevSteerSign_ = 0;
  unsigned long badGnssAnchorSinceMs_ = 0;
  unsigned long maxBadGnssAnchorMs_ = 0;
};

inline SimScenario makeBaseScenario(const char* id, float currentX, float posSigmaM) {
  SimScenario s;
  s.id = id;
  s.durationMs = 600000;
  s.dtMs = 50;
  s.seed = 12345;
  s.initialXM = 6.0f;
  s.initialYM = -2.0f;
  s.initialHeadingDeg = 20.0f;
  s.anchorXM = 0.0f;
  s.anchorYM = 0.0f;
  s.env.baseCurrentMps.x = currentX;
  s.env.baseCurrentMps.y = 0.0f;
  s.sensors.gnssHz = 5;
  s.sensors.posSigmaM = posSigmaM;
  s.sensors.hdop = 0.9f;
  s.sensors.sats = 16;
  s.sensors.headingSigmaDeg = 2.0f;
  s.expect.type = ScenarioExpect::Type::HOLD;
  s.expect.maxBadGnssInAnchorMaxS = 1.5f;
  return s;
}

inline void appendRussianWaterScenarios(std::vector<SimScenario>* v) {
  if (!v) return;

  SimScenario rf0 = makeBaseScenario("RF0", 0.35f, 0.91f);
  rf0.seed = 200001;
  rf0.durationMs = 300000;
  rf0.initialXM = 12.0f;
  rf0.initialYM = -3.0f;
  rf0.sensors.sats = 11;
  rf0.sensors.hdop = 1.2f;
  {
    TimedCurrent wake;
    wake.atMs = 110000;
    wake.durationMs = 24000;
    wake.currentMps.x = -0.10f;
    wake.currentMps.y = -0.55f;
    wake.eventCode = "WAKE_CURRENT_EMU";
    rf0.env.gusts.push_back(wake);
    TimedWaveWakePacket packet;
    packet.kind = WaveWakeKind::WAKE;
    packet.atMs = 110000;
    packet.durationMs = 24000;
    packet.heightM = 0.45f;
    packet.periodS = 1.7f;
    packet.directionDeg = 210.0f;
    rf0.env.waveWakePackets.push_back(packet);
  }
  rf0.expect.requiredEvents = {"WAKE_PACKET_EMU"};
  rf0.expect.p95ErrorMaxM = 34.0f;
  rf0.expect.maxErrorMaxM = 70.0f;
  rf0.expect.timeInDeadbandMinPct = 10.0f;
  rf0.expect.dirChangesPerMinMax = 90.0f;
  v->push_back(rf0);

  SimScenario rf1 = makeBaseScenario("RF1", 1.2f, 1.08f);
  rf1.seed = 200002;
  rf1.durationMs = 300000;
  rf1.initialXM = 16.0f;
  rf1.initialYM = -4.0f;
  rf1.sensors.sats = 11;
  rf1.sensors.hdop = 1.2f;
  rf1.maxThrustOverride = 0.85f;
  rf1.expect.p95ErrorMaxM = 135.0f;
  rf1.expect.maxErrorMaxM = 155.0f;
  rf1.expect.timeInDeadbandMinPct = -1.0f;
  rf1.expect.timeSaturatedMaxPct = 100.0f;
  v->push_back(rf1);

  SimScenario rf2 = makeBaseScenario("RF2", 0.10f, 1.63f);
  rf2.seed = 200003;
  rf2.durationMs = 300000;
  rf2.initialXM = 14.0f;
  rf2.initialYM = 6.0f;
  rf2.sensors.sats = 11;
  rf2.sensors.hdop = 1.2f;
  {
    TimedCurrent chop1;
    chop1.atMs = 72000;
    chop1.durationMs = 38000;
    chop1.currentMps.x = -0.70f;
    chop1.currentMps.y = -0.25f;
    chop1.eventCode = "CHOP_CURRENT_EMU";
    rf2.env.gusts.push_back(chop1);
    TimedCurrent chop2;
    chop2.atMs = 184000;
    chop2.durationMs = 42000;
    chop2.currentMps.x = -0.80f;
    chop2.currentMps.y = 0.20f;
    chop2.eventCode = "CHOP_CURRENT_EMU";
    rf2.env.gusts.push_back(chop2);
    TimedWaveWakePacket packet1;
    packet1.kind = WaveWakeKind::CHOP;
    packet1.atMs = 72000;
    packet1.durationMs = 38000;
    packet1.heightM = 0.85f;
    packet1.periodS = 1.45f;
    packet1.directionDeg = 255.0f;
    rf2.env.waveWakePackets.push_back(packet1);
    TimedWaveWakePacket packet2;
    packet2.kind = WaveWakeKind::CHOP;
    packet2.atMs = 184000;
    packet2.durationMs = 42000;
    packet2.heightM = 0.95f;
    packet2.periodS = 1.5f;
    packet2.directionDeg = 270.0f;
    rf2.env.waveWakePackets.push_back(packet2);
  }
  rf2.expect.requiredEvents = {"CHOP_PACKET_EMU"};
  rf2.expect.p95ErrorMaxM = 110.0f;
  rf2.expect.maxErrorMaxM = 190.0f;
  rf2.expect.timeInDeadbandMinPct = -1.0f;
  rf2.expect.dirChangesPerMinMax = 120.0f;
  v->push_back(rf2);

  SimScenario rf3 = makeBaseScenario("RF3", 0.40f, 2.45f);
  rf3.seed = 200004;
  rf3.durationMs = 180000;
  rf3.initialXM = 14.0f;
  rf3.initialYM = 5.0f;
  rf3.sensors.sats = 11;
  rf3.sensors.hdop = 1.2f;
  {
    TimedCurrent storm;
    storm.atMs = 25000;
    storm.durationMs = 60000;
    storm.currentMps.x = -1.0f;
    storm.currentMps.y = -0.85f;
    storm.eventCode = "STORM_CURRENT_EMU";
    rf3.env.gusts.push_back(storm);
    TimedWaveWakePacket packet;
    packet.kind = WaveWakeKind::CHOP;
    packet.atMs = 25000;
    packet.durationMs = 60000;
    packet.heightM = 3.0f;
    packet.periodS = 3.5f;
    packet.directionDeg = 210.0f;
    rf3.env.waveWakePackets.push_back(packet);
    TimedHdop h;
    h.atMs = 45000;
    h.durationMs = 45000;
    h.hdop = 4.2f;
    rf3.sensors.hdopChanges.push_back(h);
  }
  rf3.expect.type = ScenarioExpect::Type::FAILSAFE;
  rf3.expect.requiredEvents = {"CHOP_PACKET_EMU", "GPS_HDOP_TOO_HIGH", "FAILSAFE_TRIGGERED"};
  rf3.expect.p95ErrorMaxM = -1.0f;
  rf3.expect.maxErrorMaxM = -1.0f;
  rf3.expect.timeInDeadbandMinPct = -1.0f;
  v->push_back(rf3);

  SimScenario rf4 = makeBaseScenario("RF4", 0.15f, 1.46f);
  rf4.seed = 200005;
  rf4.durationMs = 300000;
  rf4.initialXM = 12.0f;
  rf4.initialYM = 4.0f;
  rf4.sensors.sats = 11;
  rf4.sensors.hdop = 1.2f;
  {
    TimedCurrent drift1;
    drift1.atMs = 70000;
    drift1.durationMs = 50000;
    drift1.currentMps.x = 0.45f;
    drift1.currentMps.y = 0.20f;
    drift1.eventCode = "WIND_DRIFT_EMU";
    rf4.env.gusts.push_back(drift1);
    TimedCurrent drift2;
    drift2.atMs = 185000;
    drift2.durationMs = 45000;
    drift2.currentMps.x = -0.35f;
    drift2.currentMps.y = 0.40f;
    drift2.eventCode = "WIND_DRIFT_EMU";
    rf4.env.gusts.push_back(drift2);
    TimedWaveWakePacket packet;
    packet.kind = WaveWakeKind::WAKE;
    packet.atMs = 90000;
    packet.durationMs = 30000;
    packet.heightM = 1.2f;
    packet.periodS = 3.5f;
    packet.directionDeg = 80.0f;
    rf4.env.waveWakePackets.push_back(packet);
  }
  rf4.expect.requiredEvents = {"WIND_DRIFT_EMU", "WAKE_PACKET_EMU"};
  rf4.expect.p95ErrorMaxM = 95.0f;
  rf4.expect.maxErrorMaxM = 170.0f;
  rf4.expect.timeInDeadbandMinPct = -1.0f;
  rf4.expect.dirChangesPerMinMax = 120.0f;
  v->push_back(rf4);
}

inline std::vector<SimScenario> defaultScenarios() {
  std::vector<SimScenario> v;

  SimScenario s0 = makeBaseScenario("S0", 0.0f, 0.8f);
  // "Hold still" baseline should start close to anchor; otherwise max_error<=5m
  // is impossible with the shared base init point (~6.32m from anchor).
  s0.initialXM = 0.8f;
  s0.initialYM = -0.4f;
  s0.initialHeadingDeg = 20.0f;
  s0.expect.p95ErrorMaxM = 2.5f;
  s0.expect.maxErrorMaxM = 5.0f;
  s0.expect.timeInDeadbandMinPct = 80.0f;
  s0.expect.dirChangesPerMinMax = 25.0f;
  v.push_back(s0);

  SimScenario s1 = makeBaseScenario("S1", 0.4f, 0.9f);
  s1.expect.p95ErrorMaxM = 3.5f;
  s1.expect.maxErrorMaxM = 8.0f;
  s1.expect.timeInDeadbandMinPct = 60.0f;
  v.push_back(s1);

  SimScenario s2 = makeBaseScenario("S2", 0.8f, 1.1f);
  // Hard-current case needs higher authority than default 0.60 to be physically
  // holdable against 0.8 m/s current with this world model.
  s2.maxThrustOverride = 0.70f;
  s2.expect.p95ErrorMaxM = 5.0f;
  s2.expect.maxErrorMaxM = 12.0f;
  s2.expect.timeInDeadbandMinPct = 45.0f;
  s2.expect.timeSaturatedMaxPct = 75.0f;
  v.push_back(s2);

  SimScenario s3 = makeBaseScenario("S3", 0.3f, 1.0f);
  {
    TimedCurrent g;
    g.atMs = 120000;
    g.durationMs = 15000;
    g.currentMps.x = 1.0f;
    g.currentMps.y = 0.2f;
    s3.env.gusts.push_back(g);
  }
  s3.expect.maxErrorMaxM = 15.0f;
  s3.expect.dirChangesPerMinMax = 60.0f;
  s3.expect.timeInDeadbandMinPct = -1.0f;
  s3.expect.p95ErrorMaxM = -1.0f;
  v.push_back(s3);

  SimScenario s4 = makeBaseScenario("S4", 0.4f, 0.9f);
  {
    TimedDropout d;
    d.atMs = 240000;
    d.durationMs = 3000;
    s4.sensors.dropouts.push_back(d);
  }
  s4.expect.type = ScenarioExpect::Type::FAILSAFE;
  s4.expect.requiredEvents = {"GPS_DATA_STALE", "FAILSAFE_TRIGGERED"};
  s4.expect.maxErrorMaxM = -1.0f;
  s4.expect.p95ErrorMaxM = -1.0f;
  s4.expect.timeInDeadbandMinPct = -1.0f;
  v.push_back(s4);

  SimScenario s5 = makeBaseScenario("S5", 0.4f, 0.9f);
  {
    TimedJump j;
    j.atMs = 300000;
    j.durationMs = 200;
    j.jumpM.x = 12.0f;
    j.jumpM.y = -8.0f;
    s5.sensors.jumps.push_back(j);
  }
  s5.expect.maxErrorMaxM = 15.0f;
  s5.expect.requiredEvents = {"POSITION_JUMP_DETECTED"};
  s5.expect.timeInDeadbandMinPct = -1.0f;
  s5.expect.p95ErrorMaxM = -1.0f;
  v.push_back(s5);

  SimScenario s6 = makeBaseScenario("S6", 0.4f, 1.0f);
  {
    TimedHdop h;
    h.atMs = 180000;
    h.durationMs = 30000;
    h.hdop = 3.5f;
    s6.sensors.hdopChanges.push_back(h);
  }
  s6.expect.type = ScenarioExpect::Type::FAILSAFE;
  s6.expect.requiredEvents = {"GPS_HDOP_TOO_HIGH", "FAILSAFE_TRIGGERED"};
  s6.expect.p95ErrorMaxM = -1.0f;
  s6.expect.maxErrorMaxM = -1.0f;
  s6.expect.timeInDeadbandMinPct = -1.0f;
  v.push_back(s6);

  SimScenario s7 = makeBaseScenario("S7", 0.4f, 1.0f);
  {
    TimedSats s;
    s.atMs = 180000;
    s.durationMs = 20000;
    s.sats = 6;
    s7.sensors.satsChanges.push_back(s);
  }
  s7.expect.type = ScenarioExpect::Type::FAILSAFE;
  s7.expect.requiredEvents = {"GPS_SATS_TOO_LOW", "FAILSAFE_TRIGGERED"};
  s7.expect.p95ErrorMaxM = -1.0f;
  s7.expect.maxErrorMaxM = -1.0f;
  s7.expect.timeInDeadbandMinPct = -1.0f;
  v.push_back(s7);

  SimScenario s8 = makeBaseScenario("S8", 0.4f, 0.9f);
  {
    TimedLoopStall stall;
    stall.atMs = 100000;
    stall.durationMs = 1500;
    stall.dtMs = 500;
    s8.loopStalls.push_back(stall);
  }
  s8.expect.type = ScenarioExpect::Type::FAILSAFE;
  s8.expect.requiredEvents = {"CONTROL_LOOP_TIMEOUT", "FAILSAFE_TRIGGERED"};
  s8.expect.p95ErrorMaxM = -1.0f;
  s8.expect.maxErrorMaxM = -1.0f;
  s8.expect.timeInDeadbandMinPct = -1.0f;
  v.push_back(s8);

  SimScenario s9 = makeBaseScenario("S9", 0.4f, 0.9f);
  {
    TimedNanHeading h;
    h.atMs = 80000;
    h.durationMs = 50;
    s9.sensors.nanHeading.push_back(h);
  }
  s9.expect.type = ScenarioExpect::Type::FAILSAFE;
  s9.expect.requiredEvents = {"INTERNAL_ERROR_NAN", "FAILSAFE_TRIGGERED"};
  s9.expect.p95ErrorMaxM = -1.0f;
  s9.expect.maxErrorMaxM = -1.0f;
  s9.expect.timeInDeadbandMinPct = -1.0f;
  v.push_back(s9);

  SimScenario s10 = makeBaseScenario("S10", 0.42f, 1.5f);
  s10.seed = 420101;
  s10.durationMs = 360000;
  s10.sensors.gnssHz = 1;
  s10.sensors.hdop = 1.3f;
  s10.sensors.sats = 13;
  s10.env.baseCurrentMps.y = 0.12f;
  {
    TimedCurrent g1;
    g1.atMs = 90000;
    g1.durationMs = 20000;
    g1.currentMps.x = 0.72f;
    g1.currentMps.y = -0.15f;
    s10.env.gusts.push_back(g1);
    TimedCurrent g2;
    g2.atMs = 230000;
    g2.durationMs = 15000;
    g2.currentMps.x = -0.20f;
    g2.currentMps.y = 0.55f;
    s10.env.gusts.push_back(g2);
  }
  s10.expect.p95ErrorMaxM = 6.5f;
  s10.expect.maxErrorMaxM = 15.0f;
  s10.expect.timeInDeadbandMinPct = 30.0f;
  s10.expect.dirChangesPerMinMax = 90.0f;
  v.push_back(s10);

  SimScenario s11 = makeBaseScenario("S11", 0.55f, 1.2f);
  s11.seed = 112233;
  s11.durationMs = 360000;
  s11.env.baseCurrentMps.y = -0.22f;
  {
    TimedCurrent g;
    g.atMs = 150000;
    g.durationMs = 12000;
    g.currentMps.x = 0.95f;
    g.currentMps.y = -0.35f;
    s11.env.gusts.push_back(g);
  }
  s11.expect.p95ErrorMaxM = 5.8f;
  s11.expect.maxErrorMaxM = 14.0f;
  s11.expect.timeInDeadbandMinPct = 35.0f;
  v.push_back(s11);

  SimScenario s12 = makeBaseScenario("S12", 0.4f, 0.9f);
  s12.seed = 120012;
  s12.durationMs = 240000;
  {
    TimedHeadingDropout d;
    d.atMs = 120000;
    d.durationMs = 5000;
    s12.sensors.headingDropouts.push_back(d);
  }
  s12.expect.type = ScenarioExpect::Type::FAILSAFE;
  s12.expect.requiredEvents = {
      "COMPASS_LOST_EMU", "SENSOR_TIMEOUT", "FAILSAFE_TRIGGERED", "COMPASS_RESTORED_EMU"};
  s12.expect.p95ErrorMaxM = -1.0f;
  s12.expect.maxErrorMaxM = -1.0f;
  s12.expect.timeInDeadbandMinPct = -1.0f;
  v.push_back(s12);

  SimScenario s13 = makeBaseScenario("S13", 0.4f, 0.9f);
  s13.seed = 130013;
  s13.durationMs = 240000;
  {
    TimedHeadingDropout d1;
    d1.atMs = 70000;
    d1.durationMs = 1000;
    s13.sensors.headingDropouts.push_back(d1);
    TimedHeadingDropout d2;
    d2.atMs = 90000;
    d2.durationMs = 2600;
    s13.sensors.headingDropouts.push_back(d2);
  }
  s13.expect.type = ScenarioExpect::Type::FAILSAFE;
  s13.expect.requiredEvents = {"COMPASS_LOST_EMU", "SENSOR_TIMEOUT", "FAILSAFE_TRIGGERED"};
  s13.expect.p95ErrorMaxM = -1.0f;
  s13.expect.maxErrorMaxM = -1.0f;
  s13.expect.timeInDeadbandMinPct = -1.0f;
  v.push_back(s13);

  SimScenario s14 = makeBaseScenario("S14", 0.4f, 1.0f);
  s14.seed = 140014;
  s14.durationMs = 240000;
  {
    TimedPowerLoss p;
    p.atMs = 110000;
    p.durationMs = 2000;
    s14.powerLosses.push_back(p);
  }
  s14.expect.type = ScenarioExpect::Type::FAILSAFE;
  s14.expect.requiredEvents = {"POWER_LOSS_EMU", "POWER_RESTORED_EMU", "FAILSAFE_TRIGGERED"};
  s14.expect.p95ErrorMaxM = -1.0f;
  s14.expect.maxErrorMaxM = -1.0f;
  s14.expect.timeInDeadbandMinPct = -1.0f;
  v.push_back(s14);

  SimScenario s15 = makeBaseScenario("S15", 0.45f, 1.0f);
  s15.seed = 150015;
  s15.durationMs = 280000;
  {
    TimedPowerLoss p1;
    p1.atMs = 60000;
    p1.durationMs = 1200;
    s15.powerLosses.push_back(p1);
    TimedPowerLoss p2;
    p2.atMs = 170000;
    p2.durationMs = 1800;
    s15.powerLosses.push_back(p2);
  }
  s15.expect.type = ScenarioExpect::Type::FAILSAFE;
  s15.expect.requiredEvents = {"POWER_LOSS_EMU", "POWER_RESTORED_EMU", "FAILSAFE_TRIGGERED"};
  s15.expect.p95ErrorMaxM = -1.0f;
  s15.expect.maxErrorMaxM = -1.0f;
  s15.expect.timeInDeadbandMinPct = -1.0f;
  v.push_back(s15);

  SimScenario s16 = makeBaseScenario("S16", 0.3f, 1.0f);
  s16.seed = 160016;
  s16.durationMs = 300000;
  {
    TimedDisplayLoss d;
    d.atMs = 80000;
    d.durationMs = 30000;
    s16.displayLosses.push_back(d);
  }
  s16.expect.requiredEvents = {"DISPLAY_LOST_EMU", "DISPLAY_RESTORED_EMU"};
  s16.expect.p95ErrorMaxM = 4.5f;
  s16.expect.maxErrorMaxM = 11.0f;
  s16.expect.timeInDeadbandMinPct = 45.0f;
  v.push_back(s16);

  SimScenario s17 = makeBaseScenario("S17", 0.45f, 1.0f);
  s17.seed = 170017;
  s17.durationMs = 360000;
  {
    TimedActuatorDerate d;
    d.atMs = 100000;
    d.durationMs = 120000;
    d.thrustScale = 0.45f;
    s17.actuatorDerates.push_back(d);
  }
  s17.expect.requiredEvents = {"ACTUATOR_DERATE_EMU", "ACTUATOR_DERATE_CLEAR_EMU"};
  s17.expect.p95ErrorMaxM = 7.5f;
  s17.expect.maxErrorMaxM = 18.0f;
  s17.expect.timeInDeadbandMinPct = 20.0f;
  v.push_back(s17);

  SimScenario s18 = makeBaseScenario("S18", 0.4f, 0.95f);
  s18.seed = 180018;
  s18.durationMs = 260000;
  {
    TimedHeadingDropout h;
    h.atMs = 140000;
    h.durationMs = 3000;
    s18.sensors.headingDropouts.push_back(h);
    TimedDisplayLoss d;
    d.atMs = 139000;
    d.durationMs = 6000;
    s18.displayLosses.push_back(d);
  }
  s18.expect.type = ScenarioExpect::Type::FAILSAFE;
  s18.expect.requiredEvents = {
      "COMPASS_LOST_EMU", "DISPLAY_LOST_EMU", "SENSOR_TIMEOUT", "FAILSAFE_TRIGGERED"};
  s18.expect.p95ErrorMaxM = -1.0f;
  s18.expect.maxErrorMaxM = -1.0f;
  s18.expect.timeInDeadbandMinPct = -1.0f;
  v.push_back(s18);

  SimScenario s19 = makeBaseScenario("S19", 0.6f, 1.4f);
  s19.seed = 191919;
  s19.durationMs = 360000;
  s19.sensors.gnssHz = 1;
  s19.sensors.hdop = 1.2f;
  s19.sensors.sats = 12;
  {
    TimedCurrent g;
    g.atMs = 50000;
    g.durationMs = 15000;
    g.currentMps.x = 0.95f;
    g.currentMps.y = 0.20f;
    s19.env.gusts.push_back(g);
    TimedDropout d;
    d.atMs = 210000;
    d.durationMs = 3000;
    s19.sensors.dropouts.push_back(d);
    TimedHdop h;
    h.atMs = 160000;
    h.durationMs = 20000;
    h.hdop = 3.3f;
    s19.sensors.hdopChanges.push_back(h);
    TimedPowerLoss p;
    p.atMs = 250000;
    p.durationMs = 1600;
    s19.powerLosses.push_back(p);
    TimedDisplayLoss dl;
    dl.atMs = 252000;
    dl.durationMs = 8000;
    s19.displayLosses.push_back(dl);
    TimedActuatorDerate ad;
    ad.atMs = 90000;
    ad.durationMs = 50000;
    ad.thrustScale = 0.55f;
    s19.actuatorDerates.push_back(ad);
  }
  s19.expect.type = ScenarioExpect::Type::FAILSAFE;
  s19.expect.requiredEvents = {
      "GPS_HDOP_TOO_HIGH", "FAILSAFE_TRIGGERED", "POWER_LOSS_EMU", "DISPLAY_LOST_EMU"};
  s19.expect.p95ErrorMaxM = -1.0f;
  s19.expect.maxErrorMaxM = -1.0f;
  s19.expect.timeInDeadbandMinPct = -1.0f;
  v.push_back(s19);

  appendRussianWaterScenarios(&v);

  return v;
}

class HilSimManager {
public:
  HilSimManager() : scenarios_(defaultScenarios()) {}

  bool startRun(const std::string& scenarioId, int speedupMode, std::string* err) {
    if (runner_.isRunning()) {
      if (err) *err = "already running";
      return false;
    }
    const SimScenario* s = findScenario(scenarioId);
    if (!s) {
      if (err) *err = "unknown scenario";
      return false;
    }
    speedupMode_ = (speedupMode == 1) ? 1 : 0;
    wallAccumMs_ = 0;
    if (!runner_.start(*s)) {
      if (err) *err = "start failed";
      return false;
    }
    lastReportJson_.clear();
    return true;
  }

  void abortRun() {
    runner_.abort();
    lastReportJson_ = runner_.reportJson();
  }

  void loop(unsigned long wallDtMs) {
    if (!runner_.isRunning()) {
      return;
    }
    if (speedupMode_ == 0) {
      const int batch = 16;
      for (int i = 0; i < batch; ++i) {
        if (!runner_.stepOnce()) break;
      }
    } else {
      wallAccumMs_ += wallDtMs;
      const unsigned long dtMs = std::max(1UL, runner_.scenario().dtMs);
      while (wallAccumMs_ >= dtMs && runner_.isRunning()) {
        wallAccumMs_ -= dtMs;
        if (!runner_.stepOnce()) {
          break;
        }
      }
    }
    if (runner_.isDone()) {
      lastReportJson_ = runner_.reportJson();
    }
  }

  std::string listCsv() const {
    std::string out;
    for (size_t i = 0; i < scenarios_.size(); ++i) {
      if (i > 0) out += ",";
      out += scenarios_[i].id;
    }
    return out;
  }

  std::string statusJson() const { return runner_.statusJson(); }

  std::string reportJson(const std::string& scenarioId = "") const {
    (void)scenarioId;
    if (!lastReportJson_.empty()) {
      return lastReportJson_;
    }
    return runner_.reportJson();
  }

  const char* stateLabel() const {
    return HilScenarioRunner::stateStr(runner_.state());
  }

  float progressPct() const { return runner_.progressPct(); }

  int lastPass() const {
    if (!runner_.isDone()) return -1;
    return runner_.result().pass ? 1 : 0;
  }

  bool isRunning() const { return runner_.isRunning(); }

  bool liveTelemetry(HilScenarioRunner::LiveTelemetry* out) const {
    if (!out) return false;
    *out = runner_.live();
    return out->valid;
  }

  std::string currentScenarioId() const { return runner_.scenario().id; }

private:
  const SimScenario* findScenario(const std::string& id) const {
    for (const SimScenario& s : scenarios_) {
      if (s.id == id) {
        return &s;
      }
    }
    return nullptr;
  }

private:
  std::vector<SimScenario> scenarios_;
  HilScenarioRunner runner_;
  int speedupMode_ = 0;
  unsigned long wallAccumMs_ = 0;
  std::string lastReportJson_;
};

} // namespace hilsim
