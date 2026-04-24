#pragma once
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <cstddef>
#include <cstring>
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;

constexpr uint8_t HIGH = 0x1;
constexpr uint8_t LOW = 0x0;
constexpr uint8_t OUTPUT = 0x1;
constexpr uint8_t INPUT = 0x0;
constexpr uint8_t INPUT_PULLUP = 0x2;
constexpr uint32_t SERIAL_8N1 = 0x800001c;

inline unsigned long g_mockMillis = 0;
inline int g_lastLedcChannel = -1;
inline int g_lastLedcValue = -1;
inline int g_lastDigitalPin = -1;
inline int g_lastDigitalValue = -1;

inline void mockSetMillis(unsigned long value) { g_mockMillis = value; }
inline void mockAdvanceMillis(unsigned long delta) { g_mockMillis += delta; }
inline unsigned long millis() { return g_mockMillis; }
inline void delay(unsigned long ms) { g_mockMillis += ms; }

inline void pinMode(int, int) {}
inline void digitalWrite(int pin, int value) {
  g_lastDigitalPin = pin;
  g_lastDigitalValue = value;
}
inline void ledcSetup(int, int, int) {}
inline void ledcAttachPin(int, int) {}
inline void ledcWrite(int channel, int value) {
  g_lastLedcChannel = channel;
  g_lastLedcValue = value;
}

class HardwareSerial {
public:
  explicit HardwareSerial(int = 0) {}

  void begin(uint32_t baud, uint32_t = SERIAL_8N1, int rxPin = -1, int txPin = -1) {
    baud_ = baud;
    rxPin_ = rxPin;
    txPin_ = txPin;
    open_ = true;
  }

  void end() {
    open_ = false;
    pos_ = 0;
    len_ = 0;
  }

  int available() const {
    return open_ && len_ >= pos_ ? static_cast<int>(len_ - pos_) : 0;
  }

  int read() {
    if (available() <= 0) {
      return -1;
    }
    return buffer_[pos_++];
  }

  void inject(const uint8_t* data, size_t len) {
    len_ = len > sizeof(buffer_) ? sizeof(buffer_) : len;
    if (len_ > 0) {
      std::memcpy(buffer_, data, len_);
    }
    pos_ = 0;
  }

  bool open_ = false;
  uint32_t baud_ = 0;
  int rxPin_ = -1;
  int txPin_ = -1;

private:
  uint8_t buffer_[256] = {0};
  size_t pos_ = 0;
  size_t len_ = 0;
};

template <typename T>
inline T constrain(T x, T a, T b) {
  if (x < a) return a;
  if (x > b) return b;
  return x;
}

template <typename T>
inline T min(T a, T b) {
  return a < b ? a : b;
}

template <typename T>
inline T max(T a, T b) {
  return a > b ? a : b;
}
