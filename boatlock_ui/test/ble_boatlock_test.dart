import 'package:boatlock_ui/ble/ble_boatlock.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('adapter ready helper accepts only on state', () {
    expect(BleBoatLock.isAdapterReady(BluetoothAdapterState.on), isTrue);
    expect(BleBoatLock.isAdapterReady(BluetoothAdapterState.off), isFalse);
    expect(BleBoatLock.isAdapterReady(BluetoothAdapterState.unknown), isFalse);
  });

  test('decodeLogLine strips trailing nul padding', () {
    expect(
      BleBoatLock.decodeLogLine(<int>[
        ...'[EVENT] STOP\n'.codeUnits,
        0,
        0,
        120,
      ]),
      '[EVENT] STOP\n',
    );
    expect(BleBoatLock.decodeLogLine(<int>[0, 0]), '');
  });
}
