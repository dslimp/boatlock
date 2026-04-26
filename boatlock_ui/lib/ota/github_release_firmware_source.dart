import 'dart:convert';
import 'dart:io';

import '../ble/ble_ota_payload.dart';
import 'firmware_update_manifest.dart';

typedef GitHubReleaseTextFetcher =
    Future<String> Function(Uri uri, {Map<String, String> headers});

class GitHubReleaseFirmwareSource {
  final String repository;
  final String apiBaseUrl;
  final String? token;
  final GitHubReleaseTextFetcher textFetcher;

  const GitHubReleaseFirmwareSource({
    required this.repository,
    this.apiBaseUrl = 'https://api.github.com',
    this.token,
    this.textFetcher = _defaultGitHubReleaseTextFetcher,
  });

  Uri get latestReleaseUri {
    final apiBaseUri = Uri.tryParse(apiBaseUrl.trim());
    if (apiBaseUri == null ||
        !apiBaseUri.hasScheme ||
        !apiBaseUri.hasAuthority) {
      throw const FormatException('GitHub API base URL must be absolute');
    }
    final parts = repository.split('/');
    if (parts.length != 2 || parts.any((part) => part.trim().isEmpty)) {
      throw const FormatException('GitHub repository must be owner/name');
    }
    final baseSegments = apiBaseUri.pathSegments
        .where((segment) => segment.isNotEmpty)
        .toList();
    return apiBaseUri.replace(
      pathSegments: [
        ...baseSegments,
        'repos',
        parts[0].trim(),
        parts[1].trim(),
        'releases',
        'latest',
      ],
    );
  }

  Future<FirmwareUpdateManifest> fetchLatestManifest() async {
    final releaseText = await textFetcher(
      latestReleaseUri,
      headers: _jsonHeaders,
    );
    final release = GitHubFirmwareRelease.fromJsonText(releaseText);
    return resolveManifest(release);
  }

  Future<FirmwareUpdateManifest> resolveManifest(
    GitHubFirmwareRelease release,
  ) async {
    final manifestAsset = release.preferredManifestAsset;
    if (manifestAsset != null) {
      final manifestText = await textFetcher(
        manifestAsset.downloadUri(authenticated: _authenticated),
        headers: _assetHeaders,
      );
      return FirmwareUpdateManifest.fromJsonText(manifestText);
    }

    final firmwareAsset = release.firmwareAsset;
    if (firmwareAsset == null) {
      throw const FormatException('latest release has no firmware.bin asset');
    }

    final buildInfoAsset = release.buildInfoAsset;
    if (buildInfoAsset == null) {
      throw const FormatException(
        'latest release fallback requires BUILD_INFO.txt',
      );
    }
    final buildInfo = _parseBuildInfo(
      await textFetcher(
        buildInfoAsset.downloadUri(authenticated: _authenticated),
        headers: _assetHeaders,
      ),
    );

    String? checksumSha;
    final checksumAsset = release.sha256SumsAsset;
    if (checksumAsset != null) {
      checksumSha = _shaForFirmwareBin(
        await textFetcher(
          checksumAsset.downloadUri(authenticated: _authenticated),
          headers: _assetHeaders,
        ),
      );
    }

    final digestSha = firmwareAsset.sha256Digest;
    final buildInfoSha = normalizeBoatLockSha256Hex(
      buildInfo['firmware_sha256'] ?? '',
    );
    final sha256 = _chooseSha256(
      digestSha: digestSha,
      checksumSha: checksumSha,
      buildInfoSha: buildInfoSha,
    );
    if (sha256 == null) {
      throw const FormatException(
        'latest release fallback requires firmware SHA-256',
      );
    }

    final gitSha = (buildInfo['github_sha'] ?? buildInfo['git_sha'] ?? '')
        .trim();
    final platformioEnv = (buildInfo['platformio_env'] ?? '').trim();
    final commandProfile =
        (buildInfo['command_profile'] ?? _profileForEnv(platformioEnv)).trim();
    final branch = _releaseBranch(release, buildInfo);

    return FirmwareUpdateManifest.fromJson({
      'schema': 1,
      'channel': 'main',
      'repo': repository,
      'branch': branch,
      'gitSha': gitSha,
      'workflowRunId': _optionalInt(
        buildInfo['workflow_run_id'] ?? buildInfo['github_run_id'],
      ),
      'firmwareVersion': _firmwareVersion(release, buildInfo),
      'platformioEnv': platformioEnv,
      'commandProfile': commandProfile,
      'artifactName': (buildInfo['artifact_name'] ?? firmwareAsset.name).trim(),
      'binaryUrl': firmwareAsset
          .downloadUri(authenticated: _authenticated)
          .toString(),
      'size': firmwareAsset.size,
      'sha256': sha256,
      'builtAt': release.publishedAt?.toUtc().toIso8601String(),
    });
  }

