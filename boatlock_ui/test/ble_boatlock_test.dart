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
    battery: 0,
    status: '',
    mode: '',
    rssi: 0,
    holdHeading: false,
    emuCompass: 0,
    routeIdx: 0,
    stepSpr: 200,
    stepMaxSpd: 1000,
    stepAccel: 500,
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

  test('buildSetPhoneGpsCommand validates and formats payload', () {
    expect(
      BleBoatLock.buildSetPhoneGpsCommand(59.9386312, 30.3141119, speedKmh: 12.34),
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
    expect(
      BleBoatLock.buildSetPhoneGpsCommand(95, 30.0),
      isNull,
    );
    expect(
      BleBoatLock.buildSetPhoneGpsCommand(59.0, 200.0),
      isNull,
    );
  });
}
