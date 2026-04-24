#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

struct RuntimeBleLiveTelemetry {
  double lat = 0.0;
  double lon = 0.0;
  double anchorLat = 0.0;
  double anchorLon = 0.0;
  float anchorHeadingDeg = 0.0f;
  float distanceM = 0.0f;
  float headingDeg = 0.0f;
  uint8_t batteryPct = 0;
  bool holdHeading = false;
  uint16_t stepSpr = 4096;
  uint16_t stepMaxSpd = 0;
  uint16_t stepAccel = 0;
  float headingRawDeg = 0.0f;
  float compassOffsetDeg = 0.0f;
  uint8_t compassQ = 0;
  uint8_t magQ = 0;
  uint8_t gyroQ = 0;
  float rvAccDeg = 0.0f;
  float magNorm = 0.0f;
  float gyroNorm = 0.0f;
  float pitchDeg = 0.0f;
  float rollDeg = 0.0f;
  bool secPaired = false;
  bool secAuth = false;
  bool secPairWindowOpen = false;
  uint64_t secNonce = 0;
  std::string mode = "IDLE";
  std::string status = "OK";
  std::string statusReasons;
  std::string secReject = "NONE";
  uint8_t gnssQ = 0;
};

enum RuntimeBleLiveFlags : uint16_t {
  RUNTIME_BLE_FLAG_HOLD_HEADING = 1u << 0,
  RUNTIME_BLE_FLAG_SEC_PAIRED = 1u << 1,
  RUNTIME_BLE_FLAG_SEC_AUTH = 1u << 2,
  RUNTIME_BLE_FLAG_SEC_PAIR_WINDOW = 1u << 3,
};

enum RuntimeBleReasonFlags : uint32_t {
  RUNTIME_BLE_REASON_NO_GPS = 1ul << 0,
  RUNTIME_BLE_REASON_NO_COMPASS = 1ul << 1,
  RUNTIME_BLE_REASON_DRIFT_ALERT = 1ul << 2,
  RUNTIME_BLE_REASON_DRIFT_FAIL = 1ul << 3,
  RUNTIME_BLE_REASON_CONTAINMENT_BREACH = 1ul << 4,
  RUNTIME_BLE_REASON_GPS_WEAK = 1ul << 5,
  RUNTIME_BLE_REASON_COMM_TIMEOUT = 1ul << 6,
  RUNTIME_BLE_REASON_CONTROL_LOOP_TIMEOUT = 1ul << 7,
  RUNTIME_BLE_REASON_SENSOR_TIMEOUT = 1ul << 8,
  RUNTIME_BLE_REASON_INTERNAL_ERROR_NAN = 1ul << 9,
  RUNTIME_BLE_REASON_COMMAND_OUT_OF_RANGE = 1ul << 10,
  RUNTIME_BLE_REASON_STOP_CMD = 1ul << 11,
  RUNTIME_BLE_REASON_GPS_HDOP_TOO_HIGH = 1ul << 12,
  RUNTIME_BLE_REASON_GPS_SATS_TOO_LOW = 1ul << 13,
  RUNTIME_BLE_REASON_GPS_STALE = 1ul << 14,
  RUNTIME_BLE_REASON_GPS_JUMP = 1ul << 15,
  RUNTIME_BLE_REASON_NUDGE_OK = 1ul << 16,
  RUNTIME_BLE_REASON_NO_ANCHOR_POINT = 1ul << 17,
  RUNTIME_BLE_REASON_NO_HEADING = 1ul << 18,
};

inline bool runtimeBleCsvHasToken(const std::string& csv, const char* token) {
  size_t start = 0;
  while (start <= csv.size()) {
    size_t end = csv.find(',', start);
    if (end == std::string::npos) {
      end = csv.size();
    }
    size_t first = start;
    while (first < end && csv[first] == ' ') {
      ++first;
    }
    size_t last = end;
    while (last > first && csv[last - 1] == ' ') {
      --last;
    }
    if (csv.compare(first, last - first, token) == 0) {
      return true;
    }
    if (end == csv.size()) {
      break;
    }
    start = end + 1;
  }
  return false;
}

inline uint8_t runtimeBleModeCode(const std::string& mode) {
  if (mode == "IDLE") return 0;
  if (mode == "HOLD") return 1;
  if (mode == "ANCHOR") return 2;
  if (mode == "MANUAL") return 3;
  if (mode == "SIM") return 4;
  return 255;
}

inline uint8_t runtimeBleStatusCode(const std::string& status) {
  if (status == "OK") return 0;
  if (status == "WARN") return 1;
  if (status == "ALERT") return 2;
  return 255;
}

inline uint8_t runtimeBleRejectCode(const std::string& reject) {
  if (reject == "NONE") return 0;
  if (reject == "NOT_PAIRED") return 1;
  if (reject == "PAIRING_WINDOW_CLOSED") return 2;
  if (reject == "INVALID_OWNER_SECRET") return 3;
  if (reject == "AUTH_REQUIRED") return 4;
  if (reject == "AUTH_FAILED") return 5;
  if (reject == "BAD_FORMAT") return 6;
  if (reject == "BAD_SIGNATURE") return 7;
  if (reject == "REPLAY_DETECTED") return 8;
  if (reject == "RATE_LIMIT") return 9;
  return 255;
}

