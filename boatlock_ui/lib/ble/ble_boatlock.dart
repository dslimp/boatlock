import 'dart:convert';
import 'dart:async';
import 'dart:developer' as developer;
import 'dart:io';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter/foundation.dart';
import 'package:permission_handler/permission_handler.dart';
import '../models/boat_data.dart';

typedef BoatDataCallback = void Function(BoatData? data);
typedef LogCallback = void Function(String line);

class BleBoatLock {
  BluetoothDevice? _device;
  BluetoothCharacteristic? _dataChar;
  BluetoothCharacteristic? _cmdChar;
  BluetoothCharacteristic? _logChar;
  final BoatDataCallback onData;
  final LogCallback? onLog;
  BoatData? _lastData;
  Map<String, dynamic>? _lastRaw;
  int _lastRssi = 0;
  bool _secPaired = false;
  bool _secAuth = false;
  int _secNonce = 0;
  int _secCounter = 0;
  int _sessionKey = 0;
  String? _ownerCode;

  BoatData? get currentData => _lastData;

  StreamSubscription<BluetoothConnectionState>? _connectionSub;
  StreamSubscription<List<ScanResult>>? _scanResultsSub;
  StreamSubscription<List<int>>? _dataSub;
  StreamSubscription<List<int>>? _logSub;
  Timer? _reconnectTimer;
  Timer? _heartbeatTimer;
  bool _isConnecting = false;
  DateTime? _lastDataLogAt;

  BleBoatLock({required this.onData, this.onLog});

  Future<void> connectAndListen() async {
    _log('connectAndListen()');
    await _disconnectCurrentDevice();
    final ok = await _ensurePermissions();
    if (!ok) {
      _log('BLE permissions are not granted');
      return;
    }
    await _scanAndConnect();
  }

  Future<void> _disconnectCurrentDevice() async {
    _reconnectTimer?.cancel();
    _heartbeatTimer?.cancel();
    await _scanResultsSub?.cancel();
    _scanResultsSub = null;
    await _connectionSub?.cancel();
    _connectionSub = null;
    await _dataSub?.cancel();
    _dataSub = null;
    await _logSub?.cancel();
    _logSub = null;
    if (_device != null) {
      try {
        await _device!.disconnect();
      } catch (_) {}
    }
    _clearCharacteristics();
  }

  Future<void> sendCustomCommand(String cmd) async {
    if (_cmdChar != null) {
      await _cmdChar!.write(utf8.encode(cmd), withoutResponse: false);
    }
  }

  Future<void> sendPhoneGps(
    double lat,
    double lon, {
    double speedKmh = 0.0,
    int? satellites,
  }) async {
    if (_cmdChar == null) return;
    final cmd = buildSetPhoneGpsCommand(
      lat,
      lon,
      speedKmh: speedKmh,
      satellites: satellites,
    );
    if (cmd == null) return;
    await _writeCommand(cmd);
  }

  Future<void> _scanAndConnect() async {
    _log('_scanAndConnect()');
    if (_isConnecting) return;
    _isConnecting = true;

    await FlutterBluePlus.stopScan();
    await _scanResultsSub?.cancel();
    _scanResultsSub = null;

    bool found = false;
    final scanDone = Completer<void>();
    _scanResultsSub = FlutterBluePlus.scanResults.listen((results) async {
      if (found) return;
      for (var r in results) {
        _log(
          "found device: mac=${r.device.remoteId}, name='${r.device.platformName}', advName='${r.advertisementData.advName}'",
        );
        if (_isBoatLockDevice(r)) {
          found = true;
          if (!scanDone.isCompleted) {
            scanDone.complete();
          }
          _log('BoatLock found, connecting...');
          _device = r.device;
          await FlutterBluePlus.stopScan();
          await _scanResultsSub?.cancel();
          _scanResultsSub = null;
          await _connectToDevice();
          _isConnecting = false;
          return;
        }
      }
    });

    try {
      await FlutterBluePlus.startScan(timeout: const Duration(seconds: 6));
      await Future.any([
        scanDone.future,
        Future.delayed(const Duration(seconds: 7)),
      ]);
    } finally {
      await FlutterBluePlus.stopScan();
      await _scanResultsSub?.cancel();
      _scanResultsSub = null;
      _isConnecting = false;
    }

    if (!found) {
      _log('BoatLock not found, retry scan in 3s');
      _scheduleReconnect();
    }
  }

