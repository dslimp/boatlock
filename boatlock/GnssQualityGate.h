#pragma once
#include <Arduino.h>
#include <math.h>

struct GnssQualityConfig {
  unsigned long maxGpsAgeMs = 2000;
  int minSats = 6;
  float maxHdop = 3.5f;
  float maxPositionJumpM = 20.0f;
  bool enableSpeedAccelSanity = false;
  float maxSpeedMps = 25.0f;
  float maxAccelMps2 = 6.0f;
  int requiredSentences = 0;
};

struct GnssQualitySample {
  bool fix = false;
  unsigned long ageMs = 0;
  int sats = 0;
  bool hasHdop = false;
  float hdop = NAN;
  bool jumpRejected = false;
  float jumpM = 0.0f;
  bool hasSpeed = false;
  float speedMps = 0.0f;
  bool hasAccel = false;
  float accelMps2 = 0.0f;
  bool hasSentences = false;
  int sentences = 0;
};

enum class GnssQualityFailReason {
  NONE = 0,
  NO_FIX,
  DATA_STALE,
  SATS_TOO_LOW,
  HDOP_MISSING,
  HDOP_TOO_HIGH,
  POSITION_JUMP,
  SPEED_INVALID,
  ACCEL_INVALID,
  SENTENCES_MISSING,
};

inline const char* gnssFailReasonString(GnssQualityFailReason reason) {
  switch (reason) {
    case GnssQualityFailReason::NONE: return "NONE";
    case GnssQualityFailReason::NO_FIX: return "GPS_NO_FIX";
    case GnssQualityFailReason::DATA_STALE: return "GPS_DATA_STALE";
    case GnssQualityFailReason::SATS_TOO_LOW: return "GPS_SATS_TOO_LOW";
    case GnssQualityFailReason::HDOP_MISSING: return "GPS_HDOP_MISSING";
    case GnssQualityFailReason::HDOP_TOO_HIGH: return "GPS_HDOP_TOO_HIGH";
    case GnssQualityFailReason::POSITION_JUMP: return "GPS_POSITION_JUMP";
    case GnssQualityFailReason::SPEED_INVALID: return "GPS_SPEED_INVALID";
    case GnssQualityFailReason::ACCEL_INVALID: return "GPS_ACCEL_INVALID";
    case GnssQualityFailReason::SENTENCES_MISSING: return "GPS_SENTENCES_MISSING";
    default: return "GPS_UNKNOWN";
  }
}

inline GnssQualityFailReason evaluateGnssQuality(const GnssQualityConfig& cfg,
                                                 const GnssQualitySample& sample) {
  if (!sample.fix) {
    return GnssQualityFailReason::NO_FIX;
  }
  if (cfg.maxGpsAgeMs == 0 || sample.ageMs > cfg.maxGpsAgeMs) {
    return GnssQualityFailReason::DATA_STALE;
  }
  if (cfg.minSats < 1 || sample.sats < cfg.minSats) {
    return GnssQualityFailReason::SATS_TOO_LOW;
  }
  if (!sample.hasHdop || !isfinite(sample.hdop) || sample.hdop <= 0.0f) {
    return GnssQualityFailReason::HDOP_MISSING;
  }
  if (!isfinite(cfg.maxHdop) || cfg.maxHdop <= 0.0f || sample.hdop > cfg.maxHdop) {
    return GnssQualityFailReason::HDOP_TOO_HIGH;
  }
  if (sample.jumpRejected ||
      !isfinite(cfg.maxPositionJumpM) ||
      cfg.maxPositionJumpM <= 0.0f ||
      !isfinite(sample.jumpM) ||
      sample.jumpM > cfg.maxPositionJumpM) {
    return GnssQualityFailReason::POSITION_JUMP;
  }
  if (cfg.enableSpeedAccelSanity) {
    if (!isfinite(cfg.maxSpeedMps) || cfg.maxSpeedMps <= 0.0f) {
      return GnssQualityFailReason::SPEED_INVALID;
    }
    if (sample.hasSpeed &&
        (!isfinite(sample.speedMps) || sample.speedMps < 0.0f ||
         sample.speedMps > cfg.maxSpeedMps)) {
      return GnssQualityFailReason::SPEED_INVALID;
    }
    if (!isfinite(cfg.maxAccelMps2) || cfg.maxAccelMps2 <= 0.0f) {
      return GnssQualityFailReason::ACCEL_INVALID;
    }
    if (sample.hasAccel &&
        (!isfinite(sample.accelMps2) || fabsf(sample.accelMps2) > cfg.maxAccelMps2)) {
      return GnssQualityFailReason::ACCEL_INVALID;
    }
  }
  if (cfg.requiredSentences < 0) {
    return GnssQualityFailReason::SENTENCES_MISSING;
  }
  if (cfg.requiredSentences > 0) {
    if (!sample.hasSentences || sample.sentences < cfg.requiredSentences) {
      return GnssQualityFailReason::SENTENCES_MISSING;
    }
  }
  return GnssQualityFailReason::NONE;
}
