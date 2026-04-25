#pragma once

#include <math.h>
#include <algorithm>

#include "AnchorControlLoop.h"

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

} // namespace hilsim
