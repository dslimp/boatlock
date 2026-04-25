import 'package:boatlock_ui/ble/ble_boatlock.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('security helpers build deterministic auth and secure commands', () {
    expect(
      BleBoatLock.normalizeOwnerSecret('00112233445566778899aabbccddeeff'),
      '00112233445566778899AABBCCDDEEFF',
    );
    expect(
      BleBoatLock.normalizeOwnerSecret(
        '00 11 22 33 44 55 66 77 88 99 aa bb cc dd ee ff',
      ),
      '00112233445566778899AABBCCDDEEFF',
    );
    expect(BleBoatLock.normalizeOwnerSecret('123456'), isNull);

    final proof = BleBoatLock.buildAuthProofHex(
      '00112233445566778899AABBCCDDEEFF',
      '0123456789ABCDEF',
    );
    expect(proof.length, 16);
    final secure = BleBoatLock.buildSecureCommand(
      ownerSecret: '00112233445566778899AABBCCDDEEFF',
      nonceHex: '0123456789ABCDEF',
      payload: 'ANCHOR_ON',
      counter: 1,
    );
    expect(secure.startsWith('SEC_CMD:00000001:'), isTrue);
    expect(secure.endsWith(':ANCHOR_ON'), isTrue);
  });

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