inline uint32_t runtimeBleReasonFlags(const std::string& reasons) {
  uint32_t flags = 0;
  if (runtimeBleCsvHasToken(reasons, "NO_GPS")) flags |= RUNTIME_BLE_REASON_NO_GPS;
  if (runtimeBleCsvHasToken(reasons, "NO_COMPASS")) flags |= RUNTIME_BLE_REASON_NO_COMPASS;
  if (runtimeBleCsvHasToken(reasons, "DRIFT_ALERT")) flags |= RUNTIME_BLE_REASON_DRIFT_ALERT;
  if (runtimeBleCsvHasToken(reasons, "DRIFT_FAIL")) flags |= RUNTIME_BLE_REASON_DRIFT_FAIL;
  if (runtimeBleCsvHasToken(reasons, "CONTAINMENT_BREACH")) flags |= RUNTIME_BLE_REASON_CONTAINMENT_BREACH;
  if (runtimeBleCsvHasToken(reasons, "GPS_WEAK")) flags |= RUNTIME_BLE_REASON_GPS_WEAK;
  if (runtimeBleCsvHasToken(reasons, "COMM_TIMEOUT")) flags |= RUNTIME_BLE_REASON_COMM_TIMEOUT;
  if (runtimeBleCsvHasToken(reasons, "CONTROL_LOOP_TIMEOUT")) flags |= RUNTIME_BLE_REASON_CONTROL_LOOP_TIMEOUT;
  if (runtimeBleCsvHasToken(reasons, "SENSOR_TIMEOUT")) flags |= RUNTIME_BLE_REASON_SENSOR_TIMEOUT;
  if (runtimeBleCsvHasToken(reasons, "INTERNAL_ERROR_NAN")) flags |= RUNTIME_BLE_REASON_INTERNAL_ERROR_NAN;
  if (runtimeBleCsvHasToken(reasons, "COMMAND_OUT_OF_RANGE")) flags |= RUNTIME_BLE_REASON_COMMAND_OUT_OF_RANGE;
  if (runtimeBleCsvHasToken(reasons, "STOP_CMD")) flags |= RUNTIME_BLE_REASON_STOP_CMD;
  if (runtimeBleCsvHasToken(reasons, "GPS_HDOP_TOO_HIGH")) flags |= RUNTIME_BLE_REASON_GPS_HDOP_TOO_HIGH;
  if (runtimeBleCsvHasToken(reasons, "GPS_SATS_TOO_LOW")) flags |= RUNTIME_BLE_REASON_GPS_SATS_TOO_LOW;
  if (runtimeBleCsvHasToken(reasons, "GPS_STALE")) flags |= RUNTIME_BLE_REASON_GPS_STALE;
  if (runtimeBleCsvHasToken(reasons, "GPS_JUMP")) flags |= RUNTIME_BLE_REASON_GPS_JUMP;
  if (runtimeBleCsvHasToken(reasons, "NUDGE_OK")) flags |= RUNTIME_BLE_REASON_NUDGE_OK;
  if (runtimeBleCsvHasToken(reasons, "NO_ANCHOR_POINT")) flags |= RUNTIME_BLE_REASON_NO_ANCHOR_POINT;
  if (runtimeBleCsvHasToken(reasons, "NO_HEADING")) flags |= RUNTIME_BLE_REASON_NO_HEADING;
  return flags;
}

inline uint16_t runtimeBleFlags(const RuntimeBleLiveTelemetry& telemetry) {
  uint16_t flags = 0;
  if (telemetry.holdHeading) flags |= RUNTIME_BLE_FLAG_HOLD_HEADING;
  if (telemetry.secPaired) flags |= RUNTIME_BLE_FLAG_SEC_PAIRED;
  if (telemetry.secAuth) flags |= RUNTIME_BLE_FLAG_SEC_AUTH;
  if (telemetry.secPairWindowOpen) flags |= RUNTIME_BLE_FLAG_SEC_PAIR_WINDOW;
  return flags;
}

