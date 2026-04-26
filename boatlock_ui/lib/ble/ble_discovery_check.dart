import 'ble_ids.dart';

bool boatLockDiscoveryComplete({
  required bool dataFound,
  required bool commandFound,
  required bool logFound,
  bool otaFound = true,
}) {
  return dataFound && commandFound && logFound && otaFound;
}

String describeMissingBoatLockCharacteristics({
  required bool dataFound,
  required bool commandFound,
  required bool logFound,
  bool otaFound = true,
}) {
  final missing = <String>[];
  if (!dataFound) {
    missing.add('data:$boatLockDataCharacteristicUuid');
  }
  if (!commandFound) {
    missing.add('cmd:$boatLockCommandCharacteristicUuid');
  }
  if (!logFound) {
    missing.add('log:$boatLockLogCharacteristicUuid');
  }
  if (!otaFound) {
    missing.add('ota:$boatLockOtaCharacteristicUuid');
  }
  return missing.isEmpty ? 'none' : missing.join(',');
}
