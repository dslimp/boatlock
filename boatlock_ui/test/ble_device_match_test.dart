import 'package:boatlock_ui/ble/ble_device_match.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('isBoatLockAdvertisement matches advertised and platform names', () {
    expect(
      isBoatLockAdvertisement(
        advertisedName: 'BoatLock',
        platformName: '',
        serviceUuids: const [],
      ),
      isTrue,
    );
    expect(
      isBoatLockAdvertisement(
        advertisedName: '',
        platformName: 'my boatlock bench',
        serviceUuids: const [],
      ),
      isTrue,
    );
  });

  test('isBoatLockAdvertisement matches advertised service uuid', () {
    expect(
      isBoatLockAdvertisement(
        advertisedName: '',
        platformName: '',
        serviceUuids: const ['000012ab-0000-1000-8000-00805f9b34fb'],
      ),
      isTrue,
    );
  });

  test('isBoatLockAdvertisement rejects unrelated adverts', () {
    expect(
      isBoatLockAdvertisement(
        advertisedName: 'thermometer',
        platformName: 'sensor',
        serviceUuids: const ['180f'],
      ),
      isFalse,
    );
  });

  test('isBluetoothAdapterReady accepts only on state', () {
    expect(isBluetoothAdapterReady(BluetoothAdapterState.on), isTrue);
    expect(isBluetoothAdapterReady(BluetoothAdapterState.off), isFalse);
    expect(isBluetoothAdapterReady(BluetoothAdapterState.unknown), isFalse);
  });
}
