import 'dart:typed_data';

import '../models/boat_data.dart';

class BleLiveFrame {
  final BoatData data;
  final String secNonceHex;
  final int sequence;

  const BleLiveFrame({
    required this.data,
    required this.secNonceHex,
    required this.sequence,
  });
}

const int _flagHoldHeading = 1 << 0;
const int _flagSecPaired = 1 << 1;
const int _flagSecAuth = 1 << 2;
const int _flagSecPairWindow = 1 << 3;
const int _liveFrameSize = 70;

const Map<int, String> _modeByCode = {
  0: 'IDLE',
  1: 'HOLD',
  2: 'ANCHOR',
  3: 'MANUAL',
  4: 'SIM',
};

const Map<int, String> _statusByCode = {
  0: 'OK',
  1: 'WARN',
  2: 'ALERT',
};

const Map<int, String> _rejectByCode = {
  0: 'NONE',
  1: 'NOT_PAIRED',
  2: 'PAIRING_WINDOW_CLOSED',
  3: 'INVALID_OWNER_SECRET',
  4: 'AUTH_REQUIRED',
  5: 'AUTH_FAILED',
  6: 'BAD_FORMAT',
  7: 'BAD_SIGNATURE',
  8: 'REPLAY_DETECTED',
  9: 'RATE_LIMIT',
};

const List<(int, String)> _reasonBits = [
  (1 << 0, 'NO_GPS'),
  (1 << 1, 'NO_COMPASS'),
  (1 << 2, 'DRIFT_ALERT'),
  (1 << 3, 'DRIFT_FAIL'),
  (1 << 4, 'CONTAINMENT_BREACH'),
  (1 << 5, 'GPS_WEAK'),
  (1 << 6, 'COMM_TIMEOUT'),
  (1 << 7, 'CONTROL_LOOP_TIMEOUT'),
  (1 << 8, 'SENSOR_TIMEOUT'),
  (1 << 9, 'INTERNAL_ERROR_NAN'),
  (1 << 10, 'COMMAND_OUT_OF_RANGE'),
  (1 << 11, 'STOP_CMD'),
  (1 << 12, 'GPS_HDOP_TOO_HIGH'),
  (1 << 13, 'GPS_SATS_TOO_LOW'),
  (1 << 14, 'GPS_DATA_STALE'),
  (1 << 15, 'GPS_POSITION_JUMP'),
  (1 << 16, 'NUDGE_OK'),
  (1 << 17, 'NO_ANCHOR_POINT'),
  (1 << 18, 'NO_HEADING'),
  (1 << 19, 'GPS_HDOP_MISSING'),
];

BleLiveFrame? decodeBoatLockLiveFrame(List<int> value, {int rssi = 0}) {
  if (value.length != _liveFrameSize) {
    return null;
  }
  final bytes = Uint8List.fromList(value);
  final view = ByteData.sublistView(bytes);
  if (view.getUint8(0) != 0x42 || view.getUint8(1) != 0x4C) {
    return null;
  }
  if (view.getUint8(2) != 2 || view.getUint8(3) != 1) {
    return null;
  }

  final sequence = view.getUint16(4, Endian.little);
  final flags = view.getUint16(6, Endian.little);
  final mode = _modeByCode[view.getUint8(8)] ?? 'UNKNOWN';
  final status = _statusByCode[view.getUint8(9)] ?? 'WARN';
  final reasonFlags = view.getUint32(57, Endian.little);
  final secReject = _rejectByCode[view.getUint8(56)] ?? 'UNKNOWN';
  final secNonce = view.getUint64(61, Endian.little);

  final data = BoatData(
    lat: view.getInt32(10, Endian.little) / 10000000.0,
    lon: view.getInt32(14, Endian.little) / 10000000.0,
    anchorLat: view.getInt32(18, Endian.little) / 10000000.0,
    anchorLon: view.getInt32(22, Endian.little) / 10000000.0,
    anchorHeading: view.getUint16(26, Endian.little) / 10.0,
    distance: view.getUint16(28, Endian.little) / 100.0,
    heading: view.getUint16(30, Endian.little) / 10.0,
    battery: view.getUint8(32),
    status: status,
    statusReasons: _decodeReasons(reasonFlags),
    mode: mode,
    rssi: rssi,
    holdHeading: (flags & _flagHoldHeading) != 0,
    stepSpr: view.getUint16(33, Endian.little),
    stepMaxSpd: view.getUint16(35, Endian.little).toDouble(),
    stepAccel: view.getUint16(37, Endian.little).toDouble(),
    headingRaw: view.getUint16(39, Endian.little) / 10.0,
    compassOffset: view.getInt16(41, Endian.little) / 10.0,
    compassQ: view.getUint8(43),
    magQ: view.getUint8(44),
    gyroQ: view.getUint8(45),
    rvAcc: view.getUint16(46, Endian.little) / 100.0,
    magNorm: view.getUint16(48, Endian.little) / 100.0,
    gyroNorm: view.getUint16(50, Endian.little) / 100.0,
    pitch: view.getInt16(52, Endian.little) / 10.0,
    roll: view.getInt16(54, Endian.little) / 10.0,
    secPaired: (flags & _flagSecPaired) != 0,
    secAuth: (flags & _flagSecAuth) != 0,
    secPairWindowOpen: (flags & _flagSecPairWindow) != 0,
    secReject: secReject,
  );

  return BleLiveFrame(
    data: data,
    secNonceHex: secNonce.toRadixString(16).toUpperCase().padLeft(16, '0'),
    sequence: sequence,
  );
}

String _decodeReasons(int flags) {
  final reasons = <String>[];
  for (final (bit, label) in _reasonBits) {
    if ((flags & bit) != 0) {
      reasons.add(label);
    }
  }
  return reasons.join(',');
}
