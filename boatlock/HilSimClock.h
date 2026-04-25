#pragma once

#include "ControlInterfaces.h"

namespace hilsim {

class VirtualClock : public IClock {
public:
  unsigned long now_ms() const override { return nowMs_; }
  void set(unsigned long ms) { nowMs_ = ms; }
  void advance(unsigned long ms) { nowMs_ += ms; }

private:
  unsigned long nowMs_ = 0;
};

} // namespace hilsim
