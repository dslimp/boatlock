import 'dart:async';
import 'dart:convert';
import 'dart:developer' as developer;

import 'package:flutter/material.dart';

import '../ble/ble_boatlock.dart';
import '../models/boat_data.dart';
import 'ble_smoke_logic.dart';
import 'ble_smoke_mode.dart';

class BleSmokePage extends StatefulWidget {
  const BleSmokePage({super.key, this.mode = BleSmokeMode.basic});

  final BleSmokeMode mode;

  @override
  State<BleSmokePage> createState() => _BleSmokePageState();
}

class _BleSmokePageState extends State<BleSmokePage> {
  static const Duration _basicTimeout = Duration(seconds: 75);
  static const Duration _reconnectTimeout = Duration(seconds: 150);
  static const Duration _gpsTimeout = Duration(seconds: 180);
  static const Duration _reconnectGap = Duration(seconds: 4);

  late final BleBoatLock _ble;
  Timer? _timeoutTimer;
  Timer? _gapTimer;
  final List<String> _eventLines = <String>[];
  BoatData? _lastData;
  DateTime? _lastDataAt;
  String _phase = 'starting';
  String _result = 'RUNNING';
  String _detail = 'booting';
  int _dataEvents = 0;
  int _deviceLogEvents = 0;
  String? _lastDeviceLog;
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
  bool _compassCommandsSent = false;
  bool _compassCalLogSeen = false;
  bool _compassAutosaveLogSeen = false;
  bool _compassDcdSaveLogSeen = false;
  bool _gpsWaitingLogged = false;

  @override
  void initState() {
    super.initState();
    _ble = BleBoatLock(onData: _onData, onLog: _onDeviceLog);
    _start();
  }

  Future<void> _start() async {
    _appendEvent('starting_ble_smoke');
    final timeout = switch (widget.mode) {
      BleSmokeMode.reconnect => _reconnectTimeout,
      BleSmokeMode.gps => _gpsTimeout,
      _ => _basicTimeout,
    };
    _timeoutTimer = Timer(timeout, () {
      _finish(false, 'timeout_waiting_for_telemetry');
    });
    if (widget.mode == BleSmokeMode.reconnect) {
      _gapTimer = Timer.periodic(const Duration(seconds: 1), (_) {
        _checkReconnectGap();
      });
    }
    await _ble.connectAndListen();
    _appendEvent('connect_started');
    if (mounted) {
      setState(() {
        _phase = 'connecting';
        _detail = 'waiting_for_telemetry';
      });
    }
  }

  void _onData(BoatData? data) {
    if (data == null) {
      return;
    }
    _lastData = data;
    _lastDataAt = DateTime.now();
    _dataEvents += 1;
    _appendEvent(
      'telemetry mode=${data.mode} status=${data.status} '
      'paired=${data.secPaired} auth=${data.secAuth} rssi=${data.rssi} '
      'lat=${data.lat.toStringAsFixed(6)} lon=${data.lon.toStringAsFixed(6)} '
      'gnssQ=${data.gnssQ}',
    );
    if (!_completed && smokeTelemetryLooksHealthy(data)) {
      if (widget.mode == BleSmokeMode.basic) {
        _finish(true, 'telemetry_received');
        return;
      }
      if (widget.mode == BleSmokeMode.manual) {
        _handleManualSmokeData(data);
        return;
      }
      if (widget.mode == BleSmokeMode.status) {
        _handleStatusSmokeData(data);
        return;
      }
      if (widget.mode == BleSmokeMode.sim) {
        _handleSimSmokeData(data);
        return;
      }
      if (widget.mode == BleSmokeMode.anchor) {
        _handleAnchorSmokeData(data);
        return;
      }
      if (widget.mode == BleSmokeMode.compass) {
        _handleCompassSmokeData();
        return;
      }
      if (widget.mode == BleSmokeMode.gps) {
        _handleGpsSmokeData(data);
        return;
      }
      if (!_firstTelemetrySeen) {
        _firstTelemetrySeen = true;
        _appendEvent(encodeSmokeStageLine('first_telemetry'));
        if (mounted) {
          setState(() {
            _phase = 'reconnect';
            _detail = 'first_telemetry_waiting_for_gap';
          });
        }
        return;
      }
      if (_reconnectGapSeen) {
        _finish(true, 'telemetry_after_reconnect');
        return;
      }
      return;
    }
    if (mounted) {
      setState(() {
        _phase = 'telemetry';
        _detail = 'mode=${data.mode} status=${data.status}';
      });
    }
  }

