#pragma once
#include <cstdint>
#include <cstring>

class EEPROMClass {
public:
  static const int kSize = 4096;
  uint8_t data[kSize] = {0};

  template <typename T> void put(int addr, const T &value) {
    std::memcpy(data + addr, &value, sizeof(T));
  }

  template <typename T> void get(int addr, T &value) {
    std::memcpy(&value, data + addr, sizeof(T));
  }

  void commit() {}
};

inline EEPROMClass EEPROM;