template <typename T>
inline T runtimeBleClampValue(T value, T minValue, T maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

inline int32_t runtimeBleScaleSigned(double value, double scale,
                                     int32_t minValue, int32_t maxValue) {
  if (!std::isfinite(value)) {
    return 0;
  }
  const long scaled = lround(value * scale);
  return runtimeBleClampValue<int32_t>((int32_t)scaled, minValue, maxValue);
}

inline uint32_t runtimeBleScaleUnsigned(double value, double scale,
                                        uint32_t maxValue) {
  if (!std::isfinite(value) || value <= 0) {
    return 0;
  }
  const unsigned long scaled = (unsigned long)lround(value * scale);
  return runtimeBleClampValue<uint32_t>((uint32_t)scaled, 0, maxValue);
}

inline void runtimeBleAppendU8(std::vector<uint8_t>& out, uint8_t value) {
  out.push_back(value);
}

inline void runtimeBleAppendU16(std::vector<uint8_t>& out, uint16_t value) {
  out.push_back((uint8_t)(value & 0xFF));
  out.push_back((uint8_t)((value >> 8) & 0xFF));
}

inline void runtimeBleAppendI16(std::vector<uint8_t>& out, int16_t value) {
  runtimeBleAppendU16(out, (uint16_t)value);
}

inline void runtimeBleAppendU32(std::vector<uint8_t>& out, uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    out.push_back((uint8_t)((value >> (i * 8)) & 0xFF));
  }
}

inline void runtimeBleAppendI32(std::vector<uint8_t>& out, int32_t value) {
  runtimeBleAppendU32(out, (uint32_t)value);
}

inline void runtimeBleAppendU64(std::vector<uint8_t>& out, uint64_t value) {
  for (int i = 0; i < 8; ++i) {
    out.push_back((uint8_t)((value >> (i * 8)) & 0xFF));
  }
}

inline std::vector<uint8_t> runtimeBleEncodeLiveFrame(
    const RuntimeBleLiveTelemetry& telemetry,
    uint16_t sequence) {
  std::vector<uint8_t> out;
  out.reserve(70);
  runtimeBleAppendU8(out, 'B');
  runtimeBleAppendU8(out, 'L');
  runtimeBleAppendU8(out, 2);
  runtimeBleAppendU8(out, 1);
  runtimeBleAppendU16(out, sequence);
  runtimeBleAppendU16(out, runtimeBleFlags(telemetry));
  runtimeBleAppendU8(out, runtimeBleModeCode(telemetry.mode));
  runtimeBleAppendU8(out, runtimeBleStatusCode(telemetry.status));
  runtimeBleAppendI32(out, runtimeBleScaleSigned(telemetry.lat, 10000000.0, INT32_MIN, INT32_MAX));
  runtimeBleAppendI32(out, runtimeBleScaleSigned(telemetry.lon, 10000000.0, INT32_MIN, INT32_MAX));
  runtimeBleAppendI32(out, runtimeBleScaleSigned(telemetry.anchorLat, 10000000.0, INT32_MIN, INT32_MAX));
  runtimeBleAppendI32(out, runtimeBleScaleSigned(telemetry.anchorLon, 10000000.0, INT32_MIN, INT32_MAX));
  runtimeBleAppendU16(out, (uint16_t)runtimeBleScaleUnsigned(telemetry.anchorHeadingDeg, 10.0, 3599));
  runtimeBleAppendU16(out, (uint16_t)runtimeBleScaleUnsigned(telemetry.distanceM, 100.0, UINT16_MAX));
  runtimeBleAppendU16(out, (uint16_t)runtimeBleScaleUnsigned(telemetry.headingDeg, 10.0, 3599));
  runtimeBleAppendU8(out, runtimeBleClampValue<uint8_t>(telemetry.batteryPct, 0, 100));
  runtimeBleAppendU16(out, telemetry.stepSpr);
  runtimeBleAppendU16(out, telemetry.stepMaxSpd);
  runtimeBleAppendU16(out, telemetry.stepAccel);
  runtimeBleAppendU16(out, (uint16_t)runtimeBleScaleUnsigned(telemetry.headingRawDeg, 10.0, 3599));
  runtimeBleAppendI16(out, (int16_t)runtimeBleScaleSigned(telemetry.compassOffsetDeg, 10.0, INT16_MIN, INT16_MAX));
  runtimeBleAppendU8(out, telemetry.compassQ);
  runtimeBleAppendU8(out, telemetry.magQ);
  runtimeBleAppendU8(out, telemetry.gyroQ);
  runtimeBleAppendU16(out, (uint16_t)runtimeBleScaleUnsigned(telemetry.rvAccDeg, 100.0, UINT16_MAX));
  runtimeBleAppendU16(out, (uint16_t)runtimeBleScaleUnsigned(telemetry.magNorm, 100.0, UINT16_MAX));
  runtimeBleAppendU16(out, (uint16_t)runtimeBleScaleUnsigned(telemetry.gyroNorm, 100.0, UINT16_MAX));
  runtimeBleAppendI16(out, (int16_t)runtimeBleScaleSigned(telemetry.pitchDeg, 10.0, INT16_MIN, INT16_MAX));
  runtimeBleAppendI16(out, (int16_t)runtimeBleScaleSigned(telemetry.rollDeg, 10.0, INT16_MIN, INT16_MAX));
  runtimeBleAppendU8(out, runtimeBleRejectCode(telemetry.secReject));
  runtimeBleAppendU32(out, runtimeBleReasonFlags(telemetry.statusReasons));
  runtimeBleAppendU64(out, telemetry.secNonce);
  runtimeBleAppendU8(out, telemetry.gnssQ);
  return out;
}