  void _handleManualSmokeData(BoatData data) {
    if (!_manualSetSent) {
      _manualSetSent = true;
      _appendEvent(encodeSmokeStageLine('manual_set_zero'));
      _sendManualSet();
      if (mounted) {
        setState(() {
          _phase = 'manual';
          _detail = 'waiting_for_manual_mode';
        });
      }
      return;
    }
    if (!_manualModeSeen) {
      if (data.mode != 'MANUAL') {
        return;
      }
      _manualModeSeen = true;
      _appendEvent(encodeSmokeStageLine('manual_mode_seen'));
      _sendManualOff();
      if (mounted) {
        setState(() {
          _phase = 'manual';
          _detail = 'waiting_for_manual_exit';
        });
      }
      return;
    }
    if (!_manualOffSent || data.mode == 'MANUAL') {
      return;
    }
    _finish(true, 'manual_roundtrip');
  }

  void _handleStatusSmokeData(BoatData data) {
    if (!_statusStopSent) {
      _statusStopSent = true;
      _appendEvent(encodeSmokeStageLine('status_stop'));
      _sendStatusStop();
      if (mounted) {
        setState(() {
          _phase = 'status';
          _detail = 'waiting_for_stop_alert';
        });
      }
      return;
    }
    if (!_statusAlertSeen) {
      if (!smokeStatusStopAlertSeen(data)) {
        return;
      }
      _statusAlertSeen = true;
      _appendEvent(encodeSmokeStageLine('status_stop_alert_seen'));
      _sendStatusManualSet();
      if (mounted) {
        setState(() {
          _phase = 'status';
          _detail = 'waiting_for_manual_recovery';
        });
      }
      return;
    }
    if (!_statusManualModeSeen) {
      if (!_statusManualSetSent || data.mode != 'MANUAL') {
        return;
      }
      _statusManualModeSeen = true;
      _appendEvent(encodeSmokeStageLine('status_manual_mode_seen'));
      _sendStatusManualOff();
      if (mounted) {
        setState(() {
          _phase = 'status';
          _detail = 'waiting_for_alert_clear';
        });
      }
      return;
    }
    if (!_statusManualOffSent || !smokeStatusRecoveredAfterStop(data)) {
      return;
    }
    _finish(true, 'status_stop_alert_roundtrip');
  }

  void _handleSimSmokeData(BoatData data) {
    if (!_simRunSent) {
      _simRunSent = true;
      _appendEvent(encodeSmokeStageLine('sim_run'));
      _sendSimRun();
      if (mounted) {
        setState(() {
          _phase = 'sim';
          _detail = 'waiting_for_sim_mode';
        });
      }
      return;
    }
    if (!_simModeSeen) {
      if (data.mode != 'SIM') {
        return;
      }
      _simModeSeen = true;
      _appendEvent(encodeSmokeStageLine('sim_mode_seen'));
      _sendSimAbort();
      if (mounted) {
        setState(() {
          _phase = 'sim';
          _detail = 'waiting_for_sim_abort';
        });
      }
      return;
    }
    if (!_simAbortSent || data.mode == 'SIM') {
      return;
    }
    if (!_simManualSetSent) {
      _appendEvent(encodeSmokeStageLine('sim_manual_recovery'));
      _sendSimManualSet();
      if (mounted) {
        setState(() {
          _phase = 'sim';
          _detail = 'waiting_for_manual_recovery';
        });
      }
      return;
    }
    if (!_simManualModeSeen) {
      if (data.mode != 'MANUAL') {
        return;
      }
      _simManualModeSeen = true;
      _appendEvent(encodeSmokeStageLine('sim_manual_mode_seen'));
      _sendSimManualOff();
      if (mounted) {
        setState(() {
          _phase = 'sim';
          _detail = 'waiting_for_recovered_idle';
        });
      }
      return;
    }
    if (!_simManualOffSent || !smokeStatusRecoveredAfterStop(data)) {
      return;
    }
    _finish(true, 'sim_run_abort_roundtrip');
  }

