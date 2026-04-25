import 'package:boatlock_ui/ble/ble_boatlock.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('adapter ready helper accepts only on state', () {
    expect(BleBoatLock.isAdapterReady(BluetoothAdapterState.on), isTrue);
    expect(BleBoatLock.isAdapterReady(BluetoothAdapterState.off), isFalse);
    expect(BleBoatLock.isAdapterReady(BluetoothAdapterState.unknown), isFalse);
  });
}
