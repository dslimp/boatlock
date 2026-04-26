import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import '../ble/ble_ota_payload.dart';
import 'firmware_update_manifest.dart';
import 'github_release_firmware_source.dart';

const String kBoatLockFirmwareUpdateManifestUrl = String.fromEnvironment(
  'BOATLOCK_FIRMWARE_UPDATE_MANIFEST_URL',
);
const String kBoatLockFirmwareUpdateGithubRepo = String.fromEnvironment(
  'BOATLOCK_FIRMWARE_UPDATE_GITHUB_REPO',
);

class FirmwareUpdateBundle {
  final FirmwareUpdateManifest manifest;
  final Uint8List firmware;

  const FirmwareUpdateBundle({required this.manifest, required this.firmware});
}

class FirmwareUpdateClient {
  const FirmwareUpdateClient({
    this.manifestUrl = kBoatLockFirmwareUpdateManifestUrl,
    this.githubRepository = kBoatLockFirmwareUpdateGithubRepo,
    this.githubApiBaseUrl = 'https://api.github.com',
    this.githubToken,
  });

  final String manifestUrl;
  final String githubRepository;
  final String githubApiBaseUrl;
  final String? githubToken;

  bool get configured =>
      manifestUrl.trim().isNotEmpty || githubRepository.trim().isNotEmpty;

  Future<FirmwareUpdateManifest> fetchLatestManifest() async {
    if (manifestUrl.trim().isNotEmpty) {
      final uri = _configuredManifestUri();
      final text = await _downloadText(uri);
      return FirmwareUpdateManifest.fromJsonText(text);
    }
    if (githubRepository.trim().isNotEmpty) {
      return GitHubReleaseFirmwareSource(
        repository: githubRepository.trim(),
        apiBaseUrl: githubApiBaseUrl,
        token: githubToken,
      ).fetchLatestManifest();
    }
    throw const FormatException('firmware manifest URL is not configured');
  }

  Future<FirmwareUpdateBundle> downloadFirmware(
    FirmwareUpdateManifest manifest, {
    void Function(int receivedBytes, int totalBytes)? onProgress,
  }) async {
    final bytes = await _downloadBytes(
      manifest.binaryUrl,
      onProgress: (received, total) {
        onProgress?.call(received, total ?? manifest.size);
      },
    );
    if (bytes.length != manifest.size) {
      throw StateError(
        'firmware size mismatch: got ${bytes.length}, expected ${manifest.size}',
      );
    }
    final actualSha = boatLockSha256Hex(bytes);
    if (actualSha != manifest.sha256) {
      throw StateError('firmware SHA mismatch');
    }
    return FirmwareUpdateBundle(manifest: manifest, firmware: bytes);
  }

  Future<FirmwareUpdateBundle> fetchLatestFirmware({
    void Function(int receivedBytes, int totalBytes)? onProgress,
  }) async {
    final manifest = await fetchLatestManifest();
    return downloadFirmware(manifest, onProgress: onProgress);
  }

  Uri _configuredManifestUri() {
    final uri = Uri.tryParse(manifestUrl.trim());
    if (uri == null || !uri.hasScheme || !uri.hasAuthority) {
      throw const FormatException('firmware manifest URL is not configured');
    }
    return uri;
  }
}

Future<String> _downloadText(Uri uri) async {
  final bytes = await _downloadBytes(uri);
  return utf8.decode(bytes);
}

Future<Uint8List> _downloadBytes(
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
