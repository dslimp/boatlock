#pragma once
#include <cstdint>
#include <cstdlib>
#include <cmath>
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;

constexpr uint8_t HIGH = 0x1;
constexpr uint8_t LOW = 0x0;
constexpr uint8_t OUTPUT = 0x1;
constexpr uint8_t INPUT = 0x0;
constexpr uint8_t INPUT_PULLUP = 0x2;

inline unsigned long g_mockMillis = 0;
inline int g_lastLedcChannel = -1;
inline int g_lastLedcValue = -1;
inline int g_lastDigitalPin = -1;
inline int g_lastDigitalValue = -1;

inline void mockSetMillis(unsigned long value) { g_mockMillis = value; }
inline void mockAdvanceMillis(unsigned long delta) { g_mockMillis += delta; }
inline unsigned long millis() { return g_mockMillis; }

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
