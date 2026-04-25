import 'dart:async';
import 'dart:convert';
import 'dart:developer' as developer;

import 'package:flutter/foundation.dart';

import '../ble/ble_boatlock.dart';
import '../models/boat_data.dart';
import '../smoke/ble_smoke_logic.dart';
import '../smoke/ble_smoke_mode.dart';

const kBoatLockAppE2eModeDefine = 'BOATLOCK_APP_E2E_MODE';
const kBoatLockAppE2eModeName = String.fromEnvironment(
  kBoatLockAppE2eModeDefine,
  defaultValue: '',
);

class BoatLockAppE2eProbe {
  BoatLockAppE2eProbe._({required BleBoatLock ble, required BleSmokeMode mode})
    : _ble = ble,
      _mode = mode {
    _log('starting_app_e2e mode=${_mode.name}');
    final timeout = _mode == BleSmokeMode.reconnect
        ? const Duration(seconds: 150)
        : const Duration(seconds: 75);
    _timeoutTimer = Timer(timeout, () {
      _finish(false, 'app_${_mode.name}_timeout');
    });
    if (_mode == BleSmokeMode.reconnect) {
      _gapTimer = Timer.periodic(const Duration(seconds: 1), (_) {
        _checkReconnectGap();
      });
    }
  }

  final BleBoatLock _ble;
  final BleSmokeMode _mode;
  Timer? _timeoutTimer;
  Timer? _gapTimer;
  BoatData? _lastData;
  DateTime? _lastDataAt;
  String? _lastDeviceLog;
  int _dataEvents = 0;
  int _deviceLogEvents = 0;
  bool _completed = false;
  bool _firstTelemetrySeen = false;
  bool _reconnectGapSeen = false;
  bool _manualSetSent = false;
  bool _manualModeSeen = false;
  bool _manualOffSent = false;
  bool _statusStopSent = false;
  bool _statusAlertSeen = false;
  bool _statusManualSetSent = false;
  bool _statusManualModeSeen = false;
  bool _statusManualOffSent = false;
  bool _simRunSent = false;
  bool _simModeSeen = false;
  bool _simAbortSent = false;
  bool _simManualSetSent = false;
  bool _simManualModeSeen = false;
  bool _simManualOffSent = false;
  bool _anchorOnSent = false;
  bool _anchorDeniedSeen = false;
  bool _anchorOffSent = false;

  static BoatLockAppE2eProbe? maybeCreate(BleBoatLock ble) {
    if (kBoatLockAppE2eModeName.isEmpty) {
      return null;
    }
    return BoatLockAppE2eProbe._(
      ble: ble,
      mode: boatLockSmokeModeFromString(kBoatLockAppE2eModeName),
    );
  }

  void onData(BoatData? data) {
    if (_completed || data == null) {
      return;
    }
    _lastData = data;
    _lastDataAt = DateTime.now();
    _dataEvents += 1;
    _log(
      'telemetry mode=${data.mode} status=${data.status} '
      'paired=${data.secPaired} auth=${data.secAuth} rssi=${data.rssi}',
    );
    if (!smokeTelemetryLooksHealthy(data)) {
      return;
    }

    switch (_mode) {
      case BleSmokeMode.basic:
        _finish(true, 'app_telemetry_received');
      case BleSmokeMode.reconnect:
        _handleReconnectData();
      case BleSmokeMode.manual:
        _handleManualData(data);
      case BleSmokeMode.status:
        _handleStatusData(data);
      case BleSmokeMode.sim:
        _handleSimData(data);
      case BleSmokeMode.anchor:
        _handleAnchorData(data);
    }
  }

  void onDeviceLog(String line) {
    if (_completed) {
      return;
    }
    _deviceLogEvents += 1;
    _lastDeviceLog = line.trim();
    _log('device_log $_lastDeviceLog');
    if (_mode == BleSmokeMode.anchor &&
        smokeAnchorDeniedLogSeen(_lastDeviceLog)) {
      _anchorDeniedSeen = true;
      _stage('anchor_denied_seen');
    }
  }

