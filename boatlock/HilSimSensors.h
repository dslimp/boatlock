#pragma once

#include <math.h>

#include <vector>

#include "ControlInterfaces.h"
#include "HilSimRandom.h"
#include "HilSimTime.h"
#include "HilSimWorld.h"

namespace hilsim {

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

struct TimedNanHeading {
  unsigned long atMs = 0;
  unsigned long durationMs = 50;
};

struct TimedHeadingDropout {
  unsigned long atMs = 0;
  unsigned long durationMs = 0;
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
  std::vector<TimedHeadingDropout> headingDropouts;
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
    lastHeadingMs_ = 0;
    headingBiasDeg_ = 0.0f;
    fix_ = {};
    fix_.ageMs = 999999UL;
    heading_ = {};
    heading_.ageMs = 999999UL;
  }

  void update(unsigned long nowMs, const BoatWorldState& st, XorShift32* rng) {
    heading_.valid = true;
    headingBiasDeg_ += cfg_.headingDriftDegS * 0.001f * (float)(nowMs - lastHeadingMs_);
    lastHeadingMs_ = nowMs;
    heading_.headingDeg =
        simctl::wrap360f(st.psiDeg + headingBiasDeg_ + rng->gauss(cfg_.headingSigmaDeg));
    heading_.ageMs = 0;
    for (const TimedNanHeading& inj : cfg_.nanHeading) {
      if (simTimeWindowContains(nowMs, inj.atMs, inj.durationMs)) {
        heading_.headingDeg = NAN;
        heading_.valid = true;
        break;
      }
    }
    for (const TimedHeadingDropout& inj : cfg_.headingDropouts) {
      if (simTimeWindowContains(nowMs, inj.atMs, inj.durationMs)) {
        heading_.valid = false;
        heading_.headingDeg = 0.0f;
        heading_.ageMs = 999999UL;
        break;
      }
    }

    if (nowMs - lastGnssEmitMs_ < gnssPeriodMs_) {
      fix_.ageMs = nowMs - lastGnssUpdateMs_;
      if (heading_.valid) {
        heading_.ageMs = 0;
      }
      return;
    }
    lastGnssEmitMs_ = nowMs;

    bool dropped = false;
    for (const TimedDropout& d : cfg_.dropouts) {
      if (simTimeWindowContains(nowMs, d.atMs, d.durationMs)) {
        dropped = true;
        break;
      }
    }

    if (dropped) {
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
      if (simTimeWindowContains(nowMs, j.atMs, j.durationMs)) {
        x += j.jumpM.x;
        y += j.jumpM.y;
      }
    }

    float hdop = cfg_.hdop;
    for (const TimedHdop& h : cfg_.hdopChanges) {
      if (simTimeWindowContains(nowMs, h.atMs, h.durationMs)) {
        hdop = h.hdop;
      }
    }

    int sats = cfg_.sats;
    for (const TimedSats& s : cfg_.satsChanges) {
      if (simTimeWindowContains(nowMs, s.atMs, s.durationMs)) {
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

} // namespace hilsim
