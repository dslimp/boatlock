import 'dart:async';
import 'dart:convert';
import 'dart:developer' as developer;

import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;

import '../ble/ble_boatlock.dart';
import '../ble/ble_command_rejection.dart';
import '../ble/ble_ota_payload.dart';
import '../models/boat_data.dart';
import '../ota/firmware_update_client.dart';
import '../smoke/ble_smoke_logic.dart';
import '../smoke/ble_smoke_mode.dart';

const kBoatLockAppE2eModeDefine = 'BOATLOCK_APP_E2E_MODE';
const kBoatLockAppE2eModeName = String.fromEnvironment(
  kBoatLockAppE2eModeDefine,
  defaultValue: '',
);
const kBoatLockAppE2eOtaUrlDefine = 'BOATLOCK_APP_E2E_OTA_URL';
const kBoatLockAppE2eOtaSha256Define = 'BOATLOCK_APP_E2E_OTA_SHA256';
const kBoatLockAppE2eOtaLatestReleaseDefine =
    'BOATLOCK_APP_E2E_OTA_LATEST_RELEASE';
const kBoatLockAppE2eOtaUrl = String.fromEnvironment(
  kBoatLockAppE2eOtaUrlDefine,
  defaultValue: '',
);
const kBoatLockAppE2eOtaSha256 = String.fromEnvironment(
  kBoatLockAppE2eOtaSha256Define,
  defaultValue: '',
);
const kBoatLockAppE2eOtaLatestRelease = bool.fromEnvironment(
  kBoatLockAppE2eOtaLatestReleaseDefine,
);

enum _SimSuitePhase { idle, listing, starting, running, reporting, cleanup }

List<String> parseAppE2eSimListLog(String line) {
  const prefix = '[SIM] LIST ';
  if (!line.startsWith(prefix)) {
    return const <String>[];
  }
  return line
      .substring(prefix.length)
      .split(',')
      .map((value) => value.trim())
      .where((value) => value.isNotEmpty)
      .toList(growable: false);
}

Map<String, dynamic>? parseAppE2eSimJsonLog(String line, String prefix) {
  if (!line.startsWith(prefix)) {
    return null;
  }
  try {
    final decoded = jsonDecode(line.substring(prefix.length));
    if (decoded is Map<String, dynamic>) {
      return decoded;
    }
  } catch (_) {}
  return null;
}

String? appE2eSimReportChunk(String line) {
  const prefix = '[SIM] REPORT ';
  if (!line.startsWith(prefix) || line == '[SIM] REPORT unavailable') {
    return null;
  }
  return line.substring(prefix.length);
}

String? appE2eProfileRejectionStage(BleCommandRejection? rejection) {
  if (rejection == null) return null;
  return 'command_rejected_${rejection.commandName}_${rejection.profile}';
}

String? appE2eProfileRejectionReason(
  BleSmokeMode mode,
  BleCommandRejection? rejection,
) {
  if (rejection == null) return null;
  final profile = rejection.profile;
  switch (mode) {
    case BleSmokeMode.sim:
    case BleSmokeMode.sim_suite:
      if (rejection.matchesCommandPrefix('SIM_RUN') ||
          rejection.matchesCommandPrefix('SIM_ABORT') ||
          rejection.matchesCommandPrefix('SIM_LIST') ||
          rejection.matchesCommandPrefix('SIM_STATUS') ||
          rejection.matchesCommandPrefix('SIM_REPORT')) {
        return mode == BleSmokeMode.sim_suite
            ? 'app_sim_suite_rejected_by_profile_$profile'
            : 'app_sim_rejected_by_profile_$profile';
      }
    case BleSmokeMode.compass:
      if (rejection.matchesCommandPrefix('COMPASS_CAL_START') ||
          rejection.matchesCommandPrefix('COMPASS_DCD_AUTOSAVE_OFF') ||
          rejection.matchesCommandPrefix('COMPASS_DCD_SAVE')) {
        return 'app_compass_rejected_by_profile_$profile';
      }
    case BleSmokeMode.ota:
      if (rejection.matchesCommandPrefix('OTA_BEGIN')) {
        return 'app_ota_begin_rejected_by_profile_$profile';
      }
      if (rejection.matchesCommandPrefix('OTA_FINISH')) {
        return 'app_ota_finish_rejected_by_profile_$profile';
      }
    case BleSmokeMode.basic:
    case BleSmokeMode.reconnect:
    case BleSmokeMode.manual:
    case BleSmokeMode.status:
    case BleSmokeMode.anchor:
    case BleSmokeMode.gps:
      return null;
  }
  return null;
}

