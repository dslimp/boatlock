import 'ble_ids.dart';

bool boatLockDiscoveryComplete({
  required bool dataFound,
  required bool commandFound,
  required bool logFound,
}) {
  return dataFound && commandFound && logFound;
}

String describeMissingBoatLockCharacteristics({
  required bool dataFound,
  required bool commandFound,
  required bool logFound,
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
  return missing.isEmpty ? 'none' : missing.join(',');
}
