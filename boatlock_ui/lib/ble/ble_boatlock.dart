import 'dart:convert';
import 'dart:async';
import 'dart:developer' as developer;
import 'dart:io';
import 'dart:math';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter/widgets.dart';
import 'package:permission_handler/permission_handler.dart';
import '../models/boat_data.dart';
import 'ble_commands.dart';
import 'ble_command_text.dart';
import 'ble_live_frame.dart';
import 'ble_reconnect_policy.dart';

typedef BoatDataCallback = void Function(BoatData? data);
typedef LogCallback = void Function(String line);

class BleBoatLock with WidgetsBindingObserver {
  BluetoothDevice? _device;
  BluetoothCharacteristic? _dataChar;
  BluetoothCharacteristic? _cmdChar;
  BluetoothCharacteristic? _logChar;
  final BoatDataCallback onData;
  final LogCallback? onLog;
  BoatData? _lastData;
  int _lastRssi = 0;
  bool _secPaired = false;
  bool _secAuth = false;
  bool _secPairWindowOpen = false;
  String _secNonceHex = '0000000000000000';
  String _secReject = 'NONE';
  int _secCounter = 0;
  String? _ownerSecret;

  BoatData? get currentData => _lastData;
  bool get secPaired => _secPaired;
  bool get secAuth => _secAuth;
  bool get secPairWindowOpen => _secPairWindowOpen;
  String get secReject => _secReject;
  String? get ownerSecret => _ownerSecret;

  StreamSubscription<BluetoothConnectionState>? _connectionSub;
  StreamSubscription<List<ScanResult>>? _scanResultsSub;
  StreamSubscription<BluetoothAdapterState>? _adapterStateSub;
  StreamSubscription<List<int>>? _dataSub;
  StreamSubscription<List<int>>? _logSub;
  Timer? _reconnectTimer;
  Timer? _heartbeatTimer;
  final BleReconnectPolicy _reconnectPolicy = BleReconnectPolicy();
  bool _isConnecting = false;
  bool _isDisposed = false;
  DateTime? _lastDataLogAt;

  BleBoatLock({required this.onData, this.onLog});

  Future<void> connectAndListen() async {
    _log('connectAndListen()');
    _isDisposed = false;
    WidgetsBinding.instance.addObserver(this);
    await _disconnectCurrentDevice();
    final ok = await _ensurePermissions();
    if (!ok) {
      _log('BLE permissions are not granted');
      return;
    }
    await _watchAdapterState();
    final adapterReady = await _readAdapterReady();
    final decision = _reconnectPolicy.start(adapterReady: adapterReady);
    await _applyReconnectDecision(decision, waitForScan: true);
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
    _device = null;
    _clearCharacteristics();
  }

  Future<bool> sendCustomCommand(String cmd) async {
    return _writeCommand(cmd);
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
    if (_isDisposed || !_reconnectPolicy.canAttempt) {
      _log(
        'scan skipped: disposed=$_isDisposed canAttempt=${_reconnectPolicy.canAttempt}',
      );
      return;
    }
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
    } catch (e) {
      _log('scan failed: $e');
    } finally {
      await FlutterBluePlus.stopScan();
      await _scanResultsSub?.cancel();
      _scanResultsSub = null;
      _isConnecting = false;
    }

