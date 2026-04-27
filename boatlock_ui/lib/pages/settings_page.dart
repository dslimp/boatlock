import 'dart:async';
import 'dart:developer' as developer;
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import '../ble/ble_boatlock.dart';
import '../ble/ble_command_rejection.dart';
import '../ble/ble_debug_snapshot.dart';
import '../ble/ble_ota_payload.dart';
import '../ble/ble_security_codec.dart';
import '../ota/firmware_update_client.dart';
import '../ota/firmware_update_manifest.dart';
import 'settings_command_rejection_guard.dart';

class SettingsPage extends StatefulWidget {
  final BleBoatLock ble;
  final bool holdHeading;
  final double stepMaxSpd;
  final double stepAccel;
  final double compassOffset;
  final int compassQ;
  final int magQ;
  final int gyroQ;
  final double rvAcc;
  final double magNorm;
  final double gyroNorm;
  final double pitch;
  final double roll;
  final bool isConnected;
  final bool secPaired;
  final bool secAuth;
  final bool secPairWindowOpen;
  final String secReject;
  final FirmwareUpdateClient firmwareUpdateClient;

  const SettingsPage({
    super.key,
    required this.ble,
    required this.holdHeading,
    required this.stepMaxSpd,
    required this.stepAccel,
    required this.compassOffset,
    required this.compassQ,
    required this.magQ,
    required this.gyroQ,
    required this.rvAcc,
    required this.magNorm,
    required this.gyroNorm,
    required this.pitch,
    required this.roll,
    required this.isConnected,
    required this.secPaired,
    required this.secAuth,
    required this.secPairWindowOpen,
    required this.secReject,
    this.firmwareUpdateClient = const FirmwareUpdateClient(),
  });

  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  static bool _serviceMenuSessionEnabled = false;

  late bool holdHeading;
  late bool serviceMenuEnabled;
  late double stepMaxSpd;
  late double stepAccel;
  late double compassOffset;
  late int compassQ;
  late int magQ;
  late int gyroQ;
  late double rvAcc;
  late double magNorm;
  late double gyroNorm;
  late double pitch;
  late double roll;
  late bool isConnected;
  late bool secPaired;
  late bool secAuth;
  late bool secPairWindowOpen;
  late String secReject;
  late final TextEditingController _ownerSecretCtrl;
  late final TextEditingController _otaUrlCtrl;
  late final TextEditingController _otaShaCtrl;
  bool _otaBusy = false;
  double? _otaProgress;
  String _otaStatus = '';
  String _otaProgressLabel = '';
  DateTime? _otaStartedAt;
  DateTime? _otaLastLogAt;
  int _otaLastLogBucket = -1;
  int _lastSeenCommandRejectEvents = 0;
  _ProfileRejectionWait? _profileRejectionWait;
  late final FirmwareUpdateClient _firmwareUpdateClient;
  FirmwareUpdateManifest? _releaseFirmwareManifest;

  @override
  void initState() {
    super.initState();
    holdHeading = widget.holdHeading;
    serviceMenuEnabled = _serviceMenuSessionEnabled;
    stepMaxSpd = widget.stepMaxSpd;
    stepAccel = widget.stepAccel;
    compassOffset = widget.compassOffset;
    compassQ = widget.compassQ;
    magQ = widget.magQ;
    gyroQ = widget.gyroQ;
    rvAcc = widget.rvAcc;
    magNorm = widget.magNorm;
    gyroNorm = widget.gyroNorm;
    pitch = widget.pitch;
    roll = widget.roll;
    isConnected = widget.isConnected;
    secPaired = widget.secPaired;
    secAuth = widget.secAuth;
    secPairWindowOpen = widget.secPairWindowOpen;
    secReject = widget.secReject;
    _ownerSecretCtrl = TextEditingController(
      text: widget.ble.ownerSecret ?? '',
    );
    _otaUrlCtrl = TextEditingController();
    _otaShaCtrl = TextEditingController();
    _firmwareUpdateClient = widget.firmwareUpdateClient;
    widget.ble.setOwnerSecret(_ownerSecretCtrl.text);
    _lastSeenCommandRejectEvents =
        widget.ble.diagnostics.value.commandRejectEvents;
    widget.ble.diagnostics.addListener(_onDiagnosticsChanged);
  }

