import 'dart:typed_data';

import '../models/boat_data.dart';

class BleLiveFrame {
  final BoatData data;
  final int sequence;

  const BleLiveFrame({required this.data, required this.sequence});
}

const int _flagHoldHeading = 1 << 0;
const int boatLockLiveFrameSize = 65;
const int _magicB = 0x42;
const int _magicL = 0x4C;
const int _protocolVersion = 5;
const int _frameTypeLiveTelemetry = 1;
const Endian _wireEndian = Endian.little;

const int _offsetMagicB = 0;
const int _offsetMagicL = 1;
const int _offsetProtocolVersion = 2;
const int _offsetFrameType = 3;
const int _offsetSequence = 4;
const int _offsetFlags = 6;
const int _offsetMode = 8;
const int _offsetStatus = 9;
const int _offsetLat = 10;
const int _offsetLon = 14;
const int _offsetAnchorLat = 18;
const int _offsetAnchorLon = 22;
const int _offsetAnchorHeading = 26;
const int _offsetDistance = 28;
const int _offsetAnchorBearing = 30;
const int _offsetHeading = 32;
const int _offsetBattery = 34;
const int _offsetStepSpr = 35;
const int _offsetStepGear = 37;
const int _offsetStepMaxSpd = 39;
const int _offsetStepAccel = 41;
const int _offsetHeadingRaw = 43;
const int _offsetCompassOffset = 45;
const int _offsetCompassQ = 47;
const int _offsetMagQ = 48;
const int _offsetGyroQ = 49;
const int _offsetRvAcc = 50;
const int _offsetMagNorm = 52;
const int _offsetGyroNorm = 54;
const int _offsetPitch = 56;
const int _offsetRoll = 58;
const int _offsetReasonFlags = 60;
const int _offsetGnssQ = 64;

const Map<int, String> _modeByCode = {
  0: 'IDLE',
  1: 'HOLD',
  2: 'ANCHOR',
  3: 'MANUAL',
  4: 'SIM',
};

const Map<int, String> _statusByCode = {0: 'OK', 1: 'WARN', 2: 'ALERT'};

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
  if (value.length != boatLockLiveFrameSize) {
    return null;
  }
  final bytes = value is Uint8List ? value : Uint8List.fromList(value);
  final view = ByteData.sublistView(bytes);
  if (view.getUint8(_offsetMagicB) != _magicB ||
      view.getUint8(_offsetMagicL) != _magicL) {
    return null;
  }
  final protocolVersion = view.getUint8(_offsetProtocolVersion);
  if (view.getUint8(_offsetFrameType) != _frameTypeLiveTelemetry) {
    return null;
  }
  if (protocolVersion != _protocolVersion) {
    return null;
  }

  final mode = _modeByCode[view.getUint8(_offsetMode)];
  final status = _statusByCode[view.getUint8(_offsetStatus)];
  if (mode == null || status == null) {
    return null;
  }

  final sequence = view.getUint16(_offsetSequence, _wireEndian);
  final flags = view.getUint16(_offsetFlags, _wireEndian);
  final reasonFlags = view.getUint32(_offsetReasonFlags, _wireEndian);

  final data = BoatData(
    lat: view.getInt32(_offsetLat, _wireEndian) / 10000000.0,
    lon: view.getInt32(_offsetLon, _wireEndian) / 10000000.0,
    anchorLat: view.getInt32(_offsetAnchorLat, _wireEndian) / 10000000.0,
    anchorLon: view.getInt32(_offsetAnchorLon, _wireEndian) / 10000000.0,
    anchorHeading: view.getUint16(_offsetAnchorHeading, _wireEndian) / 10.0,
    distance: view.getUint16(_offsetDistance, _wireEndian) / 100.0,
    anchorBearing: view.getUint16(_offsetAnchorBearing, _wireEndian) / 10.0,
    heading: view.getUint16(_offsetHeading, _wireEndian) / 10.0,
    battery: view.getUint8(_offsetBattery),
    status: status,
    statusReasons: _decodeReasons(reasonFlags),
    mode: mode,
    rssi: rssi,
    holdHeading: (flags & _flagHoldHeading) != 0,
    stepSpr: view.getUint16(_offsetStepSpr, _wireEndian),
    stepGear: view.getUint16(_offsetStepGear, _wireEndian) / 10.0,
    stepMaxSpd: view.getUint16(_offsetStepMaxSpd, _wireEndian).toDouble(),
    stepAccel: view.getUint16(_offsetStepAccel, _wireEndian).toDouble(),
    headingRaw: view.getUint16(_offsetHeadingRaw, _wireEndian) / 10.0,
    compassOffset: view.getInt16(_offsetCompassOffset, _wireEndian) / 10.0,
    compassQ: view.getUint8(_offsetCompassQ),
    magQ: view.getUint8(_offsetMagQ),
    gyroQ: view.getUint8(_offsetGyroQ),
    rvAcc: view.getUint16(_offsetRvAcc, _wireEndian) / 100.0,
    magNorm: view.getUint16(_offsetMagNorm, _wireEndian) / 100.0,
    gyroNorm: view.getUint16(_offsetGyroNorm, _wireEndian) / 100.0,
    pitch: view.getInt16(_offsetPitch, _wireEndian) / 10.0,
    roll: view.getInt16(_offsetRoll, _wireEndian) / 10.0,
    gnssQ: view.getUint8(_offsetGnssQ),
  );

  return BleLiveFrame(data: data, sequence: sequence);
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
