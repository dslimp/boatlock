import 'dart:async';
import 'dart:convert';
import 'dart:developer' as developer;

import 'package:flutter/material.dart';

import '../ble/ble_boatlock.dart';
import '../models/boat_data.dart';
import 'ble_smoke_logic.dart';

class BleSmokePage extends StatefulWidget {
  const BleSmokePage({super.key});

  @override
  State<BleSmokePage> createState() => _BleSmokePageState();
}

class _BleSmokePageState extends State<BleSmokePage> {
  static const Duration _smokeTimeout = Duration(seconds: 30);

  late final BleBoatLock _ble;
  Timer? _timeoutTimer;
  final List<String> _eventLines = <String>[];
  BoatData? _lastData;
  String _phase = 'starting';
  String _result = 'RUNNING';
  String _detail = 'booting';
  int _dataEvents = 0;
  int _deviceLogEvents = 0;
  String? _lastDeviceLog;
  bool _completed = false;

  @override
  void initState() {
    super.initState();
    _ble = BleBoatLock(
      onData: _onData,
      onLog: _onDeviceLog,
    );
    _start();
  }

  Future<void> _start() async {
    _appendEvent('starting_ble_smoke');
    _timeoutTimer = Timer(_smokeTimeout, () {
      _finish(false, 'timeout_waiting_for_telemetry');
    });
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
    _dataEvents += 1;
    _appendEvent(
      'telemetry mode=${data.mode} status=${data.status} '
      'paired=${data.secPaired} auth=${data.secAuth} rssi=${data.rssi}',
    );
    if (!_completed && smokeTelemetryLooksHealthy(data)) {
      _finish(true, 'telemetry_received');
      return;
    }
    if (mounted) {
      setState(() {
        _phase = 'telemetry';
        _detail = 'mode=${data.mode} status=${data.status}';
      });
    }
  }

  void _onDeviceLog(String line) {
    _deviceLogEvents += 1;
    _lastDeviceLog = line.trim();
    _appendEvent('device_log ${_lastDeviceLog!}');
  }

  void _finish(bool pass, String reason) {
    if (_completed) {
      return;
    }
    _completed = true;
    _timeoutTimer?.cancel();
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