  Map<String, String> get _jsonHeaders {
    final headers = <String, String>{
      'Accept': 'application/vnd.github+json',
      'X-GitHub-Api-Version': '2022-11-28',
      'User-Agent': 'BoatLock',
    };
    final trimmedToken = token?.trim();
    if (trimmedToken != null && trimmedToken.isNotEmpty) {
      headers['Authorization'] = 'Bearer $trimmedToken';
    }
    return headers;
  }

  bool get _authenticated => token != null && token!.trim().isNotEmpty;

  Map<String, String> get _assetHeaders {
    final headers = <String, String>{
      'Accept': 'application/octet-stream',
      'User-Agent': 'BoatLock',
    };
    final trimmedToken = token?.trim();
    if (trimmedToken != null && trimmedToken.isNotEmpty) {
      headers['Authorization'] = 'Bearer $trimmedToken';
    }
    return headers;
  }
}

class GitHubFirmwareRelease {
  final String tagName;
  final String targetCommitish;
  final DateTime? publishedAt;
  final List<GitHubFirmwareReleaseAsset> assets;

  const GitHubFirmwareRelease({
    required this.tagName,
    required this.targetCommitish,
    required this.publishedAt,
    required this.assets,
  });

  GitHubFirmwareReleaseAsset? get preferredManifestAsset =>
      _firstAssetNamed(const [
        'manifest.json',
        'firmware_update_manifest.json',
        'firmware-update-manifest.json',
      ]);

  GitHubFirmwareReleaseAsset? get firmwareAsset =>
      _firstAssetNamed(const ['firmware.bin']);

  GitHubFirmwareReleaseAsset? get sha256SumsAsset =>
      _firstAssetNamed(const ['sha256sums', 'sha256sums.txt']);

  GitHubFirmwareReleaseAsset? get buildInfoAsset =>
      _firstAssetNamed(const ['build_info.txt']);

  factory GitHubFirmwareRelease.fromJsonText(String text) {
    final decoded = jsonDecode(text);
    if (decoded is! Map<String, dynamic>) {
      throw const FormatException('GitHub release root must be an object');
    }
    return GitHubFirmwareRelease.fromJson(decoded);
  }

  factory GitHubFirmwareRelease.fromJson(Map<String, dynamic> json) {
    final assetsValue = json['assets'];
    if (assetsValue is! List) {
      throw const FormatException('GitHub release assets must be a list');
    }
    return GitHubFirmwareRelease(
      tagName: _stringField(json, 'tag_name'),
      targetCommitish: _stringField(json, 'target_commitish'),
      publishedAt: _optionalDateTime(json['published_at'], 'published_at'),
      assets: assetsValue
          .whereType<Map<String, dynamic>>()
          .map(GitHubFirmwareReleaseAsset.fromJson)
          .toList(growable: false),
    );
  }

  GitHubFirmwareReleaseAsset? _firstAssetNamed(List<String> names) {
    for (final name in names) {
      for (final asset in assets) {
        if (asset.normalizedName == name) return asset;
      }
    }
    return null;
  }
}

class GitHubFirmwareReleaseAsset {
  final String name;
  final Uri browserDownloadUrl;
  final Uri? apiUrl;
  final int size;
  final String? digest;

  const GitHubFirmwareReleaseAsset({
    required this.name,
    required this.browserDownloadUrl,
    required this.apiUrl,
    required this.size,
    required this.digest,
  });

  String get normalizedName => name.trim().toLowerCase();

  Uri downloadUri({required bool authenticated}) {
    if (authenticated && apiUrl != null) return apiUrl!;
    return browserDownloadUrl;
  }

  String? get sha256Digest {
    final value = digest?.trim() ?? '';
    const prefix = 'sha256:';
    if (!value.toLowerCase().startsWith(prefix)) return null;
    return normalizeBoatLockSha256Hex(value.substring(prefix.length));
  }

