#pragma once
#include <cstdint>
#include <cstring>

class EEPROMClass {
public:
  static const int kSize = 4096;
  uint8_t data[kSize] = {0};
  int commitCount = 0;
  bool commitResult = true;

  template <typename T> void put(int addr, const T &value) {
    std::memcpy(data + addr, &value, sizeof(T));
  }

  template <typename T> void get(int addr, T &value) {
    std::memcpy(&value, data + addr, sizeof(T));
  }

  bool commit() {
    commitCount++;
    return commitResult;
  }

  void clear() {
    std::memset(data, 0, sizeof(data));
    commitCount = 0;
    commitResult = true;
  }
};

inline EEPROMClass EEPROM;