  void dispose() {
    _timeoutTimer?.cancel();
    _gapTimer?.cancel();
  }

  void _handleReconnectData() {
    if (!_firstTelemetrySeen) {
      _firstTelemetrySeen = true;
      _stage('first_telemetry');
      return;
    }
    if (_reconnectGapSeen) {
      _finish(true, 'app_telemetry_after_reconnect');
    }
  }

  void _handleManualData(BoatData data) {
    if (!_manualSetSent) {
      _manualSetSent = true;
      _stage('manual_set_zero');
      unawaited(_sendManualSet());
      return;
    }
    if (!_manualModeSeen) {
      if (data.mode != 'MANUAL') {
        return;
      }
      _manualModeSeen = true;
      _stage('manual_mode_seen');
      unawaited(_sendManualOff());
      return;
    }
    if (!_manualOffSent || data.mode == 'MANUAL') {
      return;
    }
    _finish(true, 'app_manual_roundtrip');
  }

  void _handleStatusData(BoatData data) {
    if (!_statusStopSent) {
      _statusStopSent = true;
      _stage('status_stop');
      unawaited(_sendStatusStop());
      return;
    }
    if (!_statusAlertSeen) {
      if (!smokeStatusStopAlertSeen(data)) {
        return;
      }
      _statusAlertSeen = true;
      _stage('status_stop_alert_seen');
      unawaited(_sendStatusManualSet());
      return;
    }
    if (!_statusManualModeSeen) {
      if (!_statusManualSetSent || data.mode != 'MANUAL') {
        return;
      }
      _statusManualModeSeen = true;
      _stage('status_manual_mode_seen');
      unawaited(_sendStatusManualOff());
      return;
    }
    if (!_statusManualOffSent || !smokeStatusRecoveredAfterStop(data)) {
      return;
    }
    _finish(true, 'app_status_stop_alert_roundtrip');
  }

  void _handleSimData(BoatData data) {
    if (!_simRunSent) {
      _simRunSent = true;
      _stage('sim_run');
      unawaited(_sendSimRun());
      return;
    }
    if (!_simModeSeen) {
      if (data.mode != 'SIM') {
        return;
      }
      _simModeSeen = true;
      _stage('sim_mode_seen');
      unawaited(_sendSimAbort());
      return;
    }
    if (!_simAbortSent || data.mode == 'SIM') {
      return;
    }
    if (!_simManualSetSent) {
      _stage('sim_manual_recovery');
      unawaited(_sendSimManualSet());
      return;
    }
    if (!_simManualModeSeen) {
      if (data.mode != 'MANUAL') {
        return;
      }
      _simManualModeSeen = true;
      _stage('sim_manual_mode_seen');
      unawaited(_sendSimManualOff());
      return;
    }
    if (!_simManualOffSent || !smokeStatusRecoveredAfterStop(data)) {
      return;
    }
    _finish(true, 'app_sim_run_abort_roundtrip');
  }

  void _handleAnchorData(BoatData data) {
    if (!_anchorOnSent) {
      _anchorOnSent = true;
      _stage('anchor_on_denied_probe');
      unawaited(_sendAnchorOn());
      return;
    }
    if (!_anchorDeniedSeen) {
      return;
    }
    if (!_anchorOffSent) {
      _anchorOffSent = true;
      _stage('anchor_off_cleanup');
      unawaited(_sendAnchorOff());
      return;
    }
    if (!smokeAnchorRejectedSafely(data)) {
      return;
    }
    _finish(true, 'app_anchor_denied_roundtrip');
  }

  Future<void> _sendManualSet() async {
    final ok = await _ble.sendManualControl(
      steer: 0,
      throttlePct: 0,
      ttlMs: 1000,
    );
    _log('manual_set_zero ok=$ok');
    if (!ok) {
      _finish(false, 'app_manual_set_failed');
    }
  }

