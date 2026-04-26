import 'dart:convert';

import '../ble/ble_ota_payload.dart';

class FirmwareUpdateManifest {
  final int schema;
  final String channel;
  final String repo;
  final String branch;
  final String gitSha;
  final int? workflowRunId;
  final String firmwareVersion;
  final String platformioEnv;
  final String commandProfile;
  final String artifactName;
  final Uri binaryUrl;
  final int size;
  final String sha256;
  final DateTime? builtAt;

  const FirmwareUpdateManifest({
    required this.schema,
    required this.channel,
    required this.repo,
    required this.branch,
    required this.gitSha,
    required this.workflowRunId,
    required this.firmwareVersion,
    required this.platformioEnv,
    required this.commandProfile,
    required this.artifactName,
    required this.binaryUrl,
    required this.size,
    required this.sha256,
    required this.builtAt,
  });

  String get shortGitSha =>
      gitSha.length <= 7 ? gitSha : gitSha.substring(0, 7);

  String get displayLabel => '$firmwareVersion $shortGitSha';

  factory FirmwareUpdateManifest.fromJsonText(String text) {
    final decoded = jsonDecode(text);
    if (decoded is! Map<String, dynamic>) {
      throw const FormatException('manifest root must be an object');
    }
    return FirmwareUpdateManifest.fromJson(decoded);
  }

  factory FirmwareUpdateManifest.fromJson(Map<String, dynamic> json) {
    final schema = _intField(json, 'schema');
    final channel = _stringField(json, 'channel');
    final repo = _stringField(json, 'repo');
    final branch = _stringField(json, 'branch');
    final gitSha = _stringField(json, 'gitSha');
    final workflowRunId = _optionalIntField(json, 'workflowRunId');
    final firmwareVersion = _stringField(json, 'firmwareVersion');
    final platformioEnv = _stringField(json, 'platformioEnv');
    final commandProfile = _stringField(json, 'commandProfile');
    final artifactName = _stringField(json, 'artifactName');
    final binaryUrl = Uri.tryParse(_stringField(json, 'binaryUrl'));
    final size = _intField(json, 'size');
    final sha256 = normalizeBoatLockSha256Hex(_stringField(json, 'sha256'));
    final builtAtText = json['builtAt'];
    final builtAt = builtAtText == null
        ? null
        : DateTime.tryParse(_stringField(json, 'builtAt'));

    if (schema != 1) {
      throw FormatException('unsupported manifest schema $schema');
    }
    if (channel != 'main') {
      throw FormatException('unsupported firmware channel $channel');
    }
    if (repo != 'dslimp/boatlock') {
      throw FormatException('unexpected firmware repo $repo');
    }
    if (branch != 'main') {
      throw FormatException('unexpected firmware branch $branch');
    }
    if (!_isHexSha(gitSha)) {
      throw const FormatException('gitSha must be hex');
    }
    if (platformioEnv != 'esp32s3_service') {
      throw FormatException(
        'OTA main channel must use esp32s3_service, got $platformioEnv',
      );
    }
    if (commandProfile != 'service') {
      throw FormatException(
        'OTA main channel must use service profile, got $commandProfile',
      );
    }
    if (binaryUrl == null || !binaryUrl.hasScheme || !binaryUrl.hasAuthority) {
      throw const FormatException('binaryUrl must be absolute');
    }
    if (!_isAllowedBinaryUrl(binaryUrl)) {
      throw const FormatException('binaryUrl must use HTTPS or localhost HTTP');
    }
    if (size <= 0) {
      throw const FormatException('firmware size must be positive');
    }
    if (sha256 == null) {
      throw const FormatException('sha256 must be 64 hex chars');
    }
    if (builtAtText != null && builtAt == null) {
      throw const FormatException('builtAt must be ISO-8601');
    }

    return FirmwareUpdateManifest(
      schema: schema,
      channel: channel,
      repo: repo,
      branch: branch,
      gitSha: gitSha,
      workflowRunId: workflowRunId,
      firmwareVersion: firmwareVersion,
      platformioEnv: platformioEnv,
      commandProfile: commandProfile,
      artifactName: artifactName,
      binaryUrl: binaryUrl,
      size: size,
      sha256: sha256,
      builtAt: builtAt,
    );
  }
}

bool _isAllowedBinaryUrl(Uri uri) {
  if (uri.scheme == 'https') return true;
  if (uri.scheme != 'http') return false;
  final host = uri.host.toLowerCase();
  return host == 'localhost' || host == '127.0.0.1' || host == '::1';
}

bool _isHexSha(String value) {
  return RegExp(r'^[0-9a-fA-F]{7,40}$').hasMatch(value);
}

String _stringField(Map<String, dynamic> json, String key) {
  final value = json[key];
  if (value is String && value.trim().isNotEmpty) {
    return value.trim();
  }
  throw FormatException('$key must be a non-empty string');
}

int _intField(Map<String, dynamic> json, String key) {
  final value = json[key];
  if (value is int) return value;
  throw FormatException('$key must be an integer');
}

int? _optionalIntField(Map<String, dynamic> json, String key) {
  final value = json[key];
  if (value == null) return null;
  if (value is int) return value;
  throw FormatException('$key must be an integer');
}
