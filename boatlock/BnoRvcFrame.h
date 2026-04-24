#pragma once

#include <stddef.h>
#include <stdint.h>

struct BnoRvcSample {
  uint8_t index = 0;
  float yawDeg = 0.0f;
  float pitchDeg = 0.0f;
  float rollDeg = 0.0f;
  float accelXMps2 = 0.0f;
  float accelYMps2 = 0.0f;
  float accelZMps2 = 0.0f;
};

inline int16_t bnoRvcReadInt16Le(const uint8_t* data) {
  return static_cast<int16_t>(static_cast<uint16_t>(data[0]) |
                              (static_cast<uint16_t>(data[1]) << 8));
}

inline bool bnoRvcParseFrame(const uint8_t* frame, size_t len, BnoRvcSample* out) {
  if (!frame || len != 17 || !out) {
    return false;
  }

  uint8_t checksum = 0;
  for (size_t i = 0; i < 16; ++i) {
    checksum = static_cast<uint8_t>(checksum + frame[i]);
  }
  if (checksum != frame[16]) {
    return false;
  }

  out->index = frame[0];
  out->yawDeg = bnoRvcReadInt16Le(frame + 1) * 0.01f;
  out->pitchDeg = bnoRvcReadInt16Le(frame + 3) * 0.01f;
  out->rollDeg = bnoRvcReadInt16Le(frame + 5) * 0.01f;
  out->accelXMps2 = bnoRvcReadInt16Le(frame + 7) * 0.0098067f;
  out->accelYMps2 = bnoRvcReadInt16Le(frame + 9) * 0.0098067f;
  out->accelZMps2 = bnoRvcReadInt16Le(frame + 11) * 0.0098067f;
  return true;
}

class BnoRvcStreamParser {
public:
  bool push(uint8_t byte, BnoRvcSample* out) {
    if (state_ == State::WAIT_FIRST_SYNC) {
      if (byte == 0xAA) {
        state_ = State::WAIT_SECOND_SYNC;
      }
      return false;
    }

    if (state_ == State::WAIT_SECOND_SYNC) {
      if (byte == 0xAA) {
        framePos_ = 0;
        state_ = State::READ_FRAME;
      } else {
        state_ = State::WAIT_FIRST_SYNC;
      }
      return false;
    }

    frame_[framePos_++] = byte;
    if (framePos_ < sizeof(frame_)) {
      return false;
    }

    state_ = State::WAIT_FIRST_SYNC;
    framePos_ = 0;
    return bnoRvcParseFrame(frame_, sizeof(frame_), out);
  }

  void reset() {
    state_ = State::WAIT_FIRST_SYNC;
    framePos_ = 0;
  }

private:
  enum class State {
    WAIT_FIRST_SYNC,
    WAIT_SECOND_SYNC,
    READ_FRAME,
  };

  State state_ = State::WAIT_FIRST_SYNC;
  uint8_t frame_[17] = {0};
  size_t framePos_ = 0;
};
