#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <algorithm>
#include <string>
#include <vector>

#include "AnchorControlLoop.h"
#include "ControlInterfaces.h"

namespace hilsim {

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

inline float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

inline bool inWindow(unsigned long nowMs, unsigned long atMs, unsigned long durationMs) {
  if (nowMs < atMs) return false;
  return nowMs < (atMs + durationMs);
}

class XorShift32 {
public:
  explicit XorShift32(uint32_t seed = 1) : state_(seed == 0 ? 0x9E3779B9u : seed) {}

  uint32_t nextU32() {
    uint32_t x = state_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state_ = x;
    return x;
  }

  float uniform01() { return (nextU32() & 0x00FFFFFFu) / 16777216.0f; }

  float gauss(float sigma) {
    float acc = 0.0f;
    for (int i = 0; i < 6; ++i) {
      acc += uniform01();
    }
    return (acc - 3.0f) * sigma;
  }

private:
  uint32_t state_ = 1;
};

class VirtualClock : public IClock {
public:
  unsigned long now_ms() const override { return nowMs_; }
  void set(unsigned long ms) { nowMs_ = ms; }
  void advance(unsigned long ms) { nowMs_ += ms; }

private:
  unsigned long nowMs_ = 0;
};

class ActuatorCapture : public IActuator {
public:
  void apply(const ActuatorCmd& cmd) override { lastCmd_ = cmd; }
  const ActuatorCmd& last() const { return lastCmd_; }

private:
  ActuatorCmd lastCmd_;
};

struct BoatWorldConfig {
  float vMaxMps = 1.6f;
  float tauV = 1.2f;
  float tauPsi = 0.8f;
  float maxTurnRateDegS = 120.0f;
};

struct BoatWorldState {
  float xM = 0.0f;
  float yM = 0.0f;
  float vxBody = 0.0f;
  float vyBody = 0.0f;
  float psiDeg = 0.0f;
};

class BoatSim2D {
public:
  void setConfig(const BoatWorldConfig& cfg) { cfg_ = cfg; }
  const BoatWorldState& state() const { return st_; }
  BoatWorldState& state() { return st_; }

  void reset(float xM, float yM, float headingDeg) {
    st_.xM = xM;
    st_.yM = yM;
    st_.psiDeg = headingDeg;
    st_.vxBody = 0.0f;
    st_.vyBody = 0.0f;
  }

  void step(float dtS, const ActuatorCmd& cmd, Vec2 currentMps) {
    const float thrust = clampf(cmd.thrust, 0.0f, 1.0f);
    const float steerDeg = clampf(cmd.steerDeg, -180.0f, 180.0f);
    const float dirDeg = st_.psiDeg + steerDeg;
    const float dirRad = dirDeg * 0.0174532925f;

    const float vTarget = cfg_.vMaxMps * thrust;
    const float vTx = vTarget * sinf(dirRad);
    const float vTy = vTarget * cosf(dirRad);

    float alphaV = dtS / std::max(0.05f, cfg_.tauV);
    alphaV = clampf(alphaV, 0.0f, 1.0f);
    st_.vxBody += (vTx - st_.vxBody) * alphaV;
    st_.vyBody += (vTy - st_.vyBody) * alphaV;

    float psiErr = simctl::wrap180f(dirDeg - st_.psiDeg);
    float psiRate = psiErr / std::max(0.05f, cfg_.tauPsi);
    psiRate = clampf(psiRate, -cfg_.maxTurnRateDegS, cfg_.maxTurnRateDegS);
    st_.psiDeg = simctl::wrap360f(st_.psiDeg + psiRate * dtS);

    const float vWx = st_.vxBody + currentMps.x;
    const float vWy = st_.vyBody + currentMps.y;
    st_.xM += vWx * dtS;
    st_.yM += vWy * dtS;
  }

private:
  BoatWorldConfig cfg_;
  BoatWorldState st_;
};

struct TimedCurrent {
  unsigned long atMs = 0;
  unsigned long durationMs = 0;
  Vec2 currentMps;
};

struct TimedDropout {
  unsigned long atMs = 0;
  unsigned long durationMs = 0;
};

struct TimedJump {
  unsigned long atMs = 0;
  unsigned long durationMs = 0;
  Vec2 jumpM;
};

struct TimedHdop {
  unsigned long atMs = 0;
  unsigned long durationMs = 0;
  float hdop = 0.9f;
};

struct TimedSats {
  unsigned long atMs = 0;
  unsigned long durationMs = 0;
  int sats = 16;
};

struct TimedLoopStall {
  unsigned long atMs = 0;
  unsigned long durationMs = 0;
  unsigned long dtMs = 500;
};

struct TimedNanHeading {
  unsigned long atMs = 0;
  unsigned long durationMs = 50;
};

struct SensorConfig {
  int gnssHz = 5;
  float posSigmaM = 0.8f;
  float headingSigmaDeg = 2.0f;
  float headingDriftDegS = 0.0f;
  float hdop = 0.9f;
  int sats = 16;
  std::vector<TimedDropout> dropouts;
  std::vector<TimedJump> jumps;
  std::vector<TimedHdop> hdopChanges;
  std::vector<TimedSats> satsChanges;
  std::vector<TimedNanHeading> nanHeading;
};

struct EnvironmentConfig {
  Vec2 baseCurrentMps;
  std::vector<TimedCurrent> gusts;
};

struct ScenarioExpect {
  enum class Type : uint8_t {
    HOLD = 0,
    FAILSAFE = 1,
  };