  void _handleAnchorSmokeData(BoatData data) {
    if (!_anchorOnSent) {
      _anchorOnSent = true;
      _appendEvent(encodeSmokeStageLine('anchor_on_denied_probe'));
      _sendAnchorOn();
      if (mounted) {
        setState(() {
          _phase = 'anchor';
          _detail = 'waiting_for_anchor_denied';
        });
      }
      return;
    }
    if (!_anchorDeniedSeen) {
      return;
    }
    if (!_anchorOffSent) {
      _anchorOffSent = true;
      _appendEvent(encodeSmokeStageLine('anchor_off_cleanup'));
      _sendAnchorOff();
      if (mounted) {
        setState(() {
          _phase = 'anchor';
          _detail = 'waiting_for_safe_idle';
        });
      }
      return;
    }
    if (!smokeAnchorRejectedSafely(data)) {
      return;
    }
    _finish(true, 'anchor_denied_roundtrip');
  }

  void _handleCompassSmokeData() {
    if (!_compassCommandsSent) {
      _compassCommandsSent = true;
      _appendEvent(encodeSmokeStageLine('compass_commands'));
      _sendCompassCommands();
      if (mounted) {
        setState(() {
          _phase = 'compass';
          _detail = 'waiting_for_compass_command_logs';
        });
      }
      return;
    }
    _finishCompassIfReady();
  }

  void _handleGpsSmokeData(BoatData data) {
    if (smokeGpsFixLooksHealthy(data)) {
      _finish(true, 'gps_fix_received');
      return;
    }
    if (!_gpsWaitingLogged) {
      _gpsWaitingLogged = true;
      _appendEvent(encodeSmokeStageLine('gps_waiting_fix'));
      if (mounted) {
        setState(() {
          _phase = 'gps';
          _detail = 'waiting_for_hardware_fix';
        });
      }
    }
  }

  Future<void> _sendManualSet() async {
    final ok = await _ble.sendManualControl(
      steer: 0,
      throttlePct: 0,
      ttlMs: 1000,
    );
    _appendEvent('manual_set_zero ok=$ok');
    if (!ok) {
      _finish(false, 'manual_set_failed');
    }
  }

  Future<void> _sendManualOff() async {
    final ok = await _ble.manualOff();
    _manualOffSent = ok;
    _appendEvent('manual_off ok=$ok');
    if (!ok) {
      _finish(false, 'manual_off_failed');
    }
  }

  Future<void> _sendStatusStop() async {
    await _ble.stopAll();
    _appendEvent('status_stop sent');
  }

  Future<void> _sendStatusManualSet() async {
    final ok = await _ble.sendManualControl(
      steer: 0,
      throttlePct: 0,
      ttlMs: 1000,
    );
    _statusManualSetSent = ok;
    _appendEvent('status_manual_set_zero ok=$ok');
    if (!ok) {
      _finish(false, 'status_manual_set_failed');
    }
  }

  Future<void> _sendStatusManualOff() async {
    final ok = await _ble.manualOff();
    _statusManualOffSent = ok;
    _appendEvent('status_manual_off ok=$ok');
    if (!ok) {
      _finish(false, 'status_manual_off_failed');
    }
  }

  Future<void> _sendSimRun() async {
    final ok = await _ble.sendCustomCommand(
      'SIM_RUN:S0_hold_still_good,1',
      allowDevHil: true,
    );
    _appendEvent('sim_run ok=$ok');
    if (!ok) {
      _finish(false, 'sim_run_failed');
    }
  }