  Future<void> _connectToDevice() async {
    _log('_connectToDevice() called');
    if (_device == null) {
      _log('_device is null');
      return;
    }
    _isConnecting = true;

    try {
      await _device!.connect(timeout: const Duration(seconds: 20));
      _log('Connected to device');
      _connectionSub?.cancel();
      _connectionSub = _device!.connectionState.listen((state) {
        _log('BLE state: $state');
        if (state == BluetoothConnectionState.disconnected) {
          _heartbeatTimer?.cancel();
          _clearCharacteristics();
          _scheduleReconnect();
        }
      });

      _log('Discovering services...');
      List<BluetoothService> services = await _device!.discoverServices();
      _log('Services discovered: ${services.length}');
      _clearCharacteristics();
      for (var s in services) {
        for (var c in s.characteristics) {
          if (c.uuid.toString().toLowerCase().contains("34cd")) {
            _dataChar = c;
            await _dataChar!.setNotifyValue(true);
            _dataSub?.cancel();
            _dataSub = _dataChar!.lastValueStream.listen(_onNotify);
          }
          if (c.uuid.toString().toLowerCase().contains("56ef")) {
            _cmdChar = c;
          }
          if (c.uuid.toString().toLowerCase().contains("78ab")) {
            _logChar = c;
            await _logChar!.setNotifyValue(true);
            _logSub?.cancel();
            _logSub = _logChar!.lastValueStream.listen(_onLogNotify);
          }
        }
      }
      await requestAllParams();
      _startHeartbeat();
    } catch (e) {
      _log('connect failed: $e');
      _scheduleReconnect();
    }
    _isConnecting = false;
  }

  void _startHeartbeat() {
    _heartbeatTimer?.cancel();
    if (_cmdChar == null) return;
    _heartbeatTimer = Timer.periodic(const Duration(seconds: 2), (_) async {
      if (_cmdChar == null) return;
      try {
        await _cmdChar!.write(
          utf8.encode('HEARTBEAT'),
          withoutResponse: false,
        );
      } catch (_) {
        // reconnect flow handles link issues
      }
    });
  }

  void _scheduleReconnect() {
    _reconnectTimer?.cancel();
    _reconnectTimer = Timer(const Duration(seconds: 3), () async {
      await _scanAndConnect();
    });
  }

  Future<void> requestAllParams() async {
    if (_cmdChar != null) {
      await _cmdChar!.write(utf8.encode("all"), withoutResponse: false);
    }
  }

  void _onNotify(List<int> value) async {
    final jsonString = utf8.decode(value);
    if (jsonString.trim().isEmpty) return;
    try {
      final data = jsonDecode(jsonString);
      if (data is Map<String, dynamic>) {
        _lastRaw = data;
        _secPaired = int.tryParse(data['secPaired']?.toString() ?? '0') == 1;
        _secAuth = int.tryParse(data['secAuth']?.toString() ?? '0') == 1;
        _secNonce = int.tryParse(data['secNonce']?.toString() ?? '0') ?? 0;
      }
      try {
        _lastRssi = await _device?.readRssi() ?? _lastRssi;
      } catch (_) {}
      _lastData = BoatData.fromJson(data, rssi: _lastRssi);
      onData(_lastData);
      final now = DateTime.now();
      if (_lastDataLogAt == null ||
          now.difference(_lastDataLogAt!).inSeconds >= 2) {
        _lastDataLogAt = now;
        _log(
          'data mode=${_lastData!.mode} '
          'lat=${_lastData!.lat.toStringAsFixed(6)} lon=${_lastData!.lon.toStringAsFixed(6)}',
        );
      }
    } catch (_) {
      // ignore parse errors for noise
    }
  }

  void _onLogNotify(List<int> value) {
    final line = utf8.decode(value);
    if (line.trim().isEmpty) return;
    onLog?.call(line);
  }

  Future<void> setAnchor() async {
    if (_cmdChar == null) return;
    final cmd = buildSetAnchorCommand(_lastData);
    if (cmd == null) return;
    await _writeCommand(cmd);
    await _writeCommand('ANCHOR_ON');
  }

  Future<void> setAnchorAt(double lat, double lon) async {
    if (_cmdChar == null) return;
    final cmd = buildSetAnchorCommandFromCoords(lat, lon);
    if (cmd == null) return;
    await _writeCommand(cmd);
    await _writeCommand('ANCHOR_ON');
  }

  Future<void> anchorOn() async {
    if (_cmdChar == null) return;
    await _writeCommand('ANCHOR_ON');
  }

  Future<void> anchorOff() async {
    if (_cmdChar == null) return;
    await _writeCommand('ANCHOR_OFF');
  }

  Future<void> stopAll() async {
    if (_cmdChar == null) return;
    await _writeCommand('STOP');
  }

  Future<void> nudgeDir(String direction, {double meters = 1.0}) async {
    if (_cmdChar == null) return;
    final cmd = buildNudgeDirCommand(direction, meters: meters);
    if (cmd == null) return;
    await _writeCommand(cmd);
  }

