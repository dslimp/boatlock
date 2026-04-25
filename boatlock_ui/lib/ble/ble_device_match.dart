import 'package:flutter_blue_plus/flutter_blue_plus.dart';

bool isBluetoothAdapterReady(BluetoothAdapterState state) {
  return state == BluetoothAdapterState.on;
}

bool isBoatLockAdvertisement({
  required String advertisedName,
  required String platformName,
  required Iterable<String> serviceUuids,
}) {
  final advName = advertisedName.trim().toLowerCase();
  final devName = platformName.trim().toLowerCase();
  final nameMatch =
      advName == 'boatlock' ||
      devName == 'boatlock' ||
      advName.contains('boatlock') ||
      devName.contains('boatlock');

  final serviceMatch = serviceUuids.any(
    (uuid) => uuid.toLowerCase().contains('12ab'),
  );

  return nameMatch || serviceMatch;
}

bool isBoatLockScanResult(ScanResult result) {
  return isBoatLockAdvertisement(
    advertisedName: result.advertisementData.advName,
    platformName: result.device.platformName,
    serviceUuids: result.advertisementData.serviceUuids.map(
      (uuid) => uuid.toString(),
    ),
  );
}