  Future<void> _sendManualOff() async {
    final ok = await _ble.manualOff();
    _manualOffSent = ok;
    _log('manual_off ok=$ok');
    if (!ok) {
      _finish(false, 'app_manual_off_failed');
    }
  }

  Future<void> _sendStatusStop() async {
    await _ble.stopAll();
    _log('status_stop sent');
  }

  Future<void> _sendStatusManualSet() async {
    final ok = await _ble.sendManualControl(
      steer: 0,
      throttlePct: 0,
      ttlMs: 1000,
    );
    _statusManualSetSent = ok;
    _log('status_manual_set_zero ok=$ok');
    if (!ok) {
      _finish(false, 'app_status_manual_set_failed');
    }
  }

  Future<void> _sendStatusManualOff() async {
    final ok = await _ble.manualOff();
    _statusManualOffSent = ok;
    _log('status_manual_off ok=$ok');
    if (!ok) {
      _finish(false, 'app_status_manual_off_failed');
    }
  }

  Future<void> _sendSimRun() async {
    final ok = await _ble.sendCustomCommand('SIM_RUN:S0_hold_still_good,1');
    _log('sim_run ok=$ok');
    if (!ok) {
      _finish(false, 'app_sim_run_failed');
    }
  }

  Future<void> _sendSimAbort() async {
    final ok = await _ble.sendCustomCommand('SIM_ABORT');
    _simAbortSent = ok;
    _log('sim_abort ok=$ok');
    if (!ok) {
      _finish(false, 'app_sim_abort_failed');
    }
  }

  Future<void> _sendSimManualSet() async {
    final ok = await _ble.sendManualControl(
      steer: 0,
      throttlePct: 0,
      ttlMs: 1000,
    );
    _simManualSetSent = ok;
    _log('sim_manual_set_zero ok=$ok');
    if (!ok) {
      _finish(false, 'app_sim_manual_set_failed');
    }
  }

  Future<void> _sendSimManualOff() async {
    final ok = await _ble.manualOff();
    _simManualOffSent = ok;
    _log('sim_manual_off ok=$ok');
    if (!ok) {
      _finish(false, 'app_sim_manual_off_failed');
    }
  }

  Future<void> _sendAnchorOn() async {
    final ok = await _ble.sendCustomCommand('ANCHOR_ON');
    _log('anchor_on ok=$ok');
    if (!ok) {
      _finish(false, 'app_anchor_on_failed');
    }
  }

  Future<void> _sendAnchorOff() async {
    final ok = await _ble.sendCustomCommand('ANCHOR_OFF');
    _log('anchor_off ok=$ok');
    if (!ok) {
      _anchorOffSent = false;
      _finish(false, 'app_anchor_off_failed');
    }
  }

  void _checkReconnectGap() {
    if (_completed ||
        !_firstTelemetrySeen ||
        _reconnectGapSeen ||
        _lastDataAt == null) {
      return;
    }
    final gap = DateTime.now().difference(_lastDataAt!);
    if (gap < const Duration(seconds: 4)) {
      return;
    }
    _reconnectGapSeen = true;
    _stage('telemetry_gap');
  }

  void _stage(String stage) {
    _logLine(encodeSmokeStageLine(stage));
  }

  void _finish(bool pass, String reason) {
    if (_completed) {
      return;
    }
    _completed = true;
    _timeoutTimer?.cancel();
    _gapTimer?.cancel();
    final payload = buildSmokeResultPayload(
      pass: pass,
      reason: reason,
      dataEvents: _dataEvents,
      deviceLogEvents: _deviceLogEvents,
      data: _lastData,
      lastDeviceLog: _lastDeviceLog,
    );
    _logLine(encodeSmokeResultLine(payload));
    developer.log(jsonEncode(payload), name: 'BoatLockAppE2E');
  }

  void _log(String line) {
    _logLine('[BoatLockAppE2E] $line');
  }

  void _logLine(String line) {
    debugPrint(line);
    developer.log(line, name: 'BoatLockAppE2E');
  }
}
