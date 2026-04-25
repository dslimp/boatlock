#pragma once

#include "ControlInterfaces.h"

namespace hilsim {

class ActuatorCapture : public IActuator {
public:
  void apply(const ActuatorCmd& cmd) override { lastCmd_ = cmd; }
  const ActuatorCmd& last() const { return lastCmd_; }

private:
  ActuatorCmd lastCmd_;
};

} // namespace hilsim
