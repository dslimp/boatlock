#pragma once

#include <stdint.h>

namespace hilsim {

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

} // namespace hilsim