    if (!found) {
      _log('BoatLock not found, retry scan in 3s');
      await _applyReconnectDecision(_reconnectPolicy.scanMiss());
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
          unawaited(_applyReconnectDecision(_reconnectPolicy.disconnected()));
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
      await _setStreamEnabled(true);
      await requestSnapshot();
      _startHeartbeat();
    } catch (e) {
      _log('connect failed: $e');
      await _applyReconnectDecision(_reconnectPolicy.connectFailed());
    }
    _isConnecting = false;
  }

  void _startHeartbeat() {
    _heartbeatTimer?.cancel();
    if (_cmdChar == null) return;
    _heartbeatTimer = Timer.periodic(const Duration(seconds: 1), (_) async {
      if (_cmdChar == null) return;
      try {
        await _writeControlPoint('HEARTBEAT', withoutResponse: true);
      } catch (e) {
        _log('heartbeat failed: $e');
        await _applyReconnectDecision(_reconnectPolicy.connectFailed());
      }
    });
  }

  void _scheduleReconnect([Duration delay = const Duration(seconds: 3)]) {
    _reconnectTimer?.cancel();
    _reconnectTimer = Timer(delay, () async {
      await _scanAndConnect();
    });
  }

  Future<void> _applyReconnectDecision(
    BleReconnectDecision decision, {
    bool waitForScan = false,
  }) async {
    if (decision.isIdle) return;
    _log('reconnect decision: ${decision.reason}');
    if (decision.stopScan) {
      await FlutterBluePlus.stopScan();
      await _scanResultsSub?.cancel();
      _scanResultsSub = null;
    }
    if (decision.clearLink) {
      _heartbeatTimer?.cancel();
      await _connectionSub?.cancel();
      _connectionSub = null;
      await _dataSub?.cancel();
      _dataSub = null;
      await _logSub?.cancel();
      _logSub = null;
      _clearCharacteristics();
      final device = _device;
      _device = null;
      if (device != null) {
        try {
          await device.disconnect();
        } catch (_) {}
      }
    }
    if (decision.scanNow) {
      _reconnectTimer?.cancel();
      if (_hasUsableLink) {
        _log('scan skipped: link already active');
        return;
      }
      if (waitForScan) {
        await _scanAndConnect();
      } else {
        unawaited(_scanAndConnect());
      }
      return;
    }
    if (decision.scheduleRetry) {
      _scheduleReconnect(decision.retryDelay);
    }
  }

  Future<void> _watchAdapterState() async {
    if (!_usesRuntimeAdapterState) return;
    await _adapterStateSub?.cancel();
    _adapterStateSub = FlutterBluePlus.adapterState.listen((state) {
      _log('BLE adapter state: $state');
      final decision = _reconnectPolicy.adapterChanged(
        adapterReady: isAdapterReady(state),
      );
      unawaited(_applyReconnectDecision(decision));
    });
  }

  Future<bool> _readAdapterReady() async {
    if (!_usesRuntimeAdapterState) return true;
    try {
      final state = await FlutterBluePlus.adapterState.first.timeout(
        const Duration(seconds: 2),
      );
      _log('BLE adapter initial state: $state');
      return isAdapterReady(state);
    } catch (e) {
      _log('BLE adapter state unavailable, trying scan: $e');
      return true;
    }
  }

  bool get _usesRuntimeAdapterState {
    return Platform.isAndroid || Platform.isIOS || Platform.isMacOS;
  }

  bool get _hasUsableLink {
    return _device != null && _dataChar != null && _cmdChar != null;
  }

  static bool isAdapterReady(BluetoothAdapterState state) {
    return state == BluetoothAdapterState.on;
  }

  Future<void> requestSnapshot() async {
    await _writeControlPoint('SNAPSHOT');
  }

  Future<void> _setStreamEnabled(bool enabled) async {
    final cmd = enabled ? 'STREAM_START' : 'STREAM_STOP';
    await _writeControlPoint(cmd);
  }

  void _onNotify(List<int> value) async {
    try {
      _lastRssi = await _device?.readRssi() ?? _lastRssi;
    } catch (_) {}

    final frame = decodeBoatLockLiveFrame(value, rssi: _lastRssi);
    if (frame == null) {
      return;
    }

    _lastData = frame.data;
    _secPaired = frame.data.secPaired;
    _secAuth = frame.data.secAuth;
    _secPairWindowOpen = frame.data.secPairWindowOpen;
    _secNonceHex = frame.secNonceHex;
    _secReject = frame.data.secReject;
    if (!_secPaired) {
      _secAuth = false;
      _secCounter = 0;
    }

    onData(_lastData);
    final now = DateTime.now();
    if (_lastDataLogAt == null ||
        now.difference(_lastDataLogAt!).inSeconds >= 2) {
      _lastDataLogAt = now;
      _log(
        'data seq=${frame.sequence} mode=${_lastData!.mode} '
        'lat=${_lastData!.lat.toStringAsFixed(6)} lon=${_lastData!.lon.toStringAsFixed(6)}',
      );
    }
  }

  void _onLogNotify(List<int> value) {
    final line = decodeLogLine(value);
    if (line.trim().isEmpty) return;
    onLog?.call(line);
  }

  static String decodeLogLine(List<int> value) {
    final nul = value.indexOf(0);
    final end = nul < 0 ? value.length : nul;
    return utf8.decode(value.sublist(0, end), allowMalformed: true);
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

  Future<void> nudgeDir(String direction) async {
    if (_cmdChar == null) return;
    final cmd = buildNudgeDirCommand(direction);
    if (cmd == null) return;
    await _writeCommand(cmd);
  }

  Future<void> nudgeBearing(double bearingDeg) async {
    if (_cmdChar == null) return;
    final cmd = buildNudgeBearingCommand(bearingDeg);
    if (cmd == null) return;
    await _writeCommand(cmd);
  }

  Future<bool> setAnchorProfile(String profile) async {
    if (_cmdChar == null) return false;
    final cmd = buildSetAnchorProfileCommand(profile);
    if (cmd == null) return false;
    return _writeCommand(cmd);
  }

  Future<void> setStepperBowZero() async {
    if (_cmdChar != null) {
      await _writeCommand('SET_STEPPER_BOW');
    }
  }

  Future<bool> sendManualControl({
    required int steer,
    required int throttlePct,
    int ttlMs = 500,
  }) async {
    final cmd = buildManualSetCommand(
      steer: steer,
      throttlePct: throttlePct,
      ttlMs: ttlMs,
    );
    if (cmd == null) return false;
    return _writeCommand(cmd);
  }

  Future<bool> manualOff() async {
    return _writeCommand('MANUAL_OFF');
  }

  Future<bool> setHoldHeading(bool enabled) async {
    return _writeCommand('SET_HOLD_HEADING:${enabled ? 1 : 0}');
  }

  Future<bool> setCompassOffset(double degrees) async {
    if (!degrees.isFinite) return false;
    return _writeCommand('SET_COMPASS_OFFSET:${degrees.toStringAsFixed(1)}');
  }

  Future<bool> resetCompassOffset() async {
    return _writeCommand('RESET_COMPASS_OFFSET');
  }

  Future<bool> setStepMaxSpeed(double value) async {
    if (!value.isFinite) return false;
    return _writeCommand('SET_STEP_MAXSPD:${value.round()}');
  }

  Future<bool> setStepAccel(double value) async {
    if (!value.isFinite) return false;
    return _writeCommand('SET_STEP_ACCEL:${value.round()}');
  }

  void setOwnerSecret(String? secret) {
    if (secret == null) {
      _ownerSecret = null;
      return;
    }
    _ownerSecret = normalizeOwnerSecret(secret);
  }

  String generateOwnerSecret() {
    final random = Random.secure();
    final bytes = List<int>.generate(16, (_) => random.nextInt(256));
    final secret = bytes
        .map((b) => b.toRadixString(16).padLeft(2, '0'))
        .join()
        .toUpperCase();
    _ownerSecret = secret;
    return secret;
  }

  Future<bool> pairWithOwnerSecret([String? secret]) async {
    if (_cmdChar == null) return false;
    final normalized = normalizeOwnerSecret(secret ?? _ownerSecret);
    if (normalized == null) return false;
    _ownerSecret = normalized;
    final wrote = await _writeControlPoint('PAIR_SET:$normalized');
    if (!wrote) return false;
    await Future.delayed(const Duration(milliseconds: 120));
    await requestSnapshot();
    await Future.delayed(const Duration(milliseconds: 120));
    return _secPaired;
  }

  Future<bool> authenticateOwner([String? secret]) async {
    setOwnerSecret(secret ?? _ownerSecret);
    final ok = await _ensureSecuritySession(forceRenew: true);
    await requestSnapshot();
    return ok && _secAuth;
  }

  Future<bool> clearPairing() async {
    if (_cmdChar == null) return false;
    if (_secPaired) {
      final ok = await _writeCommand('PAIR_CLEAR');
      if (!ok) return false;
    } else {
      final wrote = await _writeControlPoint('PAIR_CLEAR');
      if (!wrote) return false;
    }
    await Future.delayed(const Duration(milliseconds: 120));
    await requestSnapshot();
    _secCounter = 0;
    if (!_secPaired) {
      _secAuth = false;
      _secNonceHex = '0000000000000000';
    }
    return !_secPaired;
  }

  Future<bool> _ensureSecuritySession({bool forceRenew = false}) async {
    if (_cmdChar == null) return false;
    if (!_secPaired) return true;
    if (!forceRenew && _secAuth) return true;
    final secret = _ownerSecret;
    if (secret == null) return false;

    final helloSent = await _writeControlPoint('AUTH_HELLO');
    if (!helloSent) return false;
    await Future.delayed(const Duration(milliseconds: 120));
    await requestSnapshot();
    await Future.delayed(const Duration(milliseconds: 120));
    if (_secNonceHex == '0000000000000000') return false;

    final proof = buildAuthProofHex(secret, _secNonceHex);
    final proofSent = await _writeControlPoint('AUTH_PROVE:$proof');
    if (!proofSent) return false;
    await Future.delayed(const Duration(milliseconds: 120));
    await requestSnapshot();
    await Future.delayed(const Duration(milliseconds: 120));
    if (!_secAuth) return false;

    _secCounter = 0;
    return true;
  }

  Future<bool> _writeCommand(String cmd) async {
    if (_cmdChar == null) return false;
    if (_isPlainAllowed(cmd)) {
      return _writeControlPoint(cmd);
    }

    final ok = await _ensureSecuritySession();
    if (!ok) {
      _log('secure session unavailable for "$cmd" reject=$_secReject');
      return false;
    }
    if (!_secPaired) {
      return _writeControlPoint(cmd);
    }

    _secCounter += 1;
    final secure = buildSecureCommand(
      ownerSecret: _ownerSecret!,
      nonceHex: _secNonceHex,
      payload: cmd,
      counter: _secCounter,
    );
    return _writeControlPoint(secure);
  }

  Future<bool> _writeControlPoint(
    String cmd, {
    bool withoutResponse = false,
  }) async {
    if (_cmdChar == null) return false;
    final bytes = encodeBoatLockCommandText(cmd);
    if (bytes == null) {
      _log(
        'control point write rejected reason=${validateBoatLockCommandText(cmd)}',
      );
      return false;
    }
    await _cmdChar!.write(bytes, withoutResponse: withoutResponse);
    return true;
  }

  bool _isPlainAllowed(String cmd) {
    return cmd == 'STOP' ||
        cmd == 'ANCHOR_OFF' ||
        cmd == 'HEARTBEAT' ||
        cmd == 'STREAM_START' ||
        cmd == 'STREAM_STOP' ||
        cmd == 'SNAPSHOT' ||
        cmd.startsWith('AUTH_') ||
        cmd.startsWith('PAIR_SET:') ||
        (cmd == 'PAIR_CLEAR' && (!_secPaired || _secPairWindowOpen)) ||
        cmd.startsWith('SEC_CMD:');
  }

  static String? normalizeOwnerSecret(String? raw) {
    if (raw == null) return null;
    final compact = raw.replaceAll(RegExp(r'[^0-9a-fA-F]'), '').toUpperCase();
    if (compact.length != 32) return null;
    return compact;
  }

  static String buildAuthProofHex(String ownerSecret, String nonceHex) {
    final secret = normalizeOwnerSecret(ownerSecret);
    final nonce = _normalizeNonceHex(nonceHex);
    if (secret == null || nonce == null) return '0000000000000000';
    final hash = _sipHash64(_hexToBytes(secret), utf8.encode('AUTH:$nonce'));
    return _hex64(hash);
  }

  static String buildSecureCommand({
    required String ownerSecret,
    required String nonceHex,
    required String payload,
    required int counter,
  }) {
    final secret = normalizeOwnerSecret(ownerSecret);
    final nonce = _normalizeNonceHex(nonceHex);
    if (secret == null || nonce == null || counter <= 0) {
      return 'SEC_CMD:00000000:0000000000000000:$payload';
    }
    final message = 'CMD:$nonce:${_hex32(counter)}:$payload';
    final hash = _sipHash64(_hexToBytes(secret), utf8.encode(message));
    return 'SEC_CMD:${_hex32(counter)}:${_hex64(hash)}:$payload';
  }

  static String? _normalizeNonceHex(String raw) {
    final compact = raw.replaceAll(RegExp(r'[^0-9a-fA-F]'), '').toUpperCase();
    if (compact.length != 16) return null;
    return compact;
  }

  static List<int> _hexToBytes(String hex) {
    final out = <int>[];
    for (var i = 0; i < hex.length; i += 2) {
      out.add(int.parse(hex.substring(i, i + 2), radix: 16));
    }
    return out;
  }

  static String _hex32(int value) {
    final v = value & 0xFFFFFFFF;
    return v.toRadixString(16).toUpperCase().padLeft(8, '0');
  }

  static String _hex64(int value) {
    final v = value & 0xFFFFFFFFFFFFFFFF;
    return v.toRadixString(16).toUpperCase().padLeft(16, '0');
  }

  static int _rotl64(int value, int shift) {
    final v = value & 0xFFFFFFFFFFFFFFFF;
    return ((v << shift) | (v >> (64 - shift))) & 0xFFFFFFFFFFFFFFFF;
  }

  static int _sipHash64(List<int> keyBytes, List<int> data) {
    if (keyBytes.length != 16) {
      return 0;
    }
    int read64(List<int> bytes, int offset) {
      var out = 0;
      for (var i = 0; i < 8; ++i) {
        out |= (bytes[offset + i] & 0xFF) << (i * 8);
      }
      return out & 0xFFFFFFFFFFFFFFFF;
    }

    void sipRound(List<int> state) {
      state[0] = (state[0] + state[1]) & 0xFFFFFFFFFFFFFFFF;
      state[1] = _rotl64(state[1], 13);
      state[1] ^= state[0];
      state[0] = _rotl64(state[0], 32);
      state[2] = (state[2] + state[3]) & 0xFFFFFFFFFFFFFFFF;
      state[3] = _rotl64(state[3], 16);
      state[3] ^= state[2];
      state[0] = (state[0] + state[3]) & 0xFFFFFFFFFFFFFFFF;
      state[3] = _rotl64(state[3], 21);
      state[3] ^= state[0];
      state[2] = (state[2] + state[1]) & 0xFFFFFFFFFFFFFFFF;
      state[1] = _rotl64(state[1], 17);
      state[1] ^= state[2];
      state[2] = _rotl64(state[2], 32);
    }

    final k0 = read64(keyBytes, 0);
    final k1 = read64(keyBytes, 8);
    final state = <int>[
      0x736f6d6570736575 ^ k0,
      0x646f72616e646f6d ^ k1,
      0x6c7967656e657261 ^ k0,
      0x7465646279746573 ^ k1,
    ];

    final blocks = data.length ~/ 8;
    for (var i = 0; i < blocks; ++i) {
      final m = read64(data, i * 8);
      state[3] ^= m;
      sipRound(state);
      sipRound(state);
      state[0] ^= m;
    }

    var b = (data.length & 0xFF) << 56;
    final tailOffset = blocks * 8;
    for (var i = data.length - tailOffset; i > 0; --i) {
      b |= (data[tailOffset + i - 1] & 0xFF) << ((i - 1) * 8);
    }

    state[3] ^= b;
    sipRound(state);
    sipRound(state);
    state[0] ^= b;
    state[2] ^= 0xFF;
    sipRound(state);
    sipRound(state);
    sipRound(state);
    sipRound(state);
    return (state[0] ^ state[1] ^ state[2] ^ state[3]) & 0xFFFFFFFFFFFFFFFF;
  }

  void dispose() {
    _isDisposed = true;
    WidgetsBinding.instance.removeObserver(this);
    unawaited(_applyReconnectDecision(_reconnectPolicy.dispose()));
    _scanResultsSub?.cancel();
    _adapterStateSub?.cancel();
    _connectionSub?.cancel();
    _dataSub?.cancel();
    _logSub?.cancel();
    _reconnectTimer?.cancel();
    _heartbeatTimer?.cancel();
    _device?.disconnect();
    _clearCharacteristics();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state != AppLifecycleState.resumed) return;
    unawaited(_applyReconnectDecision(_reconnectPolicy.appResumed()));
  }

  void _clearCharacteristics() {
    _dataChar = null;
    _cmdChar = null;
    _logChar = null;
    _lastData = null;
    _secAuth = false;
    _secPairWindowOpen = false;
    _secCounter = 0;
    _secNonceHex = '0000000000000000';
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