  Type type = Type::HOLD;
  float p95ErrorMaxM = -1.0f;
  float maxErrorMaxM = -1.0f;
  float timeInDeadbandMinPct = -1.0f;
  float timeSaturatedMaxPct = -1.0f;
  float dirChangesPerMinMax = -1.0f;
  float maxBadGnssInAnchorMaxS = -1.0f;
  std::vector<std::string> requiredEvents;
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
  EnvironmentConfig env;
  SensorConfig sensors;
  ScenarioExpect expect;
  std::vector<TimedLoopStall> loopStalls;
};

struct SimEvent {
  unsigned long atMs = 0;
  std::string code;
  std::string details;
};

class SimSensorHub : public IGnssSource, public IHeadingSource {
public:
  void configure(const SensorConfig& cfg) {
    cfg_ = cfg;
    gnssPeriodMs_ = (cfg.gnssHz <= 0) ? 1000 : (1000UL / (unsigned long)cfg.gnssHz);
    if (gnssPeriodMs_ == 0) {
      gnssPeriodMs_ = 1;
    }
  }

  void reset() {
    lastGnssEmitMs_ = 0;
    lastGnssUpdateMs_ = 0;
    headingBiasDeg_ = 0.0f;
    fix_ = {};
    fix_.ageMs = 999999UL;
    heading_ = {};
    heading_.ageMs = 999999UL;
  }

  void update(unsigned long nowMs, const BoatWorldState& st, XorShift32* rng) {
    // Heading each tick.
    heading_.valid = true;
    headingBiasDeg_ += cfg_.headingDriftDegS * 0.001f * (float)(nowMs - lastHeadingMs_);
    lastHeadingMs_ = nowMs;
    heading_.headingDeg =
        simctl::wrap360f(st.psiDeg + headingBiasDeg_ + rng->gauss(cfg_.headingSigmaDeg));
    heading_.ageMs = 0;
    for (const TimedNanHeading& inj : cfg_.nanHeading) {
      if (inWindow(nowMs, inj.atMs, inj.durationMs)) {
        heading_.headingDeg = NAN;
        heading_.valid = true;
        break;
      }
    }

    if (nowMs - lastGnssEmitMs_ < gnssPeriodMs_) {
      fix_.ageMs = nowMs - lastGnssUpdateMs_;
      heading_.ageMs = 0;
      return;
    }
    lastGnssEmitMs_ = nowMs;

    bool dropped = false;
    for (const TimedDropout& d : cfg_.dropouts) {
      if (inWindow(nowMs, d.atMs, d.durationMs)) {
        dropped = true;
        break;
      }
    }

    if (dropped) {
      // Keep last fix frozen so age naturally becomes stale.
      if (lastGnssUpdateMs_ > 0) {
        fix_.ageMs = nowMs - lastGnssUpdateMs_;
      } else {
        fix_.valid = false;
        fix_.ageMs = 999999UL;
      }
      return;
    }

    float x = st.xM + rng->gauss(cfg_.posSigmaM);
    float y = st.yM + rng->gauss(cfg_.posSigmaM);
    for (const TimedJump& j : cfg_.jumps) {
      if (inWindow(nowMs, j.atMs, j.durationMs)) {
        x += j.jumpM.x;
        y += j.jumpM.y;
      }
    }

    float hdop = cfg_.hdop;
    for (const TimedHdop& h : cfg_.hdopChanges) {
      if (inWindow(nowMs, h.atMs, h.durationMs)) {
        hdop = h.hdop;
      }
    }

    int sats = cfg_.sats;
    for (const TimedSats& s : cfg_.satsChanges) {
      if (inWindow(nowMs, s.atMs, s.durationMs)) {
        sats = s.sats;
      }
    }

    fix_.valid = true;
    fix_.xM = x;
    fix_.yM = y;
    fix_.sats = sats;
    fix_.hdop = hdop;
    fix_.ageMs = 0;
    lastGnssUpdateMs_ = nowMs;
  }