class BoatLockAppE2eProbe {
  BoatLockAppE2eProbe._({required BleBoatLock ble, required BleSmokeMode mode})
    : _ble = ble,
      _mode = mode {
    _log('starting_app_e2e mode=${_mode.name}');
    final timeout = switch (_mode) {
      BleSmokeMode.reconnect => const Duration(seconds: 150),
      BleSmokeMode.gps => const Duration(seconds: 180),
      BleSmokeMode.sim_suite => const Duration(minutes: 25),
      BleSmokeMode.ota => const Duration(minutes: 25),
      _ => const Duration(seconds: 75),
    };
    _timeoutTimer = Timer(timeout, () {
      _finish(false, 'app_${_mode.name}_timeout');
    });
    if (_mode == BleSmokeMode.reconnect || _mode == BleSmokeMode.ota) {
      _gapTimer = Timer.periodic(const Duration(seconds: 1), (_) {
        if (_mode == BleSmokeMode.ota) {
          _checkOtaReconnectGap();
        } else {
          _checkReconnectGap();
        }
      });
    }
  }

  final BleBoatLock _ble;
  final BleSmokeMode _mode;
  Timer? _timeoutTimer;
  Timer? _gapTimer;
  Timer? _simSuitePollTimer;
  BoatData? _lastData;
  DateTime? _lastDataAt;
  String? _lastDeviceLog;
  BleCommandRejection? _lastCommandRejection;
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
  _SimSuitePhase _simSuitePhase = _SimSuitePhase.idle;
  List<String> _simSuiteScenarioIds = const <String>[];
  int _simSuiteIndex = -1;
  int _simSuitePassCount = 0;
  String _simSuiteReportBuffer = '';
  String? _simSuiteCurrentScenario;
  bool _simSuiteRunStarted = false;
  bool _simSuiteReportRequested = false;
  bool _anchorOnSent = false;
  bool _anchorDeniedSeen = false;
  bool _anchorOffSent = false;
  bool _compassCommandsSent = false;
  bool _compassCalLogSeen = false;
  bool _compassAutosaveLogSeen = false;
  bool _compassDcdSaveLogSeen = false;
  bool _gpsWaitingLogged = false;
  bool _otaStarted = false;
  bool _otaUploadDone = false;
  bool _otaReconnectGapSeen = false;
  int _otaProgressBucket = -1;

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
      'paired=${data.secPaired} auth=${data.secAuth} rssi=${data.rssi} '
      'lat=${data.lat.toStringAsFixed(6)} lon=${data.lon.toStringAsFixed(6)} '
      'gnssQ=${data.gnssQ}',
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
      case BleSmokeMode.sim_suite:
        _handleSimSuiteData(data);
      case BleSmokeMode.anchor:
        _handleAnchorData(data);
      case BleSmokeMode.compass:
        _handleCompassData();
      case BleSmokeMode.gps:
        _handleGpsData(data);
      case BleSmokeMode.ota:
        _handleOtaData();
    }
  }

  void onDeviceLog(String line) {
    if (_completed) {
      return;
    }
    _deviceLogEvents += 1;
    _lastDeviceLog = line.trim();
    _log('device_log $_lastDeviceLog');
    final rejection = smokeProfileRejection(_lastDeviceLog);
    if (rejection != null) {
      _lastCommandRejection = rejection;
    }
    final rejectionReason = appE2eProfileRejectionReason(_mode, rejection);
    if (rejectionReason != null) {
      final stage = appE2eProfileRejectionStage(rejection);
      if (stage != null) {
        _stage(stage);
      }
      _finish(false, rejectionReason);
      return;
    }
    if (_mode == BleSmokeMode.anchor &&
        smokeAnchorDeniedLogSeen(_lastDeviceLog)) {
      _anchorDeniedSeen = true;
      _stage('anchor_denied_seen');
    }
    if (_mode == BleSmokeMode.sim_suite) {
      _handleSimSuiteDeviceLog(_lastDeviceLog!);
    }
    if (_mode == BleSmokeMode.compass) {
      if (smokeCompassCalStartLogSeen(_lastDeviceLog)) {
        _compassCalLogSeen = true;
        _stage('compass_cal_start_seen');
      }
      if (smokeCompassDcdAutosaveLogSeen(_lastDeviceLog)) {
        _compassAutosaveLogSeen = true;
        _stage('compass_dcd_autosave_seen');
      }
      if (smokeCompassDcdSaveLogSeen(_lastDeviceLog)) {
        _compassDcdSaveLogSeen = true;
        _stage('compass_dcd_save_seen');
      }
      _finishCompassIfReady();
    }
  }

  void dispose() {
    _timeoutTimer?.cancel();
    _gapTimer?.cancel();
    _simSuitePollTimer?.cancel();
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

  void _handleSimSuiteData(BoatData data) {
    if (_simSuitePhase == _SimSuitePhase.idle) {
      _simSuitePhase = _SimSuitePhase.listing;
      _stage('sim_suite_list');
      unawaited(_sendSimSuiteList());
      return;
    }
    if (_simSuitePhase == _SimSuitePhase.running &&
        _simSuiteRunStarted &&
        !_simSuiteReportRequested &&
        data.mode != 'SIM') {
      _stage('sim_suite_mode_exit_${_simSuiteIndex + 1}');
      unawaited(_requestSimSuiteReport());
      return;
    }
    if (_simSuitePhase != _SimSuitePhase.cleanup) {
      return;
    }
    if (!_simManualSetSent) {
      _stage('sim_suite_manual_recovery');
      unawaited(_sendSimManualSet());
      return;
    }
    if (!_simManualModeSeen) {
      if (data.mode != 'MANUAL') {
        return;
      }
      _simManualModeSeen = true;
      _stage('sim_suite_manual_mode_seen');
      unawaited(_sendSimManualOff());
      return;
    }
    if (!_simManualOffSent || !smokeStatusRecoveredAfterStop(data)) {
      return;
    }
    _finish(true, 'app_sim_suite_all_passed');
  }

  void _handleSimSuiteDeviceLog(String line) {
    if (_simSuitePhase == _SimSuitePhase.listing) {
      final ids = parseAppE2eSimListLog(line);
      if (ids.isNotEmpty) {
        _simSuiteScenarioIds = ids;
        _stage('sim_suite_list_received_${ids.length}');
        unawaited(_startNextSimSuiteScenario());
        return;
      }
    }

    if (line.startsWith('[SIM] RUN failed') ||
        line.startsWith('[SIM] RUN rejected')) {
      _finish(false, 'app_sim_suite_run_rejected');
      return;
    }
    if (line.startsWith('[SIM] RUN started')) {
      _simSuiteRunStarted = true;
      _stage('sim_suite_run_started_${_simSuiteIndex + 1}');
      _startSimSuitePolling();
      return;
    }

    final status = parseAppE2eSimJsonLog(line, '[SIM] STATUS ');
    if (status != null) {
      _handleSimSuiteStatus(status);
      return;
    }

    if (line == '[SIM] REPORT unavailable') {
      _finish(false, 'app_sim_suite_report_unavailable');
      return;
    }
    final chunk = appE2eSimReportChunk(line);
    if (chunk != null) {
      _handleSimSuiteReportChunk(chunk);
    }
  }

  void _handleSimSuiteStatus(Map<String, dynamic> status) {
    if (_simSuitePhase != _SimSuitePhase.running || _simSuiteReportRequested) {
      return;
    }
    final current = _simSuiteCurrentScenario;
    if (current == null || status['id'] != current) {
      return;
    }
    final state = status['state'];
    if (state == 'DONE' || state == 'ABORTED') {
      _stage(
        'sim_suite_status_${state.toString().toLowerCase()}_${_simSuiteIndex + 1}',
      );
      unawaited(_requestSimSuiteReport());
    }
  }

  void _handleSimSuiteReportChunk(String chunk) {
    if (_simSuitePhase != _SimSuitePhase.reporting) {
      return;
    }
    _simSuiteReportBuffer += chunk;
    Map<String, dynamic> report;
    try {
      final decoded = jsonDecode(_simSuiteReportBuffer);
      if (decoded is! Map<String, dynamic>) {
        return;
      }
      report = decoded;
    } catch (_) {
      return;
    }
    _handleSimSuiteReport(report);
  }

  void _handleSimSuiteReport(Map<String, dynamic> report) {
    final current = _simSuiteCurrentScenario;
    if (current == null || report['id'] != current) {
      _finish(false, 'app_sim_suite_report_id_mismatch');
      return;
    }
    final metrics = report['metrics'];
    final pass = report['pass'] == true;
    final state = report['state']?.toString() ?? '';
    final reason = report['reason']?.toString() ?? '';
    var metricSummary = '';
    if (metrics is Map<String, dynamic>) {
      metricSummary =
          ' p95=${metrics['p95_error_m']} max=${metrics['max_error_m']} '
          'deadband=${metrics['time_in_deadband_pct']} '
          'sat=${metrics['time_saturated_pct']}';
    }
    _log(
      'sim_suite_report ${_simSuiteIndex + 1}/${_simSuiteScenarioIds.length} '
      'id=$current pass=$pass state=$state reason=$reason$metricSummary',
    );
    if (!pass || state != 'DONE') {
      _finish(false, 'app_sim_suite_scenario_failed');
      return;
    }
    _simSuitePassCount += 1;
    _stage('sim_suite_report_pass_${_simSuiteIndex + 1}');
    unawaited(_startNextSimSuiteScenario());
  }

  Future<void> _startNextSimSuiteScenario() async {
    if (_completed) {
      return;
    }
    _simSuiteIndex += 1;
    if (_simSuiteIndex >= _simSuiteScenarioIds.length) {
      _stopSimSuitePolling();
      _simSuitePhase = _SimSuitePhase.cleanup;
      _stage('sim_suite_reports_passed_$_simSuitePassCount');
      return;
    }
    final scenarioId = _simSuiteScenarioIds[_simSuiteIndex];
    _simSuiteCurrentScenario = scenarioId;
    _simSuiteRunStarted = false;
    _simSuiteReportRequested = false;
    _simSuiteReportBuffer = '';
    _simSuitePhase = _SimSuitePhase.starting;
    _stage('sim_suite_run_${_simSuiteIndex + 1}');
    final ok = await _ble.sendCustomCommand(
      'SIM_RUN:$scenarioId,0',
      allowDevHil: true,
    );
    _log('sim_suite_run id=$scenarioId ok=$ok');
    if (!ok) {
      _finish(false, 'app_sim_suite_run_write_failed');
      return;
    }
    _simSuitePhase = _SimSuitePhase.running;
    _startSimSuitePolling();
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

  void _handleCompassData() {
    if (!_compassCommandsSent) {
      _compassCommandsSent = true;
      _stage('compass_commands');
      unawaited(_sendCompassCommands());
      return;
    }
    _finishCompassIfReady();
  }

  void _handleGpsData(BoatData data) {
    if (smokeGpsFixLooksHealthy(data)) {
      _finish(true, 'app_gps_fix_received');
      return;
    }
    if (!_gpsWaitingLogged) {
      _gpsWaitingLogged = true;
      _stage('gps_waiting_fix');
    }
  }

  void _handleOtaData() {
    if (!_otaStarted) {
      _otaStarted = true;
      _stage('ota_upload_start');
      unawaited(_runOtaUpload());
      return;
    }
    if (_otaUploadDone && _otaReconnectGapSeen) {
      _finish(true, 'app_ota_reconnect_after_update');
    }
  }

  Future<void> _runOtaUpload() async {
    if (kBoatLockAppE2eOtaLatestRelease) {
      await _runLatestReleaseOtaUpload();
      return;
    }
    final uri = Uri.tryParse(kBoatLockAppE2eOtaUrl);
    final expectedSha = normalizeBoatLockSha256Hex(kBoatLockAppE2eOtaSha256);
    if (uri == null || !uri.hasScheme || expectedSha == null) {
      _finish(false, 'app_ota_missing_firmware_define');
      return;
    }

    try {
      final response = await http
          .get(uri)
          .timeout(const Duration(seconds: 120));
      if (response.statusCode != 200) {
        _log('ota_download status=${response.statusCode}');
        _finish(false, 'app_ota_download_failed');
        return;
      }
      final firmware = response.bodyBytes;
      final actualSha = boatLockSha256Hex(firmware);
      if (actualSha != expectedSha) {
        _log('ota_sha_mismatch actual=$actualSha expected=$expectedSha');
        _finish(false, 'app_ota_sha_mismatch');
        return;
      }
      _stage('ota_download_verified');
      await _uploadVerifiedOta(firmware, expectedSha);
    } catch (e) {
      _log('ota_error $e');
      _finish(false, 'app_ota_exception');
    }
  }

  Future<void> _runLatestReleaseOtaUpload() async {
    try {
      _stage('ota_latest_release_manifest_fetch');
      final bundle = await const FirmwareUpdateClient().fetchLatestFirmware(
        onProgress: (received, total) {
          if (total <= 0) {
            return;
          }
          final bucket = (100 * received / total).floor();
          if (bucket ~/ 10 != _otaProgressBucket ~/ 10 || received == total) {
            _otaProgressBucket = bucket;
            _log(
              'ota_latest_release_download received=$received total=$total pct=$bucket',
            );
          }
        },
      );
      _stage('ota_latest_release_download_verified');
      _otaProgressBucket = -1;
      await _uploadVerifiedOta(bundle.firmware, bundle.manifest.sha256);
    } catch (e) {
      _log('ota_latest_release_error $e');
      _finish(false, 'app_ota_latest_release_exception');
    }
  }

  Future<void> _uploadVerifiedOta(List<int> firmware, String sha256Hex) async {
    final ok = await _ble.uploadFirmwareOtaBytes(
      firmware: firmware,
      sha256Hex: sha256Hex,
      onProgress: (sent, total) {
        if (total <= 0) {
          return;
        }
        final bucket = (100 * sent / total).floor();
        if (bucket ~/ 10 != _otaProgressBucket ~/ 10 || sent == total) {
          _otaProgressBucket = bucket;
          _log('ota_progress sent=$sent total=$total pct=$bucket');
        }
      },
    );
    if (!ok) {
      _finish(false, 'app_ota_upload_failed');
      return;
    }
    _otaUploadDone = true;
    _lastDataAt = DateTime.now();
    _stage('ota_upload_done_wait_reconnect');
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
    final ok = await _ble.sendCustomCommand(
      'SIM_RUN:S0_hold_still_good,1',
      allowDevHil: true,
    );
    _log('sim_run ok=$ok');
    if (!ok) {
      _finish(false, 'app_sim_run_failed');
    }
  }

  Future<void> _sendSimAbort() async {
    final ok = await _ble.sendCustomCommand('SIM_ABORT', allowDevHil: true);
    _simAbortSent = ok;
    _log('sim_abort ok=$ok');
    if (!ok) {
      _finish(false, 'app_sim_abort_failed');
    }
  }

  Future<void> _sendSimSuiteList() async {
    final ok = await _ble.sendCustomCommand('SIM_LIST', allowDevHil: true);
    _log('sim_suite_list ok=$ok');
    if (!ok) {
      _finish(false, 'app_sim_suite_list_failed');
    }
  }

  void _startSimSuitePolling() {
    if (_simSuitePollTimer != null) {
      return;
    }
    _simSuitePollTimer = Timer.periodic(const Duration(seconds: 2), (_) {
      if (_completed ||
          _simSuitePhase != _SimSuitePhase.running ||
          _simSuiteReportRequested) {
        return;
      }
      unawaited(_sendSimSuiteStatus());
    });
  }

  void _stopSimSuitePolling() {
    _simSuitePollTimer?.cancel();
    _simSuitePollTimer = null;
  }

  Future<void> _sendSimSuiteStatus() async {
    final ok = await _ble.sendCustomCommand('SIM_STATUS', allowDevHil: true);
    if (!ok) {
      _finish(false, 'app_sim_suite_status_failed');
    }
  }

  Future<void> _requestSimSuiteReport() async {
    if (_completed || _simSuiteReportRequested) {
      return;
    }
    _simSuiteReportRequested = true;
    _simSuitePhase = _SimSuitePhase.reporting;
    _simSuiteReportBuffer = '';
    _stopSimSuitePolling();
    final ok = await _ble.sendCustomCommand('SIM_REPORT', allowDevHil: true);
    _log('sim_suite_report_request ok=$ok');
    if (!ok) {
      _finish(false, 'app_sim_suite_report_failed');
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
    _log(
      'compass_commands cal=$calOk autosaveOff=$autosaveOffOk dcdSave=$saveOk',
    );
    if (!calOk || !autosaveOffOk || !saveOk) {
      _finish(false, 'app_compass_command_write_failed');
    }
  }

  void _finishCompassIfReady() {
    if (_compassCalLogSeen &&
        _compassAutosaveLogSeen &&
        _compassDcdSaveLogSeen) {
      _finish(true, 'app_compass_command_logs_received');
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

  void _checkOtaReconnectGap() {
    if (_completed ||
        !_otaUploadDone ||
        _otaReconnectGapSeen ||
        _lastDataAt == null) {
      return;
    }
    final gap = DateTime.now().difference(_lastDataAt!);
    if (gap < const Duration(seconds: 4)) {
      return;
    }
    _otaReconnectGapSeen = true;
    _stage('ota_reconnect_gap');
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
    _simSuitePollTimer?.cancel();
    final payload = buildSmokeResultPayload(
      pass: pass,
      reason: reason,
      dataEvents: _dataEvents,
      deviceLogEvents: _deviceLogEvents,
      data: _lastData,
      lastDeviceLog: _lastDeviceLog,
      commandRejection: _lastCommandRejection,
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
