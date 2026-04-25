const String boatLockDeviceName = 'BoatLock';
const String boatLockDeviceNameLower = 'boatlock';
const String boatLockServiceUuid = '12ab';
const String boatLockDataCharacteristicUuid = '34cd';
const String boatLockCommandCharacteristicUuid = '56ef';
const String boatLockLogCharacteristicUuid = '78ab';
const String boatLockBluetoothBaseUuidSuffix = '-0000-1000-8000-00805f9b34fb';

String boatLockFullUuid(String shortUuid) {
  return '0000${shortUuid.toLowerCase()}$boatLockBluetoothBaseUuidSuffix';
}

bool isBoatLockUuid(String value, String shortUuid) {
  final normalized = value.trim().toLowerCase();
  final normalizedShort = shortUuid.toLowerCase();
  return normalized == normalizedShort ||
      normalized == boatLockFullUuid(normalizedShort);
}
