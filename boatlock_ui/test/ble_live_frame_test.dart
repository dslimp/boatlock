import 'dart:typed_data';

import 'package:boatlock_ui/ble/ble_live_frame.dart';
import 'package:flutter_test/flutter_test.dart';

void _u8(BytesBuilder out, int value) {
  out.addByte(value & 0xFF);
}

void _u16(BytesBuilder out, int value) {
  out.add([value & 0xFF, (value >> 8) & 0xFF]);
}

void _i16(BytesBuilder out, int value) {
  _u16(out, value & 0xFFFF);
}

void _u32(BytesBuilder out, int value) {
  out.add([
    value & 0xFF,
    (value >> 8) & 0xFF,
    (value >> 16) & 0xFF,
    (value >> 24) & 0xFF,
  ]);
}

void _i32(BytesBuilder out, int value) {
  _u32(out, value & 0xFFFFFFFF);
}

void _u64(BytesBuilder out, int value) {
  for (var i = 0; i < 8; i += 1) {
    out.addByte((value >> (i * 8)) & 0xFF);
  }
}

List<int> _frame() {
  final out = BytesBuilder();
  _u8(out, 0x42);
  _u8(out, 0x4C);
  _u8(out, 3);
  _u8(out, 1);
  _u16(out, 9);
  _u16(out, 0x000F);
  _u8(out, 2);
  _u8(out, 1);
  _i32(out, (55.454227 * 10000000).round());
  _i32(out, (37.560818 * 10000000).round());
  _i32(out, (55.450001 * 10000000).round());
  _i32(out, (37.560002 * 10000000).round());
  _u16(out, 123);
  _u16(out, 4225);
  _u16(out, 2714);
  _u16(out, 3599);
  _u8(out, 81);
  _u16(out, 4096);
  _u16(out, 700);
  _u16(out, 250);
  _u16(out, 1111);
  _i16(out, -25);
  _u8(out, 3);
  _u8(out, 2);
  _u8(out, 1);
  _u16(out, 225);
  _u16(out, 5010);
  _u16(out, 1234);
  _i16(out, -12);
  _i16(out, 34);
  _u8(out, 4);
  _u32(
    out,
    (1 << 0) | (1 << 3) | (1 << 14) | (1 << 15) | (1 << 18) | (1 << 19),
  );
  _u64(out, 0x0123456789ABCDEF);
  _u8(out, 2);
  return out.toBytes();
}

void main() {
  test('decodeBoatLockLiveFrame decodes v3 live telemetry', () {
    final decoded = decodeBoatLockLiveFrame(_frame(), rssi: -61);

    expect(decoded, isNotNull);
    expect(_frame(), hasLength(boatLockLiveFrameSize));
    expect(decoded!.sequence, 9);
    expect(decoded.secNonceHex, '0123456789ABCDEF');
    expect(decoded.data.mode, 'ANCHOR');
    expect(decoded.data.status, 'WARN');
    expect(decoded.data.lat, closeTo(55.454227, 0.000001));
    expect(decoded.data.lon, closeTo(37.560818, 0.000001));
    expect(decoded.data.anchorHeading, 12.3);
    expect(decoded.data.distance, 42.25);
    expect(decoded.data.anchorBearing, 271.4);
    expect(decoded.data.heading, 359.9);
    expect(decoded.data.holdHeading, isTrue);
    expect(decoded.data.secPaired, isTrue);
    expect(decoded.data.secAuth, isTrue);
    expect(decoded.data.secPairWindowOpen, isTrue);
    expect(decoded.data.secReject, 'AUTH_REQUIRED');
    expect(decoded.data.gnssQ, 2);
    expect(
      decoded.data.statusReasons,
      'NO_GPS,DRIFT_FAIL,GPS_DATA_STALE,GPS_POSITION_JUMP,NO_HEADING,GPS_HDOP_MISSING',
    );
    expect(decoded.data.rssi, -61);
  });

  test('decodeBoatLockLiveFrame rejects non-v3 payloads', () {
    expect(decodeBoatLockLiveFrame(const []), isNull);
    final bad = _frame();
    bad[2] = 2;
    expect(decodeBoatLockLiveFrame(bad), isNull);
  });

  test('decodeBoatLockLiveFrame rejects padded payloads', () {
    final padded = [..._frame(), 0];

    expect(decodeBoatLockLiveFrame(padded), isNull);
  });

  test('decodeBoatLockLiveFrame rejects unknown enum codes', () {
    final unknownMode = _frame();
    unknownMode[8] = 99;
    expect(decodeBoatLockLiveFrame(unknownMode), isNull);

    final unknownStatus = _frame();
    unknownStatus[9] = 99;
    expect(decodeBoatLockLiveFrame(unknownStatus), isNull);

    final unknownReject = _frame();
    unknownReject[58] = 99;
    expect(decodeBoatLockLiveFrame(unknownReject), isNull);
  });
}