  GnssFix getFix() override { return fix_; }
  HeadingSample getHeading() override { return heading_; }

private:
  SensorConfig cfg_;
  unsigned long gnssPeriodMs_ = 200;
  unsigned long lastGnssEmitMs_ = 0;
  unsigned long lastGnssUpdateMs_ = 0;
  unsigned long lastHeadingMs_ = 0;
  float headingBiasDeg_ = 0.0f;
  GnssFix fix_;
  HeadingSample heading_;
};

struct SimMetrics {
  float p95ErrorM = 0.0f;
  float maxErrorM = 0.0f;
  float timeInDeadbandPct = 0.0f;
  float timeSaturatedPct = 0.0f;
  float dirChangesPerMin = 0.0f;
  uint32_t rampViolations = 0;
  float maxBadGnssInAnchorS = 0.0f;
  bool nanDetected = false;
  bool outOfRangeCommand = false;
  bool hardFailure = false;
  std::string hardFailureReason;
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

  HilScenarioRunner()
      : controller_(&sensors_, &sensors_, &actuator_, &clock_) {
    controller_.setEventSink(this);
  }

  void onControlEvent(unsigned long atMs,
                      const char* code,
                      const char* details) override {
    SimEvent ev;
    ev.atMs = atMs;
    ev.code = code ? code : "";
    ev.details = details ? details : "";
    if (events_.size() >= maxEvents_) {
      events_.erase(events_.begin());
    }
    events_.push_back(ev);
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
    controller_.setConfig(simctl::ControllerConfig());
    controller_.reset();
    controller_.requestAnchor(true);

    errors_.clear();
    events_.clear();
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
    return true;
  }

  bool isRunning() const { return state_ == State::RUNNING; }
  bool isDone() const { return done_; }
  State state() const { return state_; }
  float progressPct() const { return progressPct_; }
  const SimScenario& scenario() const { return scenario_; }
  const Result& result() const { return result_; }

