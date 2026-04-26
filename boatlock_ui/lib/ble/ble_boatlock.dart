import 'dart:async';
import 'dart:developer' as developer;
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter/widgets.dart';
import 'package:permission_handler/permission_handler.dart';
import '../models/boat_data.dart';
import 'ble_commands.dart';
import 'ble_command_text.dart';
import 'ble_device_match.dart';
import 'ble_discovery_check.dart';
import 'ble_ids.dart';
import 'ble_live_frame.dart';
import 'ble_log_line.dart';
import 'ble_ota_payload.dart';
import 'ble_reconnect_policy.dart';
import 'ble_rssi_throttle.dart';
import 'ble_scan_config.dart';
import 'ble_security_codec.dart';

typedef BoatDataCallback = void Function(BoatData? data);
typedef LogCallback = void Function(String line);
typedef OtaProgressCallback = void Function(int sentBytes, int totalBytes);

const int _boatLockOtaDesiredMtu = 247;
const int _boatLockOtaBackpressureWindowChunks = 8;
const Duration _boatLockOtaBackpressurePause = Duration(milliseconds: 12);

class _OtaTransport {
  const _OtaTransport({
    required this.chunkBytes,
    required this.withoutResponse,
    required this.restoreBalancedPriority,
  });

  final int chunkBytes;
  final bool withoutResponse;
  final bool restoreBalancedPriority;
}