  Future<void> _sendSimAbort() async {
    final ok = await _ble.sendCustomCommand('SIM_ABORT', allowDevHil: true);
    _simAbortSent = ok;
    _appendEvent('sim_abort ok=$ok');
    if (!ok) {
      _finish(false, 'sim_abort_failed');
    }
  }

  Future<void> _sendSimManualSet() async {
    final ok = await _ble.sendManualControl(
      steer: 0,
      throttlePct: 0,
      ttlMs: 1000,
    );
    _simManualSetSent = ok;
    _appendEvent('sim_manual_set_zero ok=$ok');
    if (!ok) {
      _finish(false, 'sim_manual_set_failed');
    }
  }

  Future<void> _sendSimManualOff() async {
    final ok = await _ble.manualOff();
    _simManualOffSent = ok;
    _appendEvent('sim_manual_off ok=$ok');
    if (!ok) {
      _finish(false, 'sim_manual_off_failed');
    }
  }

  Future<void> _sendAnchorOn() async {
    final ok = await _ble.sendCustomCommand('ANCHOR_ON');
    _appendEvent('anchor_on ok=$ok');
    if (!ok) {
      _finish(false, 'anchor_on_failed');
    }
  }

  Future<void> _sendAnchorOff() async {
    final ok = await _ble.sendCustomCommand('ANCHOR_OFF');
    _appendEvent('anchor_off ok=$ok');
    if (!ok) {
      _anchorOffSent = false;
      _finish(false, 'anchor_off_failed');
    }
  }

  Future<void> _sendCompassCommands() async {
    final calOk = await _ble.sendCustomCommand(
      'COMPASS_CAL_START',
      allowService: true,
    );
    final autosaveOffOk = await _ble.sendCustomCommand(
      'COMPASS_DCD_AUTOSAVE_OFF',
      allowService: true,
    );
    final saveOk = await _ble.sendCustomCommand(
      'COMPASS_DCD_SAVE',
      allowService: true,
    );
    _appendEvent(
      'compass_commands cal=$calOk autosaveOff=$autosaveOffOk dcdSave=$saveOk',
    );
    if (!calOk || !autosaveOffOk || !saveOk) {
      _finish(false, 'compass_command_write_failed');
    }
  }

  void _finishCompassIfReady() {
    if (_compassCalLogSeen &&
        _compassAutosaveLogSeen &&
        _compassDcdSaveLogSeen) {
      _finish(true, 'compass_command_logs_received');
    }
  }

  void _checkReconnectGap() {
    if (_completed || widget.mode != BleSmokeMode.reconnect) return;
    if (!_firstTelemetrySeen || _reconnectGapSeen || _lastDataAt == null) {
      return;
    }
    final gap = DateTime.now().difference(_lastDataAt!);
    if (gap < _reconnectGap) return;
    _reconnectGapSeen = true;
    _appendEvent(encodeSmokeStageLine('telemetry_gap'));
    if (mounted) {
      setState(() {
        _phase = 'reconnect';
        _detail = 'telemetry_gap_waiting_for_return';
      });
    }
  }

