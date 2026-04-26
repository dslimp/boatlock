import 'dart:convert';

import 'package:boatlock_ui/ota/firmware_update_client.dart';
import 'package:boatlock_ui/ota/github_release_firmware_source.dart';
import 'package:flutter_test/flutter_test.dart';

const _sha = '00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff';
const _otherSha =
    'ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff';
const _gitSha = '10d54f1abcdef1234567890abcdef1234567890';

Map<String, dynamic> _manifestJson() {
  return {
    'schema': 1,
    'channel': 'main',
    'repo': 'dslimp/boatlock',
    'branch': 'main',
    'gitSha': _gitSha,
    'workflowRunId': 123,
    'firmwareVersion': '0.2.0',
    'platformioEnv': 'esp32s3_service',
    'commandProfile': 'service',
    'artifactName': 'firmware-esp32s3-service',
    'binaryUrl':
        'https://github.com/dslimp/boatlock/releases/download/v0.2.0/firmware.bin',
    'size': 711776,
    'sha256': _sha,
    'builtAt': '2026-04-26T12:00:00Z',
  };
}

Map<String, dynamic> _releaseJson(List<Map<String, dynamic>> assets) {
  return {
    'tag_name': 'v0.2.0',
    'target_commitish': 'main',
    'published_at': '2026-04-26T12:00:00Z',
    'assets': assets,
  };
}

Map<String, dynamic> _asset(String name, {int size = 10, String? digest}) {
  final asset = <String, dynamic>{
    'name': name,
    'url': 'https://api.github.com/repos/dslimp/boatlock/releases/assets/$name',
    'browser_download_url':
        'https://github.com/dslimp/boatlock/releases/download/v0.2.0/$name',
    'size': size,
  };
  if (digest != null) asset['digest'] = digest;
  return asset;
}

GitHubReleaseTextFetcher _fetcher(Map<String, String> responses) {
  return (uri, {headers = const {}}) async {
    final response =
        responses[uri.toString()] ??
        (throw StateError('unexpected fetch $uri'));
    return response;
  };
}

void main() {
  test('fetches latest release and prefers manifest asset', () async {
    final releaseUrl =
        'https://api.github.com/repos/dslimp/boatlock/releases/latest';
    final manifestUrl =
        'https://github.com/dslimp/boatlock/releases/download/v0.2.0/manifest.json';
    final source = GitHubReleaseFirmwareSource(
      repository: 'dslimp/boatlock',
      textFetcher: _fetcher({
        releaseUrl: jsonEncode(
          _releaseJson([
            _asset('firmware.bin', size: 711776, digest: 'sha256:$_sha'),
            _asset('manifest.json'),
          ]),
        ),
        manifestUrl: jsonEncode(_manifestJson()),
      }),
    );

    final manifest = await source.fetchLatestManifest();

    expect(manifest.artifactName, 'firmware-esp32s3-service');
    expect(manifest.binaryUrl.toString(), _manifestJson()['binaryUrl']);
    expect(manifest.sha256, _sha);
  });

  test('passes GitHub token to release and asset requests', () async {
    final headersByUrl = <String, Map<String, String>>{};
    final releaseUrl =
        'https://api.github.com/repos/dslimp/boatlock/releases/latest';
    final manifestUrl =
        'https://github.com/dslimp/boatlock/releases/download/v0.2.0/manifest.json';
    final source = GitHubReleaseFirmwareSource(
      repository: 'dslimp/boatlock',
      token: 'token-123',
      textFetcher: (uri, {headers = const {}}) async {
        headersByUrl[uri.toString()] = headers;
        if (uri.toString() == releaseUrl) {
          return jsonEncode(
            _releaseJson([
              _asset('firmware.bin', size: 711776, digest: 'sha256:$_sha'),
              _asset('manifest.json'),
            ]),
          );
        }
        if (uri.toString() == manifestUrl) {
          return jsonEncode(_manifestJson());
        }
        throw StateError('unexpected fetch $uri');
      },
    );

    await source.fetchLatestManifest();

    expect(headersByUrl[releaseUrl]?['Authorization'], 'Bearer token-123');
    expect(headersByUrl[manifestUrl]?['Authorization'], 'Bearer token-123');
  });

  test(
    'builds service manifest from firmware, checksums, and build info',
    () async {
      final source = GitHubReleaseFirmwareSource(
        repository: 'dslimp/boatlock',
        textFetcher: _fetcher({
          'https://github.com/dslimp/boatlock/releases/download/v0.2.0/SHA256SUMS':
              '$_sha  firmware.bin\n',
          'https://github.com/dslimp/boatlock/releases/download/v0.2.0/BUILD_INFO.txt':
              [
                'firmware_version=0.2.0',
                'firmware_sha256=$_sha',
                'artifact_name=firmware-esp32s3-service',
                'platformio_env=esp32s3_service',
                'command_profile=service',
                'github_ref=refs/heads/main',
                'github_sha=$_gitSha',
              ].join('\n'),
        }),
      );
      final release = GitHubFirmwareRelease.fromJson(
        _releaseJson([
          _asset('firmware.bin', size: 711776, digest: 'sha256:$_sha'),
          _asset('BUILD_INFO.txt'),
          _asset('SHA256SUMS'),
        ]),
      );

      final manifest = await source.resolveManifest(release);

      expect(manifest.channel, 'main');
      expect(manifest.branch, 'main');
      expect(manifest.gitSha, _gitSha);
      expect(manifest.firmwareVersion, '0.2.0');
      expect(manifest.platformioEnv, 'esp32s3_service');
      expect(manifest.commandProfile, 'service');
      expect(manifest.size, 711776);
      expect(manifest.sha256, _sha);
    },
  );

  test('rejects fallback when release metadata hashes disagree', () async {
    final source = GitHubReleaseFirmwareSource(
      repository: 'dslimp/boatlock',
      textFetcher: _fetcher({
        'https://github.com/dslimp/boatlock/releases/download/v0.2.0/SHA256SUMS':
            '$_otherSha  firmware.bin\n',
        'https://github.com/dslimp/boatlock/releases/download/v0.2.0/BUILD_INFO.txt':
            [
              'firmware_version=0.2.0',
              'firmware_sha256=$_sha',
              'artifact_name=firmware-esp32s3-service',
              'platformio_env=esp32s3_service',
              'command_profile=service',
              'github_ref=refs/heads/main',
              'github_sha=$_gitSha',
            ].join('\n'),
      }),
    );
    final release = GitHubFirmwareRelease.fromJson(
      _releaseJson([
        _asset('firmware.bin', size: 711776, digest: 'sha256:$_sha'),
        _asset('BUILD_INFO.txt'),
        _asset('SHA256SUMS'),
      ]),
    );

    await expectLater(source.resolveManifest(release), throwsFormatException);
  });

  test('client is configured by manifest URL or GitHub release repository', () {
    expect(const FirmwareUpdateClient().configured, isFalse);
    expect(
      const FirmwareUpdateClient(
        manifestUrl: 'https://example.com/m.json',
      ).configured,
      isTrue,
    );
    expect(
      const FirmwareUpdateClient(
        githubRepository: 'dslimp/boatlock',
      ).configured,
      isTrue,
    );
  });
}
