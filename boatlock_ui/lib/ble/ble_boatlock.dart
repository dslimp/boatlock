import 'dart:convert';
import 'dart:async';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
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
  Timer? _reconnectTimer;
  bool _isConnecting = false;

  BleBoatLock({required this.onData, this.onLog});

  Future<void> connectAndListen() async {
    print("[BLE] connectAndListen()");
    await _disconnectAll();
    _scanAndConnect();
  }

  Future<void> _disconnectAll() async {
    // Отключаем все BLE-устройства (современно для flutter_blue_plus 1.35+)
    for (var d in await FlutterBluePlus.connectedSystemDevices) {
      try { await d.disconnect(); } catch (_) {}
    }
  }

  Future<void> sendCustomCommand(String cmd) async {
    if (_cmdChar != null) {
      await _cmdChar!.write(utf8.encode(cmd), withoutResponse: false);
    }
  }


  Future<void> _scanAndConnect() async {
    print("[BLE] _scanAndConnect()");
    if (_isConnecting) return;
    _isConnecting = true;

    await FlutterBluePlus.stopScan();

    FlutterBluePlus.startScan(timeout: const Duration(seconds: 6));
    FlutterBluePlus.scanResults.listen((results) async {
      for (var r in results) {
        print("[BLE] found device: mac=${r.device.remoteId}, name='${r.device.platformName}', advName='${r.advertisementData.advName}'");
        if (r.advertisementData.advName == "BoatLock") {
          print("[BLE] BoatLock found, connecting...");
          _device = r.device;
          await FlutterBluePlus.stopScan();
          await _connectToDevice();
          _isConnecting = false;
          return;
        }
      }
    });

    // Safety: разрешить новую попытку через 7 сек
    Future.delayed(Duration(seconds: 7), () {
      _isConnecting = false;
    });
  }

  Future<void> _connectToDevice() async {
    print("[BLE] _connectToDevice() called");
    if (_device == null) {
      print("[BLE] _device is null!");
      return;
    }
    _isConnecting = true;

    try {
      await _device!.connect(timeout: Duration(seconds: 10));
      print("[BLE] Connected to device!");
      _connectionSub?.cancel();
      _connectionSub = _device!.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          _scheduleReconnect();
        }
      });

      // Подписка на disconnect
      _device!.connectionState.listen((state) {
        print("[BLE] BLE state: $state");
        if (state == BluetoothConnectionState.disconnected) {
          print("[BLE] BLE disconnected! Scheduling reconnect...");
          // _scheduleReconnect(); // если используешь авто-реконнект
        }
      });



      List<BluetoothService> services = await _device!.discoverServices();
      for (var s in services) {
        for (var c in s.characteristics) {
          if (c.uuid.toString().toLowerCase().contains("34cd")) {
            _dataChar = c;
            await _dataChar!.setNotifyValue(true);
            _dataChar!.lastValueStream.listen(_onNotify);
          }
          if (c.uuid.toString().toLowerCase().contains("56ef")) {
            _cmdChar = c;
          }
          if (c.uuid.toString().toLowerCase().contains("78ab")) {
            _logChar = c;
            await _logChar!.setNotifyValue(true);
            _logChar!.lastValueStream.listen(_onLogNotify);
          }
        }
      }
      requestAllParams();
    } catch (e) {
      _scheduleReconnect();
    }
    _isConnecting = false;
  }

  void _scheduleReconnect() {
    _reconnectTimer?.cancel();
    _reconnectTimer = Timer(Duration(seconds: 3), () async {
      await _scanAndConnect();
    });
  }

  Future<void>  requestAllParams() async {
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
    } catch (_) {
      // ignore parse errors for noise
    }
  }

  void _onLogNotify(List<int> value) {
    final line = utf8.decode(value);
    if (line.trim().isEmpty) return;
    onLog?.call(line);
  }

  Future<void> setAnchor({double? lat, double? lon}) async {
    final a = lat ?? _lastData?.lat;
    final o = lon ?? _lastData?.lon;
    if (_cmdChar != null && a != null && o != null) {
      final cmd = 'SET_ANCHOR:$a,$o';
      await _cmdChar!.write(utf8.encode(cmd), withoutResponse: false);
    }
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
      await _cmdChar!.write(utf8.encode('CALIB_COMPASS'), withoutResponse: false);
    }
  }

  Future<void> setManualMode(bool manual) async {
    if (_cmdChar != null) {
      await _cmdChar!
          .write(utf8.encode('MANUAL:${manual ? 1 : 0}'), withoutResponse: false);
    }
  }

  Future<void> sendManualDirection(int dir) async {
    if (_cmdChar != null) {
      await _cmdChar!
          .write(utf8.encode('MANUAL_DIR:$dir'), withoutResponse: false);
    }
  }

  Future<void> sendManualSpeed(int speed) async {
    if (_cmdChar != null) {
      await _cmdChar!
          .write(utf8.encode('MANUAL_SPEED:$speed'), withoutResponse: false);
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

  void dispose() {
    _connectionSub?.cancel();
    _reconnectTimer?.cancel();
  }
}