  @override
  void dispose() {
    widget.ble.diagnostics.removeListener(_onDiagnosticsChanged);
    _ownerSecretCtrl.dispose();
    _otaUrlCtrl.dispose();
    _otaShaCtrl.dispose();
    super.dispose();
  }

  void _showMessage(String message) {
    if (!mounted) return;
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text(message)));
  }

  void _onDiagnosticsChanged() {
    final snapshot = widget.ble.diagnostics.value;
    if (snapshot.commandRejectEvents <= _lastSeenCommandRejectEvents) {
      return;
    }
    _lastSeenCommandRejectEvents = snapshot.commandRejectEvents;
    if (_completePendingProfileRejection(snapshot)) return;
    final rejection = snapshot.lastCommandRejection;
    if (rejection == null) return;
    _showProfileRejection(
      rejection.commandName,
      rejection.profile,
      rejection.requiredProfile,
    );
  }

  bool _completePendingProfileRejection(BleDebugSnapshot snapshot) {
    final wait = _profileRejectionWait;
    if (wait == null) return false;
    final rejection = findAnyMatchingCommandRejectionAfter(
      snapshot,
      baselineEvents: wait.baselineEvents,
      commandPrefixes: wait.commandPrefixes,
    );
    if (rejection == null) return false;
    _profileRejectionWait = null;
    if (!wait.completer.isCompleted) {
      wait.completer.complete(rejection);
    }
    return true;
  }

  _ProfileRejectionWait _beginProfileRejectionWait(String commandPrefix) {
    return _beginProfileRejectionWaitFor([commandPrefix]);
  }

  _ProfileRejectionWait _beginProfileRejectionWaitFor(
    List<String> commandPrefixes,
  ) {
    final wait = _ProfileRejectionWait(
      baselineEvents: widget.ble.diagnostics.value.commandRejectEvents,
      commandPrefixes: commandPrefixes,
    );
    _profileRejectionWait = wait;
    _completePendingProfileRejection(widget.ble.diagnostics.value);
    return wait;
  }

  void _cancelProfileRejectionWait(_ProfileRejectionWait wait) {
    if (!identical(_profileRejectionWait, wait)) return;
    _profileRejectionWait = null;
    if (!wait.completer.isCompleted) {
      wait.completer.complete(null);
    }
  }

  Future<BleCommandRejection?> _finishProfileRejectionWait(
    _ProfileRejectionWait wait,
  ) async {
    _completePendingProfileRejection(widget.ble.diagnostics.value);
    final result = await wait.completer.future.timeout(
      const Duration(milliseconds: 600),
      onTimeout: () => null,
    );
    if (identical(_profileRejectionWait, wait)) {
      _profileRejectionWait = null;
    }
    return result;
  }

  Future<void> _commitServiceSetting({
    required String commandPrefix,
    required Future<bool> Function() write,
    required VoidCallback rollback,
  }) async {
    final wait = _beginProfileRejectionWait(commandPrefix);
    final ok = await write();
    if (!mounted) {
      _cancelProfileRejectionWait(wait);
      return;
    }
    if (!ok) {
      _cancelProfileRejectionWait(wait);
      setState(rollback);
      _showMessage('Команда отклонена: ${widget.ble.secReject}');
      return;
    }

    final rejection = await _finishProfileRejectionWait(wait);
    if (!mounted || rejection == null) return;
    setState(rollback);
    _showProfileRejection(
      rejection.commandName,
      rejection.profile,
      rejection.requiredProfile,
    );
  }

  void _showProfileRejection(
    String commandName,
    String profile,
    String requiredProfile,
  ) {
    _showMessage(
      _profileRejectionMessage(commandName, profile, requiredProfile),
    );
  }

  String _profileRejectionMessage(
    String commandName,
    String profile,
    String requiredProfile,
  ) {
    return 'Команда $commandName отклонена профилем $profile: нужен $requiredProfile';
  }

  String _profileRejectionMessageFrom(BleCommandRejection rejection) {
    return _profileRejectionMessage(
      rejection.commandName,
      rejection.profile,
      rejection.requiredProfile,
    );
  }

  void _syncOwnerSecret() {
    widget.ble.setOwnerSecret(_ownerSecretCtrl.text);
  }

  bool get _serviceMenuVisible => serviceMenuEnabled;

  void _toggleServiceMenu(bool value) {
    setState(() {
      serviceMenuEnabled = value;
      _serviceMenuSessionEnabled = value;
    });
  }

  Future<void> _toggleHoldHeading(bool value) async {
    if (!isConnected) return;
    final previous = holdHeading;
    setState(() => holdHeading = value);
    final ok = await widget.ble.setHoldHeading(value);
    if (!ok) {
      setState(() => holdHeading = previous);
      _showMessage('Изменение отклонено: ${widget.ble.secReject}');
    }
  }

  Future<void> _editCompassOffset() async {
    if (!isConnected) return;
    final ctrl = TextEditingController(text: compassOffset.toStringAsFixed(1));
    final val = await showDialog<double>(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text('Офсет компаса (°)'),
        content: TextField(
          controller: ctrl,
          keyboardType: const TextInputType.numberWithOptions(signed: true),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Отмена'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, double.tryParse(ctrl.text)),
            child: const Text('OK'),
          ),
        ],
      ),
    );
    if (val == null) return;
    final previous = compassOffset;
    setState(() => compassOffset = val);
    await _commitServiceSetting(
      commandPrefix: 'SET_COMPASS_OFFSET',
      write: () => widget.ble.setCompassOffset(val),
      rollback: () => compassOffset = previous,
    );
  }

  Future<void> _resetCompassOffset() async {
    if (!isConnected) return;
    final previous = compassOffset;
    setState(() => compassOffset = 0.0);
    await _commitServiceSetting(
      commandPrefix: 'RESET_COMPASS_OFFSET',
      write: widget.ble.resetCompassOffset,
      rollback: () => compassOffset = previous,
    );
  }

  Future<void> _editStepMaxSpd() async {
    if (!isConnected) return;
    final ctrl = TextEditingController(text: stepMaxSpd.round().toString());
    final val = await showDialog<double>(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text('Макс. скорость'),
        content: TextField(
          controller: ctrl,
          keyboardType: TextInputType.number,
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Отмена'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, double.tryParse(ctrl.text)),
            child: const Text('OK'),
          ),
        ],
      ),
    );
    if (val == null || val == stepMaxSpd) return;
    final previous = stepMaxSpd;
    setState(() => stepMaxSpd = val);
    await _commitServiceSetting(
      commandPrefix: 'SET_STEP_MAXSPD',
      write: () => widget.ble.setStepMaxSpeed(val),
      rollback: () => stepMaxSpd = previous,
    );
  }

  Future<void> _editStepAccel() async {
    if (!isConnected) return;
    final ctrl = TextEditingController(text: stepAccel.round().toString());
    final val = await showDialog<double>(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text('Ускорение'),
        content: TextField(
          controller: ctrl,
          keyboardType: TextInputType.number,
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Отмена'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(context, double.tryParse(ctrl.text)),
            child: const Text('OK'),
          ),
        ],
      ),
    );
    if (val == null || val == stepAccel) return;
    final previous = stepAccel;
    setState(() => stepAccel = val);
    await _commitServiceSetting(
      commandPrefix: 'SET_STEP_ACCEL',
      write: () => widget.ble.setStepAccel(val),
      rollback: () => stepAccel = previous,
    );
  }

  void _generateOwnerSecret() {
    final secret = widget.ble.generateOwnerSecret();
    _ownerSecretCtrl.text = secret;
    _syncOwnerSecret();
    _showMessage('Новый owner secret сгенерирован');
  }

  Future<void> _pairDevice() async {
    if (!isConnected) return;
    _syncOwnerSecret();
    if (normalizeOwnerSecret(_ownerSecretCtrl.text) == null) {
      _showMessage('Нужен owner secret из 32 hex-символов');
      return;
    }
    final ok = await widget.ble.pairWithOwnerSecret(_ownerSecretCtrl.text);
    setState(() {
      secPaired = ok;
      secAuth = false;
      secReject = ok ? 'NONE' : widget.ble.secReject;
      secPairWindowOpen = widget.ble.secPairWindowOpen;
    });
    _showMessage(
      ok
          ? 'Пара привязана. Owner secret нужен для последующей авторизации.'
          : 'Привязка не прошла: ${widget.ble.secReject}',
    );
  }

  Future<void> _authenticateOwner() async {
    if (!isConnected) return;
    _syncOwnerSecret();
    final ok = await widget.ble.authenticateOwner(_ownerSecretCtrl.text);
    setState(() {
      secAuth = ok;
      secPaired = widget.ble.secPaired;
      secReject = ok ? 'NONE' : widget.ble.secReject;
    });
    _showMessage(
      ok ? 'Owner auth выполнен' : 'Auth не прошёл: ${widget.ble.secReject}',
    );
  }

  Future<void> _clearPairing() async {
    if (!isConnected) return;
    final ok = await showDialog<bool>(
      context: context,
      builder: (_) => AlertDialog(
        title: const Text('Сбросить pairing'),
        content: const Text(
          'Сброс будет принят только из owner-session или пока открыт pairing window.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Отмена'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Сбросить'),
          ),
        ],
      ),
    );
    if (ok != true) return;

    _syncOwnerSecret();
    final cleared = await widget.ble.clearPairing();
    setState(() {
      secPaired = !cleared;
      secAuth = false;
      secReject = cleared ? 'NONE' : widget.ble.secReject;
    });
    _showMessage(
      cleared ? 'Pairing сброшен' : 'Сброс отклонён: ${widget.ble.secReject}',
    );
  }

  Future<Uint8List> _downloadFirmware(
    Uri uri, {
    void Function(int receivedBytes, int? totalBytes)? onProgress,
  }) async {
    final client = HttpClient();
    client.connectionTimeout = const Duration(seconds: 15);
    try {
      final request = await client.getUrl(uri);
      final response = await request.close();
      if (response.statusCode != HttpStatus.ok) {
        throw HttpException('HTTP ${response.statusCode}', uri: uri);
      }
      final builder = BytesBuilder(copy: false);
      final total = response.contentLength > 0 ? response.contentLength : null;
      var received = 0;
      await for (final chunk in response) {
        builder.add(chunk);
        received += chunk.length;
        onProgress?.call(received, total);
      }
      return builder.takeBytes();
    } finally {
      client.close(force: true);
    }
  }

  String _formatBytes(int bytes) {
    if (bytes >= 1024 * 1024) {
      return '${(bytes / (1024 * 1024)).toStringAsFixed(2)} MB';
    }
    if (bytes >= 1024) {
      return '${(bytes / 1024).toStringAsFixed(1)} KB';
    }
    return '$bytes B';
  }

  String _formatOtaProgress(String phase, int sent, int? total) {
    final elapsed = _otaStartedAt == null
        ? Duration.zero
        : DateTime.now().difference(_otaStartedAt!);
    final seconds = elapsed.inMilliseconds / 1000.0;
    final speed = seconds > 0 ? sent / seconds : 0.0;
    final speedText = '${_formatBytes(speed.round())}/s';
    if (total != null && total > 0) {
      final percent = (100.0 * sent / total).clamp(0.0, 100.0);
      return '$phase ${percent.toStringAsFixed(1)}% '
          '(${_formatBytes(sent)} / ${_formatBytes(total)}, $speedText)';
    }
    return '$phase ${_formatBytes(sent)} ($speedText)';
  }

  void _logOta(String event, {int? sent, int? total, String? detail}) {
    final parts = <String>['BOATLOCK_OTA_PROGRESS event=$event'];
    if (sent != null) parts.add('sent=$sent');
    if (total != null) parts.add('total=$total');
    if (detail != null && detail.isNotEmpty) parts.add('detail=$detail');
    final line = parts.join(' ');
    debugPrint(line);
    developer.log(line, name: 'BoatLockOTA');
  }

  void _logOtaProgress(
    String event,
    int sent,
    int? total, {
    bool force = false,
  }) {
    final now = DateTime.now();
    final bucket = total != null && total > 0 ? ((100 * sent) ~/ total) : -1;
    final logByBucket = bucket >= 0 && bucket ~/ 5 != _otaLastLogBucket ~/ 5;
    final logByTime =
        _otaLastLogAt == null ||
        now.difference(_otaLastLogAt!) >= const Duration(seconds: 10);
    if (!force && !logByBucket && !logByTime) {
      return;
    }
    _otaLastLogAt = now;
    _otaLastLogBucket = bucket;
    _logOta(event, sent: sent, total: total);
  }

  Future<void> _uploadVerifiedFirmware(
    Uint8List firmware,
    String sha256Hex,
  ) async {
    setState(() {
      _otaProgress = 0;
      _otaProgressLabel = 'Передача по BLE 0.0%';
      _otaStatus = _otaProgressLabel;
      _otaStartedAt = DateTime.now();
      _otaLastLogAt = null;
      _otaLastLogBucket = -1;
    });
    _logOta('download_verified', sent: firmware.length, total: firmware.length);
    final profileRejectWait = _beginProfileRejectionWaitFor([
      'OTA_BEGIN',
      'OTA_FINISH',
    ]);
    final ok = await widget.ble.uploadFirmwareOtaBytes(
      firmware: firmware,
      sha256Hex: sha256Hex,
      onProgress: (sent, total) {
        if (!mounted || total <= 0) return;
        final label = _formatOtaProgress('Передача по BLE', sent, total);
        setState(() {
          _otaProgress = sent / total;
          _otaProgressLabel = label;
          _otaStatus = label;
        });
        _logOtaProgress('upload', sent, total);
      },
    );
    if (!mounted) return;
    final profileRejection = ok
        ? null
        : await _finishProfileRejectionWait(profileRejectWait);
    if (ok) {
      _cancelProfileRejectionWait(profileRejectWait);
    }
    final failureMessage = profileRejection == null
        ? 'OTA отклонено'
        : _profileRejectionMessageFrom(profileRejection);
    setState(() {
      _otaBusy = false;
      _otaProgress = ok ? 1 : _otaProgress;
      _otaStatus = ok ? 'Готово, ESP перезагружается' : failureMessage;
      _otaProgressLabel = ok ? '100.0%' : _otaProgressLabel;
    });
    _logOta(
      ok ? 'upload_done' : 'upload_rejected',
      detail: profileRejection == null
          ? null
          : 'command=${profileRejection.commandName},profile=${profileRejection.profile},required=${profileRejection.requiredProfile}',
    );
    _showMessage(ok ? 'OTA завершено' : failureMessage);
  }

  Future<void> _runReleaseOta() async {
    if (!isConnected || _otaBusy) return;
    if (!_firmwareUpdateClient.configured) {
      _showMessage('Источник релиза не задан в сборке приложения');
      return;
    }

    setState(() {
      _otaBusy = true;
      _otaProgress = null;
      _otaProgressLabel = '';
      _otaStatus = 'Поиск релиза';
      _otaStartedAt = DateTime.now();
      _otaLastLogAt = null;
      _otaLastLogBucket = -1;
      _releaseFirmwareManifest = null;
    });
    _logOta('manifest_fetch_start');
    try {
      final manifest = await _firmwareUpdateClient.fetchLatestManifest();
      if (!mounted) return;
      setState(() {
        _releaseFirmwareManifest = manifest;
        _otaStatus = 'Найдена ${manifest.displayLabel}, скачивание';
      });
      _logOta(
        'manifest_ready',
        total: manifest.size,
        detail: '${manifest.platformioEnv}/${manifest.shortGitSha}',
      );

      final bundle = await _firmwareUpdateClient.downloadFirmware(
        manifest,
        onProgress: (received, total) {
          if (!mounted) return;
          final progress = total > 0 ? received / total : null;
          final label = _formatOtaProgress(
            'Скачивание релиза',
            received,
            total,
          );
          setState(() {
            _otaProgress = progress;
            _otaProgressLabel = label;
            _otaStatus = label;
          });
          _logOtaProgress('download_release', received, total);
        },
      );
      if (!mounted) return;
      await _uploadVerifiedFirmware(bundle.firmware, bundle.manifest.sha256);
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _otaBusy = false;
        _otaStatus = 'Ошибка release OTA';
      });
      _logOta('release_error', detail: e.toString());
      _showMessage('Ошибка release OTA: $e');
    }
  }

  Future<void> _runBleOta() async {
    if (!isConnected || _otaBusy) return;
    final uri = Uri.tryParse(_otaUrlCtrl.text.trim());
    if (uri == null || !uri.hasScheme || !uri.hasAuthority) {
      _showMessage('Нужен URL firmware.bin');
      return;
    }
    final expectedSha = normalizeBoatLockSha256Hex(_otaShaCtrl.text);
    if (expectedSha == null) {
      _showMessage('Нужен SHA-256 из 64 hex-символов');
      return;
    }

    setState(() {
      _otaBusy = true;
      _otaProgress = null;
      _otaProgressLabel = '';
      _otaStatus = 'Скачивание';
      _otaStartedAt = DateTime.now();
      _otaLastLogAt = null;
      _otaLastLogBucket = -1;
    });
    _logOta('download_start', detail: uri.toString());
    try {
      final firmware = await _downloadFirmware(
        uri,
        onProgress: (received, total) {
          if (!mounted) return;
          final progress = total != null && total > 0 ? received / total : null;
          final label = _formatOtaProgress('Скачивание', received, total);
          setState(() {
            _otaProgress = progress;
            _otaProgressLabel = label;
            _otaStatus = label;
          });
          _logOtaProgress('download', received, total);
        },
      );
      if (!mounted) return;
      final actualSha = boatLockSha256Hex(firmware);
      if (actualSha != expectedSha) {
        setState(() {
          _otaBusy = false;
          _otaStatus = 'SHA не совпал';
        });
        _showMessage('SHA не совпал');
        _logOta('sha_mismatch', sent: firmware.length, total: firmware.length);
        return;
      }

      await _uploadVerifiedFirmware(firmware, actualSha);
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _otaBusy = false;
        _otaStatus = 'Ошибка OTA';
      });
      _logOta('error', detail: e.toString());
      _showMessage('Ошибка OTA: $e');
    }
  }

  Widget _securityTile(String label, String value) {
    return ListTile(dense: true, title: Text(label), trailing: Text(value));
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Настройки')),
      body: ListView(
        padding: const EdgeInsets.only(bottom: 24),
        children: [
          SwitchListTile(
            title: const Text('Поддерживать курс носа'),
            value: holdHeading,
            onChanged: isConnected ? _toggleHoldHeading : null,
          ),
          SwitchListTile(
            title: const Text('Сервисный режим'),
            value: serviceMenuEnabled,
            onChanged: _toggleServiceMenu,
          ),
          if (_serviceMenuVisible) ...[
            ListTile(
              title: const Text('Макс. скорость'),
              subtitle: Text(stepMaxSpd.round().toString()),
              trailing: const Icon(Icons.chevron_right),
              enabled: isConnected,
              onTap: isConnected ? _editStepMaxSpd : null,
            ),
            ListTile(
              title: const Text('Ускорение'),
              subtitle: Text(stepAccel.round().toString()),
              trailing: const Icon(Icons.chevron_right),
              enabled: isConnected,
              onTap: isConnected ? _editStepAccel : null,
            ),
            const Divider(),
            ListTile(
              title: const Text('Офсет компаса'),
              subtitle: Text('${compassOffset.toStringAsFixed(1)}°'),
              trailing: const Icon(Icons.chevron_right),
              enabled: isConnected,
              onTap: isConnected ? _editCompassOffset : null,
            ),
            ListTile(
              title: const Text('Сбросить офсет'),
              enabled: isConnected,
              onTap: isConnected ? _resetCompassOffset : null,
            ),
            const Divider(),
          ],
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 8, 16, 4),
            child: Text(
              'Security',
              style: Theme.of(context).textTheme.titleMedium,
            ),
          ),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 16),
            child: TextField(
              controller: _ownerSecretCtrl,
              enabled: isConnected,
              autocorrect: false,
              enableSuggestions: false,
              textCapitalization: TextCapitalization.characters,
              onChanged: (_) => _syncOwnerSecret(),
              decoration: const InputDecoration(
                labelText: 'Owner secret',
                hintText: '32 HEX символа',
                helperText:
                    'Сгенерируй secret, затем открой pairing window кнопкой STOP на устройстве.',
              ),
            ),
          ),
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 8, 16, 0),
            child: Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                OutlinedButton(
                  onPressed: isConnected ? _generateOwnerSecret : null,
                  child: const Text('Сгенерировать'),
                ),
                FilledButton(
                  onPressed: isConnected ? _pairDevice : null,
                  child: const Text('Привязать'),
                ),
                OutlinedButton(
                  onPressed: isConnected ? _authenticateOwner : null,
                  child: const Text('Авторизоваться'),
                ),
                OutlinedButton(
                  onPressed: isConnected ? _clearPairing : null,
                  child: const Text('Сбросить pairing'),
                ),
              ],
            ),
          ),
          _securityTile('Paired', secPaired ? 'YES' : 'NO'),
          _securityTile('Auth', secAuth ? 'YES' : 'NO'),
          _securityTile('Pair window', secPairWindowOpen ? 'OPEN' : 'CLOSED'),
          _securityTile('Last reject', secReject),
          if (_serviceMenuVisible) ...[
            const Divider(),
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 8, 16, 4),
              child: Text(
                'Firmware OTA',
                style: Theme.of(context).textTheme.titleMedium,
              ),
            ),
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16),
              child: TextField(
                controller: _otaUrlCtrl,
                enabled: isConnected && !_otaBusy,
                autocorrect: false,
                enableSuggestions: false,
                keyboardType: TextInputType.url,
                decoration: const InputDecoration(
                  labelText: 'Firmware URL',
                  hintText: 'https://.../firmware.bin',
                ),
              ),
            ),
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 8, 16, 0),
              child: TextField(
                controller: _otaShaCtrl,
                enabled: isConnected && !_otaBusy,
                autocorrect: false,
                enableSuggestions: false,
                textCapitalization: TextCapitalization.characters,
                decoration: const InputDecoration(
                  labelText: 'SHA-256',
                  hintText: '64 HEX символа',
                ),
              ),
            ),
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 8, 16, 0),
              child: Row(
                children: [
                  FilledButton(
                    onPressed: isConnected && !_otaBusy ? _runBleOta : null,
                    child: const Text('Обновить по BLE'),
                  ),
                  const SizedBox(width: 12),
                  Expanded(child: Text(_otaStatus)),
                ],
              ),
            ),
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 8, 16, 0),
              child: Wrap(
                spacing: 8,
                runSpacing: 8,
                crossAxisAlignment: WrapCrossAlignment.center,
                children: [
                  FilledButton.icon(
                    onPressed:
                        isConnected &&
                            !_otaBusy &&
                            _firmwareUpdateClient.configured
                        ? _runReleaseOta
                        : null,
                    icon: const Icon(Icons.system_update_alt),
                    label: const Text('Обновить до релиза'),
                  ),
                  if (_releaseFirmwareManifest != null)
                    Text(
                      '${_releaseFirmwareManifest!.displayLabel} '
                      '${_releaseFirmwareManifest!.platformioEnv}',
                    )
                  else if (!_firmwareUpdateClient.configured)
                    const Text('Источник релиза не задан'),
                ],
              ),
            ),
            if (_otaProgress != null)
              Padding(
                padding: const EdgeInsets.fromLTRB(16, 8, 16, 0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    LinearProgressIndicator(value: _otaProgress),
                    if (_otaProgressLabel.isNotEmpty) ...[
                      const SizedBox(height: 6),
                      Text(
                        _otaProgressLabel,
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ],
                  ],
                ),
              ),
            const Divider(),
            ListTile(
              title: const Text('BNO08x quality'),
              subtitle: Text('RV=$compassQ MAG=$magQ GYR=$gyroQ'),
            ),
            ListTile(
              title: const Text('RV accuracy'),
              subtitle: Text('${rvAcc.toStringAsFixed(2)}°'),
            ),
            ListTile(
              title: const Text('Mag / Gyro'),
              subtitle: Text(
                '|B|=${magNorm.toStringAsFixed(1)} uT, |w|=${gyroNorm.toStringAsFixed(1)} dps',
              ),
            ),
            ListTile(
              title: const Text('Pitch / Roll'),
              subtitle: Text(
                '${pitch.toStringAsFixed(1)}° / ${roll.toStringAsFixed(1)}°',
              ),
            ),
          ],
        ],
      ),
    );
  }
}

class _ProfileRejectionWait {
  _ProfileRejectionWait({
    required this.baselineEvents,
    required this.commandPrefixes,
  });

  final int baselineEvents;
  final List<String> commandPrefixes;
  final Completer<BleCommandRejection?> completer =
      Completer<BleCommandRejection?>();
}
