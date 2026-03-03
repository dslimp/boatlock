import 'package:boatlock_ui/ble/ble_boatlock.dart';
import 'package:boatlock_ui/models/boat_data.dart';
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
    speedKmh: 0,
    motorPwm: 0,
    battery: 0,
    status: '',
    mode: '',
    rssi: 0,
    holdHeading: false,
    stepSpr: 200,
    stepMaxSpd: 1000,
    stepAccel: 500,
    stepperDeg: 0,
    motorReverse: false,
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

  test('buildNudgeDirCommand validates direction and distance', () {
    expect(
      BleBoatLock.buildNudgeDirCommand('left', meters: 2.0),
      'NUDGE_DIR:LEFT,2.0',
    );
    expect(BleBoatLock.buildNudgeDirCommand('foo', meters: 2.0), isNull);
    expect(BleBoatLock.buildNudgeDirCommand('RIGHT', meters: 0.5), isNull);
    expect(BleBoatLock.buildNudgeDirCommand('RIGHT', meters: 6.0), isNull);
  });

  test(
    'buildNudgeBearingCommand normalizes bearing and validates distance',
    () {
      expect(
        BleBoatLock.buildNudgeBearingCommand(450.0, meters: 3.0),
        'NUDGE_BRG:90.0,3.0',
      );
      expect(
        BleBoatLock.buildNudgeBearingCommand(-10.0, meters: 1.0),
        'NUDGE_BRG:350.0,1.0',
      );
      expect(
        BleBoatLock.buildNudgeBearingCommand(double.nan, meters: 2.0),
        isNull,
      );
      expect(BleBoatLock.buildNudgeBearingCommand(120.0, meters: 0.9), isNull);
    },
  );

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

  test('security helpers build deterministic auth and secure commands', () {
    expect(BleBoatLock.normalizeOwnerCode('123456'), '123456');
    expect(BleBoatLock.normalizeOwnerCode('12-34-56'), '123456');
    expect(BleBoatLock.normalizeOwnerCode('012345'), isNull);

    final proof = BleBoatLock.buildAuthProofHex('123456', 0xABCDEF01);
    expect(proof.length, 8);
    final sessionKey = BleBoatLock.buildSessionKey(proof);
    final secure = BleBoatLock.buildSecureCommand(
      payload: 'ANCHOR_ON',
      counter: 1,
      sessionKey: sessionKey,
    );
    expect(secure.startsWith('SEC_CMD:00000001:'), isTrue);
    expect(secure.endsWith(':ANCHOR_ON'), isTrue);
  });
}
