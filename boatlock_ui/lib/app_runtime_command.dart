import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

class BoatLockRuntimeCommand {
  const BoatLockRuntimeCommand({
    this.mode = '',
    this.otaUrl = '',
    this.otaSha256 = '',
    this.otaLatestRelease = false,
    this.firmwareManifestUrl = '',
  });

  static const MethodChannel _channel = MethodChannel('boatlock/runtime');

  static const String modeKey = 'boatlock_check_mode';
  static const String otaUrlKey = 'boatlock_ota_url';
  static const String otaSha256Key = 'boatlock_ota_sha256';
  static const String otaLatestReleaseKey = 'boatlock_ota_latest_release';
  static const String firmwareManifestUrlKey = 'boatlock_firmware_manifest_url';

  final String mode;
  final String otaUrl;
  final String otaSha256;
  final bool otaLatestRelease;
  final String firmwareManifestUrl;

  bool get enabled => mode.trim().isNotEmpty;

  static Future<BoatLockRuntimeCommand> readInitial() async {
    if (kIsWeb || defaultTargetPlatform != TargetPlatform.android) {
      return const BoatLockRuntimeCommand();
    }
    try {
      final raw = await _channel.invokeMapMethod<String, Object?>(
        'initialCommand',
      );
      return BoatLockRuntimeCommand.fromMap(raw);
    } catch (_) {
      return const BoatLockRuntimeCommand();
    }
  }

  factory BoatLockRuntimeCommand.fromMap(Map<String, Object?>? raw) {
    if (raw == null) {
      return const BoatLockRuntimeCommand();
    }
    return BoatLockRuntimeCommand(
      mode: _string(raw[modeKey]),
      otaUrl: _string(raw[otaUrlKey]),
      otaSha256: _string(raw[otaSha256Key]),
      otaLatestRelease: raw[otaLatestReleaseKey] == true,
      firmwareManifestUrl: _string(raw[firmwareManifestUrlKey]),
    );
  }

  static String _string(Object? value) => value is String ? value.trim() : '';
}
