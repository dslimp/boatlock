#pragma once

#include "GnssQualityGate.h"

enum class AnchorDeniedReason : uint8_t {
  NONE = 0,
  NO_ANCHOR_POINT,
  NO_HEADING,
  GPS_NO_FIX,
  GPS_DATA_STALE,
  GPS_SATS_TOO_LOW,
  GPS_HDOP_TOO_HIGH,
  GPS_POSITION_JUMP,
  GPS_SPEED_INVALID,
  GPS_ACCEL_INVALID,
  GPS_SENTENCES_MISSING,
  INTERNAL_ERROR,
};

enum class FailsafeReason : uint8_t {
  NONE = 0,
  STOP_CMD,
  GPS_WEAK,
  CONTAINMENT_BREACH,
  COMM_TIMEOUT,
  CONTROL_LOOP_TIMEOUT,
  SENSOR_TIMEOUT,
  INTERNAL_ERROR_NAN,
  COMMAND_OUT_OF_RANGE,
};

inline const char* anchorDeniedReasonString(AnchorDeniedReason reason) {
  switch (reason) {
    case AnchorDeniedReason::NO_ANCHOR_POINT:
      return "NO_ANCHOR_POINT";
    case AnchorDeniedReason::NO_HEADING:
      return "NO_HEADING";
    case AnchorDeniedReason::GPS_NO_FIX:
      return "GPS_NO_FIX";
    case AnchorDeniedReason::GPS_DATA_STALE:
      return "GPS_DATA_STALE";
    case AnchorDeniedReason::GPS_SATS_TOO_LOW:
      return "GPS_SATS_TOO_LOW";
    case AnchorDeniedReason::GPS_HDOP_TOO_HIGH:
      return "GPS_HDOP_TOO_HIGH";
    case AnchorDeniedReason::GPS_POSITION_JUMP:
      return "GPS_POSITION_JUMP";
    case AnchorDeniedReason::GPS_SPEED_INVALID:
      return "GPS_SPEED_INVALID";
    case AnchorDeniedReason::GPS_ACCEL_INVALID:
      return "GPS_ACCEL_INVALID";
    case AnchorDeniedReason::GPS_SENTENCES_MISSING:
      return "GPS_SENTENCES_MISSING";
    case AnchorDeniedReason::INTERNAL_ERROR:
      return "INTERNAL_ERROR";
    case AnchorDeniedReason::NONE:
    default:
      return "NONE";
  }
}

inline const char* failsafeReasonString(FailsafeReason reason) {
  switch (reason) {
    case FailsafeReason::STOP_CMD:
      return "STOP_CMD";
    case FailsafeReason::GPS_WEAK:
      return "GPS_WEAK";
    case FailsafeReason::CONTAINMENT_BREACH:
      return "CONTAINMENT_BREACH";
    case FailsafeReason::COMM_TIMEOUT:
      return "COMM_TIMEOUT";
    case FailsafeReason::CONTROL_LOOP_TIMEOUT:
      return "CONTROL_LOOP_TIMEOUT";
    case FailsafeReason::SENSOR_TIMEOUT:
      return "SENSOR_TIMEOUT";
    case FailsafeReason::INTERNAL_ERROR_NAN:
      return "INTERNAL_ERROR_NAN";
    case FailsafeReason::COMMAND_OUT_OF_RANGE:
      return "COMMAND_OUT_OF_RANGE";
    case FailsafeReason::NONE:
    default:
      return "NONE";
  }
}

inline AnchorDeniedReason deniedReasonFromGnss(GnssQualityFailReason reason) {
  switch (reason) {
    case GnssQualityFailReason::NO_FIX:
      return AnchorDeniedReason::GPS_NO_FIX;
    case GnssQualityFailReason::DATA_STALE:
      return AnchorDeniedReason::GPS_DATA_STALE;
    case GnssQualityFailReason::SATS_TOO_LOW:
      return AnchorDeniedReason::GPS_SATS_TOO_LOW;
    case GnssQualityFailReason::HDOP_TOO_HIGH:
      return AnchorDeniedReason::GPS_HDOP_TOO_HIGH;
    case GnssQualityFailReason::POSITION_JUMP:
      return AnchorDeniedReason::GPS_POSITION_JUMP;
    case GnssQualityFailReason::SPEED_INVALID:
      return AnchorDeniedReason::GPS_SPEED_INVALID;
    case GnssQualityFailReason::ACCEL_INVALID:
      return AnchorDeniedReason::GPS_ACCEL_INVALID;
    case GnssQualityFailReason::SENTENCES_MISSING:
      return AnchorDeniedReason::GPS_SENTENCES_MISSING;
    case GnssQualityFailReason::NONE:
    default:
      return AnchorDeniedReason::NONE;
  }
}