class BleBoatLock with WidgetsBindingObserver {
  BluetoothDevice? _device;
  BluetoothCharacteristic? _dataChar;
  BluetoothCharacteristic? _cmdChar;
  BluetoothCharacteristic? _logChar;
  BluetoothCharacteristic? _otaChar;
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
  final BleRssiThrottle _rssiThrottle = BleRssiThrottle();
  bool _isConnecting = false;
  bool _isDisposed = false;
  DateTime? _lastDataLogAt;
  Completer<bool>? _otaBeginAck;
  Completer<bool>? _otaFinishAck;
  bool _otaFinishRequested = false;

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
        if (isBoatLockScanResult(r)) {
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
      await FlutterBluePlus.startScan(
        timeout: kBoatLockScanTimeout,
        withServices: boatLockScanServiceFilters(),
      );
      await Future.any([
        scanDone.future,
        Future.delayed(kBoatLockScanCompletionWait),
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
      var dataFound = false;
      var commandFound = false;
      var logFound = false;
      var otaFound = false;
      for (var s in services) {
        if (!isBoatLockUuid(s.uuid.toString(), boatLockServiceUuid)) {
          continue;
        }
        for (var c in s.characteristics) {
          final uuid = c.uuid.toString().toLowerCase();
          if (isBoatLockUuid(uuid, boatLockDataCharacteristicUuid)) {
            _dataChar = c;
            dataFound = true;
            await _dataChar!.setNotifyValue(true);
            _dataSub?.cancel();
            _dataSub = _dataChar!.lastValueStream.listen(_onNotify);
          }
          if (isBoatLockUuid(uuid, boatLockCommandCharacteristicUuid)) {
            _cmdChar = c;
            commandFound = true;
          }
          if (isBoatLockUuid(uuid, boatLockLogCharacteristicUuid)) {
            _logChar = c;
            logFound = true;
            await _logChar!.setNotifyValue(true);
            _logSub?.cancel();
            _logSub = _logChar!.lastValueStream.listen(_onLogNotify);
          }
          if (isBoatLockUuid(uuid, boatLockOtaCharacteristicUuid)) {
            _otaChar = c;
            otaFound = true;
          }
        }
      }
      if (!boatLockDiscoveryComplete(
        dataFound: dataFound,
        commandFound: commandFound,
        logFound: logFound,
      )) {
        _log(
          'BoatLock characteristics missing ${describeMissingBoatLockCharacteristics(dataFound: dataFound, commandFound: commandFound, logFound: logFound)}',
        );
        await _applyReconnectDecision(_reconnectPolicy.connectFailed());
        return;
      }
      if (!otaFound) {
        _log(
          'BoatLock OTA characteristic missing ota:$boatLockOtaCharacteristicUuid',
        );
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
        adapterReady: isBluetoothAdapterReady(state),
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
      return isBluetoothAdapterReady(state);
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

  Future<void> requestSnapshot() async {
    await _writeControlPoint('SNAPSHOT');
  }

  Future<void> _setStreamEnabled(bool enabled) async {
    final cmd = enabled ? 'STREAM_START' : 'STREAM_STOP';
    await _writeControlPoint(cmd);
  }

  void _onNotify(List<int> value) async {
    final now = DateTime.now();
    if (_rssiThrottle.shouldRead(now)) {
      try {
        _lastRssi = await _device?.readRssi() ?? _lastRssi;
      } catch (_) {}
    }

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
    final line = decodeBoatLockLogLine(value);
    if (line.trim().isEmpty) return;
    _handleOtaLogLine(line);
    onLog?.call(line);
  }

  Future<bool> _waitForOtaAck(
    Completer<bool> completer,
    Duration timeout,
  ) async {
    return completer.future.timeout(timeout, onTimeout: () => false);
  }

  void _handleOtaLogLine(String line) {
    if (_otaBeginAck != null && !_otaBeginAck!.isCompleted) {
      if (line.contains('[OTA] begin ok')) {
        _otaBeginAck!.complete(true);
      } else if (line.contains('[OTA] begin rejected')) {
        _otaBeginAck!.complete(false);
      }
    }
    if (_otaFinishAck != null && !_otaFinishAck!.isCompleted) {
      if (line.contains('[OTA] finish ok')) {
        _otaFinishAck!.complete(true);
      } else if (line.contains('[OTA] finish rejected')) {
        _otaFinishAck!.complete(false);
      }
    }
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

  Future<bool> uploadFirmwareOtaBytes({
    required List<int> firmware,
    required String sha256Hex,
    OtaProgressCallback? onProgress,
  }) async {
    final sha = normalizeBoatLockSha256Hex(sha256Hex);
    if (_cmdChar == null || _otaChar == null || sha == null) return false;
    if (!isBoatLockOtaImageSizeValid(firmware.length)) return false;

    _heartbeatTimer?.cancel();
    var beginAccepted = false;
    var finishRequested = false;
    var finishAccepted = false;
    var sent = 0;
    final transport = await _prepareOtaTransport();
    try {
      _log(
        'BLE OTA begin size=${firmware.length} sha=$sha '
        'chunk=${transport.chunkBytes} '
        'withoutResponse=${transport.withoutResponse} '
        'window=$_boatLockOtaBackpressureWindowChunks '
        'pauseMs=${_boatLockOtaBackpressurePause.inMilliseconds}',
      );
      _otaBeginAck = Completer<bool>();
      _otaFinishAck = null;
      final beginSent = await _writeCommand(
        'OTA_BEGIN:${firmware.length},$sha',
      );
      if (!beginSent) return false;
      beginAccepted = await _waitForOtaAck(
        _otaBeginAck!,
        const Duration(seconds: 20),
      );
      if (!beginAccepted) return false;

      onProgress?.call(0, firmware.length);
      var lastLoggedBucket = -1;
      var lastLoggedAt = DateTime.now();
      var chunksInWindow = 0;
      for (final chunk in boatLockOtaChunks(
        firmware,
        chunkBytes: transport.chunkBytes,
      )) {
        await _otaChar!.write(
          chunk,
          withoutResponse: transport.withoutResponse,
          timeout: transport.withoutResponse ? 5 : 15,
        );
        sent += chunk.length;
        chunksInWindow += 1;
        onProgress?.call(sent, firmware.length);
        final bucket = (100 * sent) ~/ firmware.length;
        final now = DateTime.now();
        if (bucket ~/ 5 != lastLoggedBucket ~/ 5 ||
            now.difference(lastLoggedAt) >= const Duration(seconds: 10) ||
            sent == firmware.length) {
          lastLoggedBucket = bucket;
          lastLoggedAt = now;
          _log(
            'BLE OTA progress sent=$sent total=${firmware.length} pct=$bucket',
          );
        }
        if (transport.withoutResponse &&
            chunksInWindow >= _boatLockOtaBackpressureWindowChunks &&
            sent < firmware.length) {
          chunksInWindow = 0;
          await Future<void>.delayed(_boatLockOtaBackpressurePause);
        }
      }

      _otaFinishAck = Completer<bool>();
      _otaFinishRequested = true;
      _log('BLE OTA finish requested sent=$sent total=${firmware.length}');
      finishRequested = await _writeCommand('OTA_FINISH');
      if (!finishRequested) {
        _log('BLE OTA finish write returned false, waiting for firmware ack');
      }
      finishAccepted = await _waitForOtaAck(
        _otaFinishAck!,
        const Duration(seconds: 45),
      );
      if (finishAccepted) {
        finishRequested = true;
      }
      _log('BLE OTA finish ack=$finishAccepted');
      return finishAccepted;
    } catch (e) {
      _log('BLE OTA failed sent=$sent total=${firmware.length}: $e');
      return false;
    } finally {
      if (beginAccepted && !finishRequested && _cmdChar != null) {
        try {
          _log('BLE OTA abort requested sent=$sent total=${firmware.length}');
          await _writeCommand('OTA_ABORT');
        } catch (_) {}
      }
      if (transport.restoreBalancedPriority && !finishAccepted) {
        await _restoreBalancedConnectionPriority();
      }
      _otaBeginAck = null;
      _otaFinishAck = null;
      _otaFinishRequested = false;
      if (!_isDisposed && _cmdChar != null) {
        _startHeartbeat();
      }
    }
  }

  Future<_OtaTransport> _prepareOtaTransport() async {
    var mtu = _device?.mtuNow ?? 23;
    var restoreBalancedPriority = false;
    if (!kIsWeb && Platform.isAndroid && _device != null) {
      try {
        await _device!.requestConnectionPriority(
          connectionPriorityRequest: ConnectionPriority.high,
        );
        restoreBalancedPriority = true;
        _log('BLE OTA connection priority requested=high');
      } catch (e) {
        _log('BLE OTA connection priority high request failed: $e');
      }
      try {
        mtu = await _device!.requestMtu(
          _boatLockOtaDesiredMtu,
          predelay: 0,
          timeout: 10,
        );
        _log('BLE OTA MTU negotiated=$mtu');
      } catch (e) {
        mtu = _device?.mtuNow ?? mtu;
        _log('BLE OTA MTU request failed, using mtu=$mtu: $e');
      }
    }
    final chunkBytes = boatLockOtaChunkBytesForMtu(mtu);
    final withoutResponse = _otaChar?.properties.writeWithoutResponse == true;
    if (!withoutResponse) {
      _log(
        'BLE OTA writeWithoutResponse unavailable, using acknowledged writes',
      );
    }
    return _OtaTransport(
      chunkBytes: chunkBytes,
      withoutResponse: withoutResponse,
      restoreBalancedPriority: restoreBalancedPriority,
    );
  }

  Future<void> _restoreBalancedConnectionPriority() async {
    if (kIsWeb ||
        !Platform.isAndroid ||
        _device == null ||
        _device!.isDisconnected) {
      return;
    }
    try {
      await _device!.requestConnectionPriority(
        connectionPriorityRequest: ConnectionPriority.balanced,
      );
      _log('BLE OTA connection priority restored=balanced');
    } catch (e) {
      _log('BLE OTA connection priority restore failed: $e');
    }
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
    final secret = generateOwnerSecretHex();
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
    _otaChar = null;
    _lastData = null;
    _secAuth = false;
    _secPairWindowOpen = false;
    _secCounter = 0;
    _secNonceHex = '0000000000000000';
    if (_otaBeginAck != null && !_otaBeginAck!.isCompleted) {
      _otaBeginAck!.complete(false);
    }
    if (_otaFinishAck != null &&
        !_otaFinishAck!.isCompleted &&
        !_otaFinishRequested) {
      _otaFinishAck!.complete(false);
    }
    _otaBeginAck = null;
    _otaFinishAck = null;
    _otaFinishRequested = false;
    _rssiThrottle.reset();
  }

  void _log(String msg) {
    debugPrint('[BleBoatLock] $msg');
    developer.log(msg, name: 'BleBoatLock');
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
