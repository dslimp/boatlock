import 'package:boatlock_ui/ble/ble_boatlock.dart';
import 'package:boatlock_ui/models/boat_data.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_test/flutter_test.dart';

BoatData _data({required double lat, required double lon}) {
  return BoatData(
    lat: lat,
    lon: lon,
    anchorLat: 0,
    anchorLon: 0,
    anchorHeading: 0,
    distance: 0,
    heading: 0,
    battery: 0,
    status: '',
    statusReasons: '',
    mode: '',
    rssi: 0,
    holdHeading: false,
    stepSpr: 200,
    stepMaxSpd: 1000,
    stepAccel: 500,
    headingRaw: 0,
    compassOffset: 0,
    compassQ: 0,
    magQ: 0,
    gyroQ: 0,
    rvAcc: 0,
    magNorm: 0,
    gyroNorm: 0,
    pitch: 0,
    roll: 0,
    secPaired: false,
    secAuth: false,
    secPairWindowOpen: false,
    secReject: 'NONE',
  );
}

void main() {
  test('buildSetAnchorCommand returns null for null data', () {
    expect(BleBoatLock.buildSetAnchorCommand(null), isNull);
  });

  test('buildSetAnchorCommand returns null for zero coordinates', () {
    expect(BleBoatLock.buildSetAnchorCommand(_data(lat: 0, lon: 30.2)), isNull);
    expect(BleBoatLock.buildSetAnchorCommand(_data(lat: 59.1, lon: 0)), isNull);
  });

  test('buildSetAnchorCommand builds protocol command with 6 digits', () {
    expect(
      BleBoatLock.buildSetAnchorCommand(
        _data(lat: 59.1234567, lon: 30.7654321),
      ),
      'SET_ANCHOR:59.123457,30.765432',
    );
  });

  test('buildSetAnchorCommandFromCoords validates ranges', () {
    expect(
      BleBoatLock.buildSetAnchorCommandFromCoords(59.1, 30.2),
      'SET_ANCHOR:59.100000,30.200000',
    );
    expect(
      BleBoatLock.buildSetAnchorCommandFromCoords(double.nan, 30.2),
      isNull,
    );
    expect(
      BleBoatLock.buildSetAnchorCommandFromCoords(59.1, double.infinity),
      isNull,
    );
    expect(BleBoatLock.buildSetAnchorCommandFromCoords(-95.0, 30.0), isNull);
    expect(BleBoatLock.buildSetAnchorCommandFromCoords(59.0, 181.0), isNull);
  });

  test('buildSetPhoneGpsCommand validates and formats payload', () {
    expect(
      BleBoatLock.buildSetPhoneGpsCommand(
        59.9386312,
        30.3141119,
        speedKmh: 12.34,
      ),
      'SET_PHONE_GPS:59.938631,30.314112,12.3',
    );
    expect(
      BleBoatLock.buildSetPhoneGpsCommand(
        59.9386312,
        30.3141119,
        speedKmh: 12.34,
        satellites: 9,
      ),
      'SET_PHONE_GPS:59.938631,30.314112,12.3,9',
    );
    expect(BleBoatLock.buildSetPhoneGpsCommand(95, 30.0), isNull);
    expect(BleBoatLock.buildSetPhoneGpsCommand(59.0, 200.0), isNull);
  });

  test('buildNudgeDirCommand validates direction', () {
    expect(BleBoatLock.buildNudgeDirCommand('left'), 'NUDGE_DIR:LEFT');
    expect(BleBoatLock.buildNudgeDirCommand('foo'), isNull);
  });

  test('buildNudgeBearingCommand normalizes bearing', () {
    expect(BleBoatLock.buildNudgeBearingCommand(450.0), 'NUDGE_BRG:90.0');
    expect(BleBoatLock.buildNudgeBearingCommand(-10.0), 'NUDGE_BRG:350.0');
    expect(BleBoatLock.buildNudgeBearingCommand(double.nan), isNull);
  });

  test('buildSetAnchorProfileCommand validates profile values', () {
    expect(
      BleBoatLock.buildSetAnchorProfileCommand('quiet'),
      'SET_ANCHOR_PROFILE:quiet',
    );
    expect(
      BleBoatLock.buildSetAnchorProfileCommand(' CURRENT '),
      'SET_ANCHOR_PROFILE:current',
    );
    expect(BleBoatLock.buildSetAnchorProfileCommand('storm'), isNull);
    expect(BleBoatLock.buildSetAnchorProfileCommand(''), isNull);
  });

  test('buildManualSetCommand validates atomic deadman payload', () {
    expect(
      BleBoatLock.buildManualSetCommand(steer: -1, throttlePct: 45, ttlMs: 500),
      'MANUAL_SET:-1,45,500',
    );
    expect(
      BleBoatLock.buildManualSetCommand(steer: 2, throttlePct: 45, ttlMs: 500),
      isNull,
    );
    expect(
      BleBoatLock.buildManualSetCommand(steer: 0, throttlePct: 101, ttlMs: 500),
      isNull,
    );
    expect(
      BleBoatLock.buildManualSetCommand(steer: 0, throttlePct: 10, ttlMs: 50),
      isNull,
    );
  });

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