  Future<void> nudgeBearing(double bearingDeg, {double meters = 1.0}) async {
    if (_cmdChar == null) return;
    final cmd = buildNudgeBearingCommand(bearingDeg, meters: meters);
    if (cmd == null) return;
    await _writeCommand(cmd);
  }

  Future<void> setAnchorProfile(String profile) async {
    if (_cmdChar == null) return;
    final cmd = buildSetAnchorProfileCommand(profile);
    if (cmd == null) return;
    await _cmdChar!.write(utf8.encode(cmd), withoutResponse: false);
  }

  Future<void> setManualMode(bool manual) async {
    if (_cmdChar != null) {
      await _writeCommand('MANUAL:${manual ? 1 : 0}');
    }
  }

  Future<void> setStepperBowZero() async {
    if (_cmdChar != null) {
      await _writeCommand('SET_STEPPER_BOW');
    }
  }

  Future<void> sendManualDirection(int dir) async {
    if (_cmdChar != null) {
      await _writeCommand('MANUAL_DIR:$dir');
    }
  }

  Future<void> sendManualSpeed(int speed) async {
    if (_cmdChar != null) {
      await _writeCommand('MANUAL_SPEED:$speed');
    }
  }

  void setOwnerCode(String? code) {
    if (code == null) {
      _ownerCode = null;
      return;
    }
    _ownerCode = normalizeOwnerCode(code);
  }

  Future<void> pairSetOwnerCode(String code) async {
    if (_cmdChar == null) return;
    final normalized = normalizeOwnerCode(code);
    if (normalized == null) return;
    await _cmdChar!.write(
      utf8.encode('PAIR_SET:$normalized'),
      withoutResponse: false,
    );
  }

  Future<void> clearPairing() async {
    if (_cmdChar == null) return;
    await _cmdChar!.write(
      utf8.encode('PAIR_CLEAR'),
      withoutResponse: false,
    );
    _sessionKey = 0;
    _secCounter = 0;
  }

  Future<bool> _ensureSecuritySession() async {
    if (_cmdChar == null) return false;
    if (!_secPaired) return true;
    if (_secAuth && _sessionKey != 0) return true;
    final code = _ownerCode;
    if (code == null) return false;

    await _cmdChar!.write(utf8.encode('AUTH_HELLO'), withoutResponse: false);
    await Future.delayed(const Duration(milliseconds: 120));
    await requestAllParams();
    await Future.delayed(const Duration(milliseconds: 120));
    if (_secNonce == 0) return false;

    final proof = buildAuthProofHex(code, _secNonce);
    await _cmdChar!.write(utf8.encode('AUTH_PROVE:$proof'), withoutResponse: false);
    await Future.delayed(const Duration(milliseconds: 120));
    await requestAllParams();
    await Future.delayed(const Duration(milliseconds: 120));
    if (!_secAuth) return false;

    _sessionKey = buildSessionKey(proof);
    _secCounter = 0;
    return true;
  }

  Future<void> _writeCommand(String cmd) async {
    if (_cmdChar == null) return;
    if (_isPlainAllowed(cmd)) {
      await _cmdChar!.write(utf8.encode(cmd), withoutResponse: false);
      return;
    }

    final ok = await _ensureSecuritySession();
    if (!ok || !_secPaired) {
      await _cmdChar!.write(utf8.encode(cmd), withoutResponse: false);
      return;
    }

    _secCounter += 1;
    final secure = buildSecureCommand(
      payload: cmd,
      counter: _secCounter,
      sessionKey: _sessionKey,
    );
    await _cmdChar!.write(utf8.encode(secure), withoutResponse: false);
  }

  bool _isPlainAllowed(String cmd) {
    return cmd == 'STOP' ||
        cmd == 'ANCHOR_OFF' ||
        cmd == 'HEARTBEAT' ||
        cmd == 'all' ||
        cmd.startsWith('AUTH_') ||
        cmd.startsWith('PAIR_') ||
        cmd.startsWith('SEC_CMD:');
  }

  static String? buildSetAnchorCommand(BoatData? data) {
    if (data == null) return null;
    return buildSetAnchorCommandFromCoords(data.lat, data.lon);
  }

