import 'dart:async';
import 'dart:developer' as developer;
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter/widgets.dart';
import 'package:permission_handler/permission_handler.dart';
import '../models/boat_data.dart';
import 'ble_command_rejection.dart';
import 'ble_debug_snapshot.dart';
import 'ble_command_scope.dart';
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
import 'ble_security_policy.dart';

typedef BoatDataCallback = void Function(BoatData? data);
typedef LogCallback = void Function(String line);
typedef OtaProgressCallback = void Function(int sentBytes, int totalBytes);

const int _boatLockOtaDesiredMtu = 247;
const int _boatLockControlDesiredMtu = 96;
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

class _BoatLockDiscovery {
  const _BoatLockDiscovery({
    required this.dataFound,
    required this.commandFound,
    required this.logFound,
    required this.otaFound,
  });

  final bool dataFound;
  final bool commandFound;
  final bool logFound;
  final bool otaFound;

  bool get complete => boatLockDiscoveryComplete(
    dataFound: dataFound,
    commandFound: commandFound,
    logFound: logFound,
    otaFound: otaFound,
  );

  String get missing => describeMissingBoatLockCharacteristics(
    dataFound: dataFound,
    commandFound: commandFound,
    logFound: logFound,
    otaFound: otaFound,
  );
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
  final ValueNotifier<BleDebugSnapshot> diagnostics =
      ValueNotifier<BleDebugSnapshot>(BleDebugSnapshot.initial());
  final List<String> _debugAppLogs = <String>[];
  final List<String> _debugDeviceLogs = <String>[];
  final List<BleCommandRejection> _debugCommandRejects =
      <BleCommandRejection>[];

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
  bool _otaFinishWriteAccepted = false;
  bool _otaFinishDisconnectSeen = false;

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
    _setDiagnostics(
      connectionState: 'disconnected',
      deviceId: '',
      deviceName: '',
      isScanning: false,
      clearConnectedAt: true,
      lastError: '',
    );
  }

  Future<bool> sendCustomCommand(
    String cmd, {
    bool allowService = false,
    bool allowDevHil = false,
  }) async {
    final scope = classifyBoatLockCommand(cmd);
    if (scope == BoatLockCommandScope.unknown) {
      _log('custom command rejected scope=unknown command="$cmd"');
      return false;
    }
    if (scope == BoatLockCommandScope.service &&
        !allowService &&
        !kBoatLockServiceUiEnabled) {
      _log('custom command rejected scope=service command="$cmd"');
      return false;
    }
    if (scope == BoatLockCommandScope.devHil &&
        !allowDevHil &&
        !kBoatLockDevHilCommandsEnabled) {
      _log('custom command rejected scope=devHil command="$cmd"');
      return false;
    }
    return _writeCommand(cmd);
  }

  Future<void> sendPhoneGps(
    double lat,
    double lon, {
    double speedKmh = 0.0,
    int? satellites,
    bool allowDevHil = false,
  }) async {
    if (_cmdChar == null) return;
    if (!allowDevHil && !kBoatLockDevHilCommandsEnabled) {
      _log('phone GPS command rejected scope=devHil');
      return;
    }
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
    _setDiagnostics(isScanning: true, lastError: '');

    await FlutterBluePlus.stopScan();
    await _scanResultsSub?.cancel();
    _scanResultsSub = null;

    ScanResult? foundResult;
    final scanDone = Completer<ScanResult>();
    _scanResultsSub = FlutterBluePlus.scanResults.listen((results) {
      if (scanDone.isCompleted) return;
      for (var r in results) {
        _log(
          "found device: mac=${r.device.remoteId}, rssi=${r.rssi}, name='${r.device.platformName}', advName='${r.advertisementData.advName}'",
        );
        if (isBoatLockScanResult(r)) {
          scanDone.complete(r);
          return;
        }
      }
    });

    try {
      _log('scan start: unfiltered name_or_service_match');
      await FlutterBluePlus.startScan(timeout: kBoatLockScanTimeout);
      foundResult = await Future.any<ScanResult?>([
        scanDone.future,
        Future.delayed(kBoatLockScanCompletionWait, () => null),
      ]);
    } catch (e) {
      _log('scan failed: $e');
      _setDiagnostics(lastError: e.toString());
    } finally {
      await FlutterBluePlus.stopScan();
      await _scanResultsSub?.cancel();
      _scanResultsSub = null;
      _isConnecting = false;
      _setDiagnostics(isScanning: false);
    }

    if (foundResult == null) {
      _log('BoatLock not found, retry scan in 3s');
      await _applyReconnectDecision(_reconnectPolicy.scanMiss());
      return;
    }

    final r = foundResult;
    _log('BoatLock found, connecting...');
    _device = r.device;
    _setDiagnostics(
      deviceId: r.device.remoteId.toString(),
      deviceName: r.device.platformName.isNotEmpty
          ? r.device.platformName
          : r.advertisementData.advName,
    );
    await _connectToDevice();
  }

  Future<void> _connectToDevice() async {
    _log('_connectToDevice() called');
    if (_device == null) {
      _log('_device is null');
      return;
    }
    _isConnecting = true;

    try {
      await _device!.connect(
        license: License.free,
        timeout: const Duration(seconds: 20),
      );
      _log('Connected to device');
      _setDiagnostics(
        connectionState: 'connected',
        connectedAt: DateTime.now(),
        mtu: _device?.mtuNow ?? 0,
        lastError: '',
      );
      _connectionSub?.cancel();
      _connectionSub = _device!.connectionState.listen((state) {
        _log('BLE state: $state');
        _setDiagnostics(connectionState: state.name);
        if (state == BluetoothConnectionState.disconnected) {
          _handleOtaFinishDisconnect();
          unawaited(_applyReconnectDecision(_reconnectPolicy.disconnected()));
        }
      });

      _log('Discovering services...');
      var services = await _device!.discoverServices();
      _log('Services discovered: ${services.length}');
      var discovery = await _bindBoatLockCharacteristics(services);
      if (!discovery.complete && !discovery.otaFound) {
        final refreshed = await _rediscoverAfterMissingOtaCharacteristic();
        if (refreshed != null) {
          discovery = refreshed;
        }
      }
      if (!discovery.complete) {
        _log('BoatLock characteristics missing ${discovery.missing}');
        await _applyReconnectDecision(_reconnectPolicy.connectFailed());
        return;
      }
      _setDiagnostics(
        hasDataChar: discovery.dataFound,
        hasCommandChar: discovery.commandFound,
        hasLogChar: discovery.logFound,
        hasOtaChar: discovery.otaFound,
        mtu: _device?.mtuNow ?? diagnostics.value.mtu,
      );
      await _prepareControlTransport();
      await _setStreamEnabled(true);
      await requestSnapshot();
      _startHeartbeat();
    } catch (e) {
      _log('connect failed: $e');
      _setDiagnostics(lastError: e.toString());
      await _applyReconnectDecision(_reconnectPolicy.connectFailed());
    } finally {
      _isConnecting = false;
    }
  }

  Future<_BoatLockDiscovery> _bindBoatLockCharacteristics(
    List<BluetoothService> services,
  ) async {
    await _dataSub?.cancel();
    _dataSub = null;
    await _logSub?.cancel();
    _logSub = null;
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
        }
        if (isBoatLockUuid(uuid, boatLockCommandCharacteristicUuid)) {
          _cmdChar = c;
          commandFound = true;
        }
        if (isBoatLockUuid(uuid, boatLockLogCharacteristicUuid)) {
          _logChar = c;
          logFound = true;
        }
        if (isBoatLockUuid(uuid, boatLockOtaCharacteristicUuid)) {
          _otaChar = c;
          otaFound = true;
        }
      }
    }
    if (_dataChar != null) {
      await _dataChar!.setNotifyValue(true);
      _dataSub = _dataChar!.lastValueStream.listen(_onNotify);
    }
    if (_logChar != null) {
      await _logChar!.setNotifyValue(true);
      _logSub = _logChar!.lastValueStream.listen(_onLogNotify);
    }
    return _BoatLockDiscovery(
      dataFound: dataFound,
      commandFound: commandFound,
      logFound: logFound,
      otaFound: otaFound,
    );
  }

  Future<_BoatLockDiscovery?> _rediscoverAfterMissingOtaCharacteristic() async {
    if (kIsWeb || !Platform.isAndroid || _device == null) {
      return null;
    }
    _log(
      'BoatLock OTA characteristic missing; clearing Android GATT cache and rediscovering',
    );
    try {
      await _device!.clearGattCache();
      await Future<void>.delayed(const Duration(milliseconds: 250));
      final services = await _device!.discoverServices();
      _log('Services rediscovered: ${services.length}');
      return _bindBoatLockCharacteristics(services);
    } catch (e) {
      _log('Android GATT rediscovery failed: $e');
      return null;
    }
  }

  void _startHeartbeat() {
    _heartbeatTimer?.cancel();
    if (_cmdChar == null) return;
    _heartbeatTimer = Timer.periodic(const Duration(seconds: 1), (_) async {
      if (_cmdChar == null) return;
      try {
        final ok = await _writeCommand('HEARTBEAT');
        if (!ok) {
          throw StateError('secure heartbeat rejected');
        }
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
      _setDiagnostics(
        connectionState: 'disconnected',
        hasDataChar: false,
        hasCommandChar: false,
        hasLogChar: false,
        hasOtaChar: false,
        clearConnectedAt: true,
      );
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
      _setDiagnostics(adapterState: state.name);
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
      _setDiagnostics(adapterState: state.name);
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
    _setDiagnostics(
      dataEvents: diagnostics.value.dataEvents + 1,
      lastDataAt: now,
      rssi: _lastRssi,
      mtu: _device?.mtuNow ?? diagnostics.value.mtu,
    );
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
    final trimmed = line.trim();
    if (trimmed.isEmpty) return;
    _appendDeviceDebugLog(trimmed);
    final rejection = parseBleCommandRejectionLog(trimmed);
    if (rejection != null) {
      _appendCommandRejection(rejection);
    }
    _handleOtaLogLine(trimmed, rejection: rejection);
    onLog?.call(trimmed);
  }

  Future<bool> _waitForOtaAck(
    Completer<bool> completer,
    Duration timeout,
  ) async {
    return completer.future.timeout(timeout, onTimeout: () => false);
  }

  void _handleOtaLogLine(String line, {BleCommandRejection? rejection}) {
    if (_otaBeginAck != null && !_otaBeginAck!.isCompleted) {
      if (line.contains('[OTA] begin ok')) {
        _otaBeginAck!.complete(true);
      } else if (line.contains('[OTA] begin rejected')) {
        _otaBeginAck!.complete(false);
      } else if (rejection != null &&
          rejection.matchesCommandPrefix('OTA_BEGIN')) {
        _otaBeginAck!.complete(false);
      }
    }
    if (_otaFinishAck != null && !_otaFinishAck!.isCompleted) {
      if (line.contains('[OTA] finish ok')) {
        _otaFinishAck!.complete(true);
      } else if (line.contains('[OTA] finish rejected')) {
        _otaFinishAck!.complete(false);
      } else if (rejection != null &&
          rejection.matchesCommandPrefix('OTA_FINISH')) {
        _otaFinishAck!.complete(false);
      }
    }
  }

  void _handleOtaFinishDisconnect() {
    if (!_otaFinishRequested) return;
    _otaFinishDisconnectSeen = true;
    if (_otaFinishWriteAccepted &&
        _otaFinishAck != null &&
        !_otaFinishAck!.isCompleted) {
      _log('BLE OTA finish accepted by post-finish disconnect');
      _otaFinishAck!.complete(true);
    }
  }

  Future<bool> setAnchor() async {
    if (_cmdChar == null) return false;
    final cmd = buildSetAnchorCommand(_lastData);
    if (cmd == null) return false;
    return _writeCommand(cmd);
  }

  Future<bool> setAnchorAt(double lat, double lon) async {
    if (_cmdChar == null) return false;
    final cmd = buildSetAnchorCommandFromCoords(lat, lon);
    if (cmd == null) return false;
    return _writeCommand(cmd);
  }

  Future<bool> anchorOn() async {
    return _writeCommand('ANCHOR_ON');
  }

  Future<bool> anchorOff() async {
    return _writeCommand('ANCHOR_OFF');
  }

  Future<bool> stopAll() async {
    return _writeCommand('STOP');
  }

  Future<bool> nudgeDir(String direction) async {
    if (_cmdChar == null) return false;
    final cmd = buildNudgeDirCommand(direction);
    if (cmd == null) return false;
    return _writeCommand(cmd);
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
    required double angleDeg,
    required int throttlePct,
    int ttlMs = 1000,
  }) async {
    final cmd = buildManualTargetCommand(
      angleDeg: angleDeg,
      throttlePct: throttlePct,
      ttlMs: ttlMs,
    );
    if (cmd == null) return false;
    return _writeCommand(cmd, withoutResponse: true);
  }

  Future<bool> manualOff() async {
    return _writeCommand('MANUAL_OFF');
  }

  Future<void> _prepareControlTransport() async {
    if (kIsWeb || !Platform.isAndroid || _device == null) return;
    final mtuNow = _device!.mtuNow;
    if (mtuNow >= _boatLockControlDesiredMtu) return;
    try {
      final mtu = await _device!.requestMtu(
        _boatLockControlDesiredMtu,
        predelay: 0,
        timeout: 5,
      );
      _setDiagnostics(mtu: mtu);
      _log('BLE control MTU negotiated=$mtu');
    } catch (e) {
      final mtu = _device?.mtuNow ?? mtuNow;
      _setDiagnostics(mtu: mtu, lastError: e.toString());
      _log('BLE control MTU request failed, using mtu=$mtu: $e');
    }
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
    if (_cmdChar == null) {
      _log('BLE OTA unavailable reason=missing_command_char');
      return false;
    }
    if (_otaChar == null) {
      _log('BLE OTA unavailable reason=missing_ota_char');
      return false;
    }
    if (sha == null) {
      _log('BLE OTA unavailable reason=bad_sha256');
      return false;
    }
    if (!isBoatLockOtaImageSizeValid(firmware.length)) {
      _log('BLE OTA unavailable reason=bad_size size=${firmware.length}');
      return false;
    }

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
      final beginSent = await sendCustomCommand(
        'OTA_BEGIN:${firmware.length},$sha',
        allowService: true,
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
      _otaFinishWriteAccepted = false;
      _otaFinishDisconnectSeen = false;
      _log('BLE OTA finish requested sent=$sent total=${firmware.length}');
      finishRequested = await sendCustomCommand(
        'OTA_FINISH',
        allowService: true,
      );
      _otaFinishWriteAccepted = finishRequested;
      if (!finishRequested) {
        _log('BLE OTA finish write returned false, waiting for firmware ack');
      } else if (_otaFinishDisconnectSeen &&
          _otaFinishAck != null &&
          !_otaFinishAck!.isCompleted) {
        _log('BLE OTA finish accepted by earlier post-finish disconnect');
        _otaFinishAck!.complete(true);
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
          await sendCustomCommand('OTA_ABORT', allowService: true);
        } catch (_) {}
      }
      if (transport.restoreBalancedPriority && !finishAccepted) {
        await _restoreBalancedConnectionPriority();
      }
      _otaBeginAck = null;
      _otaFinishAck = null;
      _otaFinishRequested = false;
      _otaFinishWriteAccepted = false;
      _otaFinishDisconnectSeen = false;
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
        _setDiagnostics(mtu: mtu);
        _log('BLE OTA MTU negotiated=$mtu');
      } catch (e) {
        mtu = _device?.mtuNow ?? mtu;
        _setDiagnostics(mtu: mtu, lastError: e.toString());
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

  Future<bool> _writeCommand(String cmd, {bool withoutResponse = false}) async {
    if (_cmdChar == null) return false;
    if (_isPlainAllowed(cmd)) {
      return _writeControlPoint(cmd, withoutResponse: withoutResponse);
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
    return _writeControlPoint(secure, withoutResponse: withoutResponse);
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
    final mtu = _device?.mtuNow ?? 23;
    final safeWithoutResponse =
        withoutResponse &&
        _cmdChar!.properties.writeWithoutResponse &&
        bytes.length <= mtu - 3;
    await _cmdChar!.write(bytes, withoutResponse: safeWithoutResponse);
    return true;
  }

  bool _isPlainAllowed(String cmd) {
    return boatLockAllowsPlainCommand(
      command: cmd,
      paired: _secPaired,
      pairWindowOpen: _secPairWindowOpen,
    );
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
    _setDiagnostics(
      hasDataChar: false,
      hasCommandChar: false,
      hasLogChar: false,
      hasOtaChar: false,
      clearLastDataAt: true,
    );
    _clearLastData();
    _secAuth = false;
    _secPairWindowOpen = false;
    _secCounter = 0;
    _secNonceHex = '0000000000000000';
    if (_otaBeginAck != null && !_otaBeginAck!.isCompleted) {
      _otaBeginAck!.complete(false);
    }
    if (_otaFinishAck != null && !_otaFinishAck!.isCompleted) {
      if (_otaFinishRequested) {
        _handleOtaFinishDisconnect();
      } else {
        _otaFinishAck!.complete(false);
      }
    }
    _otaBeginAck = null;
    if (!_otaFinishRequested) {
      _otaFinishAck = null;
      _otaFinishWriteAccepted = false;
      _otaFinishDisconnectSeen = false;
    }
    _rssiThrottle.reset();
  }

  void _clearLastData() {
    final hadData = _lastData != null;
    _lastData = null;
    _lastDataLogAt = null;
    if (hadData && !_isDisposed) {
      onData(null);
    }
  }

  void _log(String msg) {
    debugPrint('[BleBoatLock] $msg');
    developer.log(msg, name: 'BleBoatLock');
    _appendAppDebugLog(msg);
  }

  void _appendAppDebugLog(String line) {
    final now = DateTime.now();
    _debugAppLogs.add(line);
    if (_debugAppLogs.length > 80) {
      _debugAppLogs.removeRange(0, _debugAppLogs.length - 80);
    }
    _setDiagnostics(
      appLogEvents: diagnostics.value.appLogEvents + 1,
      lastAppLogAt: now,
      appLogLines: List<String>.unmodifiable(_debugAppLogs),
    );
  }

  void _appendDeviceDebugLog(String line) {
    final now = DateTime.now();
    _debugDeviceLogs.add(line);
    if (_debugDeviceLogs.length > 80) {
      _debugDeviceLogs.removeRange(0, _debugDeviceLogs.length - 80);
    }
    _setDiagnostics(
      deviceLogEvents: diagnostics.value.deviceLogEvents + 1,
      lastDeviceLogAt: now,
      deviceLogLines: List<String>.unmodifiable(_debugDeviceLogs),
    );
  }

  void _appendCommandRejection(BleCommandRejection rejection) {
    _debugCommandRejects.add(rejection);
    if (_debugCommandRejects.length > 40) {
      _debugCommandRejects.removeRange(0, _debugCommandRejects.length - 40);
    }
    _setDiagnostics(
      commandRejectEvents: diagnostics.value.commandRejectEvents + 1,
      lastCommandRejection: rejection,
      commandRejects: List<BleCommandRejection>.unmodifiable(
        _debugCommandRejects,
      ),
    );
  }

  void _setDiagnostics({
    String? adapterState,
    String? connectionState,
    String? deviceId,
    String? deviceName,
    int? mtu,
    int? rssi,
    bool? isScanning,
    bool? hasDataChar,
    bool? hasCommandChar,
    bool? hasLogChar,
    bool? hasOtaChar,
    int? dataEvents,
    int? deviceLogEvents,
    int? commandRejectEvents,
    int? appLogEvents,
    DateTime? connectedAt,
    bool clearConnectedAt = false,
    DateTime? lastDataAt,
    bool clearLastDataAt = false,
    DateTime? lastDeviceLogAt,
    bool clearLastDeviceLogAt = false,
    DateTime? lastAppLogAt,
    bool clearLastAppLogAt = false,
    String? lastError,
    BleCommandRejection? lastCommandRejection,
    bool clearLastCommandRejection = false,
    List<String>? appLogLines,
    List<String>? deviceLogLines,
    List<BleCommandRejection>? commandRejects,
  }) {
    diagnostics.value = diagnostics.value.copyWith(
      adapterState: adapterState,
      connectionState: connectionState,
      deviceId: deviceId,
      deviceName: deviceName,
      mtu: mtu,
      rssi: rssi,
      isScanning: isScanning,
      hasDataChar: hasDataChar,
      hasCommandChar: hasCommandChar,
      hasLogChar: hasLogChar,
      hasOtaChar: hasOtaChar,
      dataEvents: dataEvents,
      deviceLogEvents: deviceLogEvents,
      commandRejectEvents: commandRejectEvents,
      appLogEvents: appLogEvents,
      connectedAt: connectedAt,
      clearConnectedAt: clearConnectedAt,
      lastDataAt: lastDataAt,
      clearLastDataAt: clearLastDataAt,
      lastDeviceLogAt: lastDeviceLogAt,
      clearLastDeviceLogAt: clearLastDeviceLogAt,
      lastAppLogAt: lastAppLogAt,
      clearLastAppLogAt: clearLastAppLogAt,
      lastError: lastError,
      lastCommandRejection: lastCommandRejection,
      clearLastCommandRejection: clearLastCommandRejection,
      appLogLines: appLogLines,
      deviceLogLines: deviceLogLines,
      commandRejects: commandRejects,
    );
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
