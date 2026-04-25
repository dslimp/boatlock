import 'dart:convert';

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
    'lastDeviceLog': lastDeviceLog ?? '',
  };
}

String encodeSmokeResultLine(Map<String, dynamic> payload) {
  return '$kBoatLockSmokeResultPrefix${jsonEncode(payload)}';
}

String encodeSmokeStageLine(String stage) {
  return '$kBoatLockSmokeStagePrefix${jsonEncode(<String, String>{'stage': stage})}';
}