  void abort() {
    if (state_ != State::RUNNING) {
      return;
    }
    done_ = true;
    state_ = State::ABORTED;
    result_.pass = false;
    result_.reason = "ABORTED";
    result_.events = events_;
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

    controller_.step(scenario_.anchorXM, scenario_.anchorYM);
    const simctl::ControllerTelemetry tele = controller_.telemetry();
    const ActuatorCmd cmd = actuator_.last();

    Vec2 current = scenario_.env.baseCurrentMps;
    for (const TimedCurrent& g : scenario_.env.gusts) {
      if (inWindow(simMs_, g.atMs, g.durationMs)) {
        current = g.currentMps;
      }
    }
    world_.step(dtS, cmd, current);

    const BoatWorldState& st = world_.state();
    const float errTrue = hypotf(st.xM - scenario_.anchorXM, st.yM - scenario_.anchorYM);
    if (!isfinite(errTrue)) {
      result_.metrics.hardFailure = true;
      result_.metrics.hardFailureReason = "ERR_NAN";
    }
    errors_.push_back(errTrue);
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
    if (fabsf(cmd.steerDeg) > 15.0f && cmd.thrust > 0.05f) {
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

    clock_.advance(dtMs);
    simMs_ += dtMs;
    sensors_.update(clock_.now_ms(), world_.state(), &rng_);

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
    char buf[256];
    snprintf(buf,
             sizeof(buf),
             "{\"id\":\"%s\",\"state\":\"%s\",\"progress_pct\":%.2f,"
             "\"sim_ms\":%lu,\"duration_ms\":%lu,\"dt_ms\":%lu}",
             scenario_.id.c_str(),
             stateStr(state_),
             progressPct_,
             (unsigned long)simMs_,
             (unsigned long)scenario_.durationMs,
             (unsigned long)lastAppliedDtMs_);
    return std::string(buf);
  }

  std::string reportJson() const {
    std::string out;
    char line[256];
    snprintf(line,
             sizeof(line),
             "{\"id\":\"%s\",\"state\":\"%s\",\"pass\":%s,\"reason\":\"%s\",",
             scenario_.id.c_str(),
             stateStr(state_),
             result_.pass ? "true" : "false",
             result_.reason.c_str());
    out += line;

    snprintf(line,
             sizeof(line),
             "\"metrics\":{\"p95_error_m\":%.3f,\"max_error_m\":%.3f,"
             "\"time_in_deadband_pct\":%.2f,\"time_saturated_pct\":%.2f,"
             "\"dir_changes_per_min\":%.2f,\"ramp_violations\":%lu,"
             "\"max_bad_gnss_in_anchor_s\":%.3f,\"nan_detected\":%s,"
             "\"out_of_range_command\":%s},\"events\":[",
             result_.metrics.p95ErrorM,
             result_.metrics.maxErrorM,
             result_.metrics.timeInDeadbandPct,
             result_.metrics.timeSaturatedPct,
             result_.metrics.dirChangesPerMin,
             (unsigned long)result_.metrics.rampViolations,
             result_.metrics.maxBadGnssInAnchorS,
             result_.metrics.nanDetected ? "true" : "false",
             result_.metrics.outOfRangeCommand ? "true" : "false");
    out += line;

    for (size_t i = 0; i < result_.events.size(); ++i) {
      const SimEvent& ev = result_.events[i];
      if (i > 0) {
        out += ",";
      }
      char evBuf[256];
      snprintf(evBuf,
               sizeof(evBuf),
               "{\"at_ms\":%lu,\"code\":\"%s\",\"details\":\"%s\"}",
               (unsigned long)ev.atMs,
               ev.code.c_str(),
               ev.details.c_str());
      out += evBuf;
    }
    out += "]}";
    return out;
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
  unsigned long effectiveDtMs(unsigned long nowMs, unsigned long defaultDtMs) const {
    for (const TimedLoopStall& s : scenario_.loopStalls) {
      if (inWindow(nowMs, s.atMs, s.durationMs)) {
        return s.dtMs;
      }
    }
    return defaultDtMs;
  }

  void finalize() {
    done_ = true;
    state_ = State::DONE;
    result_.events = events_;

    if (samples_ == 0) {
      result_.pass = false;
      result_.reason = "NO_SAMPLES";
      return;
    }

    std::vector<float> sorted = errors_;
    std::sort(sorted.begin(), sorted.end());
    const size_t p95Idx = (size_t)floorf((float)(sorted.size() - 1) * 0.95f);
    result_.metrics.p95ErrorM = sorted[p95Idx];
    result_.metrics.timeInDeadbandPct = 100.0f * ((float)deadbandCount_ / (float)samples_);
    result_.metrics.timeSaturatedPct = 100.0f * ((float)saturatedCount_ / (float)samples_);
    const float minutes = (scenario_.durationMs / 1000.0f) / 60.0f;
    result_.metrics.dirChangesPerMin = (minutes > 0.0f) ? (signChanges_ / minutes) : 0.0f;
    result_.metrics.maxBadGnssInAnchorS = maxBadGnssAnchorMs_ / 1000.0f;

    const ScenarioExpect& ex = scenario_.expect;
    bool pass = true;
    std::string reason = "PASS";

    if (result_.metrics.nanDetected) {
      pass = false;
      reason = "NAN_DETECTED";
    }
    if (pass && result_.metrics.outOfRangeCommand) {
      pass = false;
      reason = "COMMAND_OUT_OF_RANGE";
    }
    if (pass && ex.p95ErrorMaxM > 0.0f && result_.metrics.p95ErrorM > ex.p95ErrorMaxM) {
      pass = false;
      reason = "P95_ERROR";
    }
    if (pass && ex.maxErrorMaxM > 0.0f && result_.metrics.maxErrorM > ex.maxErrorMaxM) {
      pass = false;
      reason = "MAX_ERROR";
    }
    if (pass && ex.timeInDeadbandMinPct >= 0.0f &&
        result_.metrics.timeInDeadbandPct < ex.timeInDeadbandMinPct) {
      pass = false;
      reason = "DEADBAND_PCT";
    }
    if (pass && ex.timeSaturatedMaxPct >= 0.0f &&
        result_.metrics.timeSaturatedPct > ex.timeSaturatedMaxPct) {
      pass = false;
      reason = "SATURATED_PCT";
    }
    if (pass && ex.dirChangesPerMinMax >= 0.0f &&
        result_.metrics.dirChangesPerMin > ex.dirChangesPerMinMax) {
      pass = false;
      reason = "DIR_CHANGES";
    }
    if (pass && ex.maxBadGnssInAnchorMaxS >= 0.0f &&
        result_.metrics.maxBadGnssInAnchorS > ex.maxBadGnssInAnchorMaxS) {
      pass = false;
      reason = "BAD_GNSS_TIME";
    }
    if (pass) {
      for (const std::string& required : ex.requiredEvents) {
        bool found = false;
        for (const SimEvent& ev : result_.events) {
          if (ev.code == required || ev.details == required) {
            found = true;
            break;
          }
        }
        if (!found) {
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
  static constexpr size_t maxEvents_ = 500;

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

  std::vector<float> errors_;
  std::vector<SimEvent> events_;
  Result result_;

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

inline std::vector<SimScenario> defaultScenarios() {
  std::vector<SimScenario> v;

  SimScenario s0 = makeBaseScenario("S0_hold_still_good", 0.0f, 0.8f);
  s0.expect.p95ErrorMaxM = 2.5f;
  s0.expect.maxErrorMaxM = 5.0f;
  s0.expect.timeInDeadbandMinPct = 80.0f;
  s0.expect.dirChangesPerMinMax = 25.0f;
  v.push_back(s0);

  SimScenario s1 = makeBaseScenario("S1_current_0p4_good", 0.4f, 0.9f);
  s1.expect.p95ErrorMaxM = 3.5f;
  s1.expect.maxErrorMaxM = 8.0f;
  s1.expect.timeInDeadbandMinPct = 60.0f;
  v.push_back(s1);

  SimScenario s2 = makeBaseScenario("S2_current_0p8_hard", 0.8f, 1.1f);
  s2.expect.p95ErrorMaxM = 5.0f;
  s2.expect.maxErrorMaxM = 12.0f;
  s2.expect.timeInDeadbandMinPct = 45.0f;
  s2.expect.timeSaturatedMaxPct = 75.0f;
  v.push_back(s2);

  SimScenario s3 = makeBaseScenario("S3_gusts", 0.3f, 1.0f);
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

  SimScenario s4 = makeBaseScenario("S4_gnss_dropout_3s", 0.4f, 0.9f);
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

  SimScenario s5 = makeBaseScenario("S5_position_jump_12m_once", 0.4f, 0.9f);
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

  SimScenario s6 = makeBaseScenario("S6_hdop_degrade_then_recover", 0.4f, 1.0f);
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

  SimScenario s7 = makeBaseScenario("S7_sats_drop", 0.4f, 1.0f);
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

  SimScenario s8 = makeBaseScenario("S8_control_loop_stall", 0.4f, 0.9f);
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

  SimScenario s9 = makeBaseScenario("S9_nan_heading_injection", 0.4f, 0.9f);
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
      const int batch = 256;
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
