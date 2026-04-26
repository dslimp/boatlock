import 'dart:convert';

import '../ble/ble_command_rejection.dart';
import '../models/boat_data.dart';

const kBoatLockSmokeResultPrefix = 'BOATLOCK_SMOKE_RESULT ';
const kBoatLockSmokeStagePrefix = 'BOATLOCK_SMOKE_STAGE ';
const Set<String> kBoatLockSmokeModes = {
  'IDLE',
  'HOLD',
  'ANCHOR',
  'MANUAL',
  'SIM',
};
const Set<String> kBoatLockSmokeStatuses = {'OK', 'WARN', 'ALERT'};

bool smokeTelemetryLooksHealthy(BoatData? data) {
  if (data == null) {
    return false;
  }
  return kBoatLockSmokeModes.contains(data.mode.trim()) &&
      kBoatLockSmokeStatuses.contains(data.status.trim());
}

bool smokeStatusHasReason(BoatData data, String reason) {
  return data.statusReasons
      .split(',')
      .map((value) => value.trim())
      .where((value) => value.isNotEmpty)
      .contains(reason);
}

bool smokeStatusStopAlertSeen(BoatData data) {
  return data.status == 'ALERT' && smokeStatusHasReason(data, 'STOP_CMD');
}

bool smokeStatusRecoveredAfterStop(BoatData data) {
  return data.mode != 'MANUAL' && data.status != 'ALERT';
}

bool smokeAnchorDeniedLogSeen(String? line) {
  return line != null && line.contains('[EVENT] ANCHOR_DENIED');
}

bool smokeAnchorRejectedSafely(BoatData data) {
  return data.mode != 'ANCHOR';
}

bool smokeCompassCalStartLogSeen(String? line) {
  return line != null && line.contains('[EVENT] COMPASS_CAL_START');
}

bool smokeCompassDcdAutosaveLogSeen(String? line) {
  return line != null && line.contains('[EVENT] COMPASS_DCD_AUTOSAVE');
}

bool smokeCompassDcdSaveLogSeen(String? line) {
  return line != null && line.contains('[EVENT] COMPASS_DCD_SAVE');
}

BleCommandRejection? smokeProfileRejection(String? line) {
  if (line == null) return null;
  return parseBleCommandRejectionLog(line);
}

bool smokeCommandRejectedByProfile(
  String? line, {
  required String commandPrefix,
  String? profile,
  String? scope,
}) {
  final rejection = smokeProfileRejection(line);
  if (rejection == null) return false;
  if (!rejection.matchesCommandPrefix(commandPrefix)) return false;
  if (profile != null && rejection.profile != profile) return false;
  if (scope != null && rejection.scope != scope) return false;
  return true;
}

bool smokeGpsFixLooksHealthy(BoatData data) {
  if (data.gnssQ <= 0) {
    return false;
  }
  if (!data.lat.isFinite ||
      !data.lon.isFinite ||
      data.lat < -90.0 ||
      data.lat > 90.0 ||
      data.lon < -180.0 ||
      data.lon > 180.0) {
    return false;
  }
  if (data.lat.abs() < 0.000001 && data.lon.abs() < 0.000001) {
    return false;
  }
  return !smokeStatusHasReason(data, 'NO_GPS') &&
      !smokeStatusHasReason(data, 'GPS_DATA_STALE') &&
      !smokeStatusHasReason(data, 'GPS_HDOP_MISSING');
}

Map<String, dynamic> buildSmokeResultPayload({
  required bool pass,
  required String reason,
  required int dataEvents,
  required int deviceLogEvents,
  BoatData? data,
  String? lastDeviceLog,
}) {
  return <String, dynamic>{
    'pass': pass,
    'reason': reason,
    'dataEvents': dataEvents,
    'deviceLogEvents': deviceLogEvents,
    'mode': data?.mode ?? '',
    'status': data?.status ?? '',
    'statusReasons': data?.statusReasons ?? '',
    'secPaired': data?.secPaired ?? false,
    'secAuth': data?.secAuth ?? false,
    'rssi': data?.rssi ?? 0,
    'lat': data?.lat ?? 0.0,
    'lon': data?.lon ?? 0.0,
    'gnssQ': data?.gnssQ ?? 0,
    'lastDeviceLog': lastDeviceLog ?? '',
  };
}

String encodeSmokeResultLine(Map<String, dynamic> payload) {
  return '$kBoatLockSmokeResultPrefix${jsonEncode(payload)}';
}

String encodeSmokeStageLine(String stage) {
  return '$kBoatLockSmokeStagePrefix${jsonEncode(<String, String>{'stage': stage})}';
}
