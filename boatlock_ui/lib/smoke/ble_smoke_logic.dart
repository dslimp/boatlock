import 'dart:convert';

import '../models/boat_data.dart';

const kBoatLockSmokeResultPrefix = 'BOATLOCK_SMOKE_RESULT ';
const kBoatLockSmokeStagePrefix = 'BOATLOCK_SMOKE_STAGE ';

bool smokeTelemetryLooksHealthy(BoatData? data) {
  if (data == null) {
    return false;
  }
  return data.mode.trim().isNotEmpty && data.status.trim().isNotEmpty;
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
