#pragma once

#include <stdint.h>

struct GnssFix {
  bool valid = false;
  float xM = 0.0f;
  float yM = 0.0f;
  int sats = 0;
  float hdop = 99.0f;
  unsigned long ageMs = 999999UL;
};

struct HeadingSample {
  bool valid = false;
  float headingDeg = 0.0f;
  unsigned long ageMs = 999999UL;
};

struct ActuatorCmd {
  float thrust = 0.0f;   // normalized 0..1
  float steerDeg = 0.0f; // -180..180, relative to boat nose
  bool stop = true;
};

class IGnssSource {
public:
  virtual ~IGnssSource() {}
  virtual GnssFix getFix() = 0;
};

class IHeadingSource {
public:
  virtual ~IHeadingSource() {}
  virtual HeadingSample getHeading() = 0;
};

class IActuator {
public:
  virtual ~IActuator() {}
  virtual void apply(const ActuatorCmd& cmd) = 0;
};

class IClock {
public:
  virtual ~IClock() {}
  virtual unsigned long now_ms() const = 0;
};