  factory GitHubFirmwareReleaseAsset.fromJson(Map<String, dynamic> json) {
    final browserDownloadUrl = Uri.tryParse(
      _stringField(json, 'browser_download_url'),
    );
    if (browserDownloadUrl == null ||
        !browserDownloadUrl.hasScheme ||
        !browserDownloadUrl.hasAuthority) {
      throw const FormatException(
        'GitHub release asset browser_download_url must be absolute',
      );
    }
    final apiUrlText = json['url'];
    final apiUrl = apiUrlText is String ? Uri.tryParse(apiUrlText) : null;
    final size = _intField(json, 'size');
    if (size <= 0) {
      throw const FormatException('GitHub release asset size must be positive');
    }
    final digestValue = json['digest'];
    return GitHubFirmwareReleaseAsset(
      name: _stringField(json, 'name'),
      browserDownloadUrl: browserDownloadUrl,
      apiUrl: apiUrl,
      size: size,
      digest: digestValue is String ? digestValue : null,
    );
  }
}

Future<String> _defaultGitHubReleaseTextFetcher(
  Uri uri, {
  Map<String, String> headers = const {},
}) async {
  final client = HttpClient();
  client.connectionTimeout = const Duration(seconds: 15);
  try {
    final request = await client.getUrl(uri);
    headers.forEach(request.headers.set);
    final response = await request.close();
    if (response.statusCode != HttpStatus.ok) {
      throw HttpException('HTTP ${response.statusCode}', uri: uri);
    }
    return utf8.decode(
      await response.fold<List<int>>(
        <int>[],
        (buffer, chunk) => buffer..addAll(chunk),
      ),
    );
  } finally {
    client.close(force: true);
  }
}

Map<String, String> _parseBuildInfo(String text) {
  final values = <String, String>{};
  for (final rawLine in const LineSplitter().convert(text)) {
    final line = rawLine.trim();
    if (line.isEmpty || line.startsWith('#')) continue;
    final equals = line.indexOf('=');
    if (equals <= 0) continue;
    values[line.substring(0, equals).trim()] = line
        .substring(equals + 1)
        .trim();
  }
  return values;
}

String? _shaForFirmwareBin(String text) {
  for (final rawLine in const LineSplitter().convert(text)) {
    final match = RegExp(
      r'^([0-9a-fA-F]{64})\s+\*?(.+)$',
    ).firstMatch(rawLine.trim());
    if (match == null) continue;
    final path = match.group(2)!.trim().replaceAll('\\', '/');
    final name = path.split('/').last.toLowerCase();
    if (name == 'firmware.bin') {
      return normalizeBoatLockSha256Hex(match.group(1)!);
    }
  }
  return null;
}

String? _chooseSha256({
  required String? digestSha,
  required String? checksumSha,
  required String? buildInfoSha,
}) {
  final candidates = <String>[];
  if (digestSha != null) candidates.add(digestSha);
  if (checksumSha != null) candidates.add(checksumSha);
  if (buildInfoSha != null) candidates.add(buildInfoSha);
  if (candidates.isEmpty) return null;
  final first = candidates.first;
  if (candidates.any((candidate) => candidate != first)) {
    throw const FormatException('firmware SHA-256 metadata disagrees');
  }
  return first;
}

String _releaseBranch(
  GitHubFirmwareRelease release,
  Map<String, String> buildInfo,
) {
  final githubRef = (buildInfo['github_ref'] ?? '').trim();
  const headPrefix = 'refs/heads/';
  if (githubRef.startsWith(headPrefix)) {
    return githubRef.substring(headPrefix.length);
  }
  return release.targetCommitish.trim();
}

String _firmwareVersion(
  GitHubFirmwareRelease release,
  Map<String, String> buildInfo,
) {
  final version = (buildInfo['firmware_version'] ?? '').trim();
  if (version.isNotEmpty) return version;
  final tag = release.tagName.trim();
  return tag.startsWith('v') ? tag.substring(1) : tag;
}

String _profileForEnv(String platformioEnv) {
  if (platformioEnv == 'esp32s3_service') return 'service';
  if (platformioEnv == 'esp32s3_release' || platformioEnv == 'esp32s3') {
    return 'release';
  }
  return '';
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

DateTime? _optionalDateTime(Object? value, String key) {
  if (value == null) return null;
  if (value is! String) throw FormatException('$key must be ISO-8601');
  final parsed = DateTime.tryParse(value);
  if (parsed == null) throw FormatException('$key must be ISO-8601');
  return parsed;
}

int? _optionalInt(String? value) {
  if (value == null || value.trim().isEmpty) return null;
  return int.tryParse(value.trim());
}