  static String? buildSetAnchorCommandFromCoords(double lat, double lon) {
    if (!lat.isFinite || !lon.isFinite) return null;
    if (lat == 0 || lon == 0) return null;
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180) return null;
    return 'SET_ANCHOR:${lat.toStringAsFixed(6)},${lon.toStringAsFixed(6)}';
  }

  static String? buildSetPhoneGpsCommand(
    double lat,
    double lon, {
    double speedKmh = 0.0,
    int? satellites,
  }) {
    if (!lat.isFinite || !lon.isFinite) return null;
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180) return null;
    final safeSpeed = (speedKmh.isFinite && speedKmh > 0) ? speedKmh : 0.0;
    final base =
        'SET_PHONE_GPS:${lat.toStringAsFixed(6)},${lon.toStringAsFixed(6)},${safeSpeed.toStringAsFixed(1)}';
    if (satellites == null) {
      return base;
    }
    final safeSat = satellites < 0 ? 0 : satellites;
    return '$base,$safeSat';
  }

  static String? buildNudgeDirCommand(String direction, {double meters = 1.0}) {
    final dir = direction.trim().toUpperCase();
    const allowed = {'FWD', 'BACK', 'LEFT', 'RIGHT'};
    if (!allowed.contains(dir)) return null;
    if (!meters.isFinite || meters < 1.0 || meters > 5.0) return null;
    return 'NUDGE_DIR:$dir,${meters.toStringAsFixed(1)}';
  }

  static String? buildNudgeBearingCommand(double bearingDeg, {double meters = 1.0}) {
    if (!bearingDeg.isFinite) return null;
    if (!meters.isFinite || meters < 1.0 || meters > 5.0) return null;
    var norm = bearingDeg % 360.0;
    if (norm < 0) norm += 360.0;
    return 'NUDGE_BRG:${norm.toStringAsFixed(1)},${meters.toStringAsFixed(1)}';
  }

  static String? buildSetAnchorProfileCommand(String profile) {
    final raw = profile.trim().toLowerCase();
    if (raw.isEmpty) return null;
    const allowed = {'quiet', 'normal', 'current'};
    if (!allowed.contains(raw)) return null;
    return 'SET_ANCHOR_PROFILE:$raw';
  }

  static String? normalizeOwnerCode(String raw) {
    final digits = raw.replaceAll(RegExp(r'[^0-9]'), '');
    if (digits.length != 6) return null;
    if (digits.startsWith('0')) return null;
    return digits;
  }

  static String buildAuthProofHex(String ownerCode, int nonce) {
    final norm = normalizeOwnerCode(ownerCode);
    if (norm == null || nonce <= 0) return '00000000';
    final msg = '$norm:${_hex32(nonce)}';
    final hash = _fnv1a32(utf8.encode(msg));
    return _hex32(hash);
  }

  static int buildSessionKey(String proofHex) {
    final msg = 'SK:${proofHex.toUpperCase()}';
    return _fnv1a32(utf8.encode(msg));
  }

  static String buildSecureCommand({
    required String payload,
    required int counter,
    required int sessionKey,
  }) {
    final prefix = '${_hex32(sessionKey)}:${_hex32(counter)}:';
    final hash = _fnv1a32(utf8.encode(prefix + payload));
    return 'SEC_CMD:${_hex32(counter)}:${_hex32(hash)}:$payload';
  }

  static int _fnv1a32(List<int> bytes) {
    var hash = 0x811C9DC5;
    for (final b in bytes) {
      hash ^= (b & 0xFF);
      hash = (hash * 0x01000193) & 0xFFFFFFFF;
    }
    return hash & 0xFFFFFFFF;
  }

  static String _hex32(int value) {
    final v = value & 0xFFFFFFFF;
    return v.toRadixString(16).toUpperCase().padLeft(8, '0');
  }

  void dispose() {
    _scanResultsSub?.cancel();
    _connectionSub?.cancel();
    _dataSub?.cancel();
    _logSub?.cancel();
    _reconnectTimer?.cancel();
    _heartbeatTimer?.cancel();
    _device?.disconnect();
    _clearCharacteristics();
  }

  void _clearCharacteristics() {
    _dataChar = null;
    _cmdChar = null;
    _logChar = null;
  }

  void _log(String msg) {
    debugPrint('[BleBoatLock] $msg');
    developer.log(msg, name: 'BleBoatLock');
  }

  bool _isBoatLockDevice(ScanResult r) {
    final advName = r.advertisementData.advName.trim().toLowerCase();
    final devName = r.device.platformName.trim().toLowerCase();
    final nameMatch =
        advName == 'boatlock' ||
        devName == 'boatlock' ||
        advName.contains('boatlock') ||
        devName.contains('boatlock');

    final serviceMatch = r.advertisementData.serviceUuids.any(
      (uuid) => uuid.toString().toLowerCase().contains('12ab'),
    );

    return nameMatch || serviceMatch;
  }

  Future<bool> _ensurePermissions() async {
    if (!Platform.isAndroid) {
      return true;
    }

    final required = <Permission>[
      Permission.locationWhenInUse,
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
    ];
    final statuses = await required.request();
    for (final entry in statuses.entries) {
      if (!entry.value.isGranted) {
        _log('Permission denied: ${entry.key}');
        return false;
      }
    }
    return true;
  }
}
