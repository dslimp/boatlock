#pragma once

#include <math.h>
#include <stdint.h>
#include <stddef.h>

#include <array>

namespace hilsim {

class SimErrorHistogram {
public:
  void clear() {
    bins_.fill(0);
    samples_ = 0;
  }

  void add(float errTrueM) {
    float err = errTrueM;
    if (!isfinite(err) || err < 0.0f) {
      err = kMaxM;
    }
    size_t idx = (size_t)floorf(err / kBinM);
    if (idx >= kBins) {
      idx = kBins - 1;
    }
    bins_[idx]++;
    samples_++;
  }

  uint32_t samples() const { return samples_; }

  float p95(float fallbackM = 0.0f) const {
    if (samples_ == 0) {
      return 0.0f;
    }
    const uint32_t target = (uint32_t)ceilf(0.95f * (float)samples_);
    uint32_t acc = 0;
    for (size_t i = 0; i < kBins; ++i) {
      acc += bins_[i];
      if (acc >= target) {
        return ((float)i + 0.5f) * kBinM;
      }
    }
    return fallbackM;
  }

private:
  static constexpr float kBinM = 0.1f;
  static constexpr float kMaxM = 64.0f;
  static constexpr size_t kBins = (size_t)(kMaxM / kBinM) + 1;

  std::array<uint32_t, kBins> bins_{};
  uint32_t samples_ = 0;
};

} // namespace hilsim
