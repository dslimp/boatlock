import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'ble_ids.dart';

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
      advName == boatLockDeviceNameLower ||
      devName == boatLockDeviceNameLower ||
      advName.contains(boatLockDeviceNameLower) ||
      devName.contains(boatLockDeviceNameLower);

  final serviceMatch = serviceUuids.any(
    (uuid) => uuid.toLowerCase().contains(boatLockServiceUuid),
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