  void _onDeviceLog(String line) {
    _deviceLogEvents += 1;
    _lastDeviceLog = line.trim();
    _appendEvent('device_log ${_lastDeviceLog!}');
    final profileRejection = smokeProfileRejection(_lastDeviceLog);
    if (profileRejection != null) {
      _appendEvent(
        encodeSmokeStageLine(
          'command_rejected_${profileRejection.commandName}_${profileRejection.profile}',
        ),
      );
      if (widget.mode == BleSmokeMode.sim &&
          profileRejection.matchesCommandPrefix('SIM_RUN')) {
        _finish(false, 'sim_rejected_by_profile_${profileRejection.profile}');
        return;
      }
      if (widget.mode == BleSmokeMode.compass &&
          (profileRejection.matchesCommandPrefix('COMPASS_CAL_START') ||
              profileRejection.matchesCommandPrefix(
                'COMPASS_DCD_AUTOSAVE_OFF',
              ) ||
              profileRejection.matchesCommandPrefix('COMPASS_DCD_SAVE'))) {
        _finish(
          false,
          'compass_rejected_by_profile_${profileRejection.profile}',
        );
        return;
      }
    }
    if (widget.mode == BleSmokeMode.anchor &&
        smokeAnchorDeniedLogSeen(_lastDeviceLog)) {
      _anchorDeniedSeen = true;
      _appendEvent(encodeSmokeStageLine('anchor_denied_seen'));
    }
    if (widget.mode == BleSmokeMode.compass) {
      if (smokeCompassCalStartLogSeen(_lastDeviceLog)) {
        _compassCalLogSeen = true;
        _appendEvent(encodeSmokeStageLine('compass_cal_start_seen'));
      }
      if (smokeCompassDcdAutosaveLogSeen(_lastDeviceLog)) {
        _compassAutosaveLogSeen = true;
        _appendEvent(encodeSmokeStageLine('compass_dcd_autosave_seen'));
      }
      if (smokeCompassDcdSaveLogSeen(_lastDeviceLog)) {
        _compassDcdSaveLogSeen = true;
        _appendEvent(encodeSmokeStageLine('compass_dcd_save_seen'));
      }
      _finishCompassIfReady();
    }
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
    final line = encodeSmokeResultLine(payload);
    debugPrint(line);
    developer.log(jsonEncode(payload), name: 'BoatLockSmoke');
    _appendEvent(line);
    if (mounted) {
      setState(() {
        _result = pass ? 'PASS' : 'FAIL';
        _phase = 'done';
        _detail = reason;
      });
    }
  }

  void _appendEvent(String line) {
    debugPrint('[BoatLockSmoke] $line');
    developer.log(line, name: 'BoatLockSmoke');
    if (!mounted) {
      return;
    }
    setState(() {
      _eventLines.add(line);
      if (_eventLines.length > 120) {
        _eventLines.removeAt(0);
      }
    });
  }

  @override
  void dispose() {
    _timeoutTimer?.cancel();
    _gapTimer?.cancel();
    _ble.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final pass = _result == 'PASS';
    final fail = _result == 'FAIL';
    final color = pass
        ? Colors.green
        : fail
        ? Colors.red
        : Colors.orange;

    return Scaffold(
      appBar: AppBar(title: const Text('BoatLock BLE Smoke')),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              _result,
              style: theme.textTheme.headlineMedium?.copyWith(
                color: color,
                fontWeight: FontWeight.bold,
              ),
            ),
            const SizedBox(height: 8),
            Text('phase=$_phase'),
            Text('detail=$_detail'),
            Text('dataEvents=$_dataEvents deviceLogEvents=$_deviceLogEvents'),
            if (_lastData != null) ...[
              const SizedBox(height: 8),
              Text('mode=${_lastData!.mode} status=${_lastData!.status}'),
              Text(
                'gps=${_lastData!.lat.toStringAsFixed(6)}, '
                '${_lastData!.lon.toStringAsFixed(6)} q=${_lastData!.gnssQ}',
              ),
              Text('statusReasons=${_lastData!.statusReasons}'),
              Text(
                'secPaired=${_lastData!.secPaired} '
                'secAuth=${_lastData!.secAuth} '
                'rssi=${_lastData!.rssi}',
              ),
            ],
            const SizedBox(height: 12),
            const Text('events'),
            const SizedBox(height: 8),
            Expanded(
              child: DecoratedBox(
                decoration: BoxDecoration(
                  border: Border.all(color: Colors.black12),
                  borderRadius: BorderRadius.circular(12),
                ),
                child: ListView.builder(
                  padding: const EdgeInsets.all(12),
                  itemCount: _eventLines.length,
                  itemBuilder: (context, index) {
                    return Text(
                      _eventLines[index],
                      style: theme.textTheme.bodySmall,
                    );
                  },
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
