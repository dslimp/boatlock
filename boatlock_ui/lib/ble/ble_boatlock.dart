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
  int _lastRssi = 0;

  BoatData? get currentData => _lastData;

  StreamSubscription<BluetoothConnectionState>? _connectionSub;
  StreamSubscription<List<ScanResult>>? _scanResultsSub;
  StreamSubscription<List<int>>? _dataSub;
  StreamSubscription<List<int>>? _logSub;
  Timer? _reconnectTimer;
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

  Future<void> sendHeading(double headingDeg) async {
    if (_cmdChar == null || !headingDeg.isFinite) return;
    final normalized = ((headingDeg % 360.0) + 360.0) % 360.0;
    await _cmdChar!.write(
      utf8.encode('SET_HEADING:${normalized.toStringAsFixed(1)}'),
      withoutResponse: false,
    );
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
    await _cmdChar!.write(utf8.encode(cmd), withoutResponse: false);
  }

  Future<void> _scanAndConnect() async {
    _log('_scanAndConnect()');
    if (_isConnecting) return;
    _isConnecting = true;

    await FlutterBluePlus.stopScan();
    await _scanResultsSub?.cancel();
    _scanResultsSub = null;

    bool found = false;
    await FlutterBluePlus.startScan(timeout: const Duration(seconds: 6));
    _scanResultsSub = FlutterBluePlus.scanResults.listen((results) async {
      if (found) return;
      for (var r in results) {
        _log(
          "found device: mac=${r.device.remoteId}, name='${r.device.platformName}', advName='${r.advertisementData.advName}'",
        );
        if (_isBoatLockDevice(r)) {
          found = true;
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

    // Safety: разрешить новую попытку через 7 сек
    Future.delayed(const Duration(seconds: 7), () {
      _isConnecting = false;
    });
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
    } catch (e) {
      _log('connect failed: $e');
      _scheduleReconnect();
    }
    _isConnecting = false;
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
          'data mode=${_lastData!.mode} emu=${_lastData!.emuCompass} '
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
    await _cmdChar!.write(utf8.encode(cmd), withoutResponse: false);
  }

  Future<void> startRoute() async {
    if (_cmdChar != null) {
      await _cmdChar!.write(utf8.encode('START_ROUTE'), withoutResponse: false);
    }
  }

  Future<void> stopRoute() async {
    if (_cmdChar != null) {
      await _cmdChar!.write(utf8.encode('STOP_ROUTE'), withoutResponse: false);
    }
  }

  Future<void> calibrateCompass() async {
    if (_cmdChar != null) {
      await _cmdChar!.write(
        utf8.encode('CALIB_COMPASS'),
        withoutResponse: false,
      );
    }
  }

  Future<void> setManualMode(bool manual) async {
    if (_cmdChar != null) {
      await _cmdChar!.write(
        utf8.encode('MANUAL:${manual ? 1 : 0}'),
        withoutResponse: false,
      );
    }
  }

  Future<void> sendManualDirection(int dir) async {
    if (_cmdChar != null) {
      await _cmdChar!.write(
        utf8.encode('MANUAL_DIR:$dir'),
        withoutResponse: false,
      );
    }
  }

  Future<void> sendManualSpeed(int speed) async {
    if (_cmdChar != null) {
      await _cmdChar!.write(
        utf8.encode('MANUAL_SPEED:$speed'),
        withoutResponse: false,
      );
    }
  }

  Future<void> exportLog() async {
    if (_cmdChar != null) {
      await _cmdChar!.write(utf8.encode('EXPORT_LOG'), withoutResponse: false);
    }
  }

  Future<void> clearLog() async {
    if (_cmdChar != null) {
      await _cmdChar!.write(utf8.encode('CLEAR_LOG'), withoutResponse: false);
    }
  }

  static String? buildSetAnchorCommand(BoatData? data) {
    if (data == null) return null;
    if (data.lat == 0 || data.lon == 0) return null;
    return 'SET_ANCHOR:${data.lat.toStringAsFixed(6)},${data.lon.toStringAsFixed(6)}';
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

  void dispose() {
    _scanResultsSub?.cancel();
    _connectionSub?.cancel();
    _dataSub?.cancel();
    _logSub?.cancel();
    _reconnectTimer?.cancel();
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
    return advName == 'boatlock' || devName == 'boatlock';
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
