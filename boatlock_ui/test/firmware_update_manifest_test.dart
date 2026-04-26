import 'package:boatlock_ui/ota/firmware_update_manifest.dart';
import 'package:flutter_test/flutter_test.dart';

const _sha = '00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff';

Map<String, dynamic> _manifestJson({
  String channel = 'main',
  String branch = 'main',
  String platformioEnv = 'esp32s3_service',
  String commandProfile = 'service',
  String binaryUrl = 'https://example.com/firmware/main/firmware.bin',
  String sha256 = _sha,
  int size = 711776,
}) {
  return {
    'schema': 1,
    'channel': channel,
    'repo': 'dslimp/boatlock',
    'branch': branch,
    'gitSha': '10d54f1abcdef1234567890abcdef1234567890',
    'workflowRunId': 123,
    'firmwareVersion': '0.2.0',
    'platformioEnv': platformioEnv,
    'commandProfile': commandProfile,
    'artifactName': 'firmware-esp32s3-service',
    'binaryUrl': binaryUrl,
    'size': size,
    'sha256': sha256,
    'builtAt': '2026-04-26T12:00:00Z',
  };
}

void main() {
  test('parses main service firmware manifest', () {
    final manifest = FirmwareUpdateManifest.fromJson(_manifestJson());

    expect(manifest.channel, 'main');
    expect(manifest.branch, 'main');
    expect(manifest.platformioEnv, 'esp32s3_service');
    expect(manifest.commandProfile, 'service');
    expect(manifest.sha256, _sha);
    expect(manifest.shortGitSha, '10d54f1');
    expect(manifest.displayLabel, '0.2.0 10d54f1');
  });

  test('parses release service firmware manifest', () {
    final manifest = FirmwareUpdateManifest.fromJson(
      _manifestJson(
        channel: 'release',
        branch: 'release/v0.2.x',
        binaryUrl:
            'https://github.com/dslimp/boatlock/releases/download/v0.2.0/firmware-esp32s3-service.bin',
      ),
    );

    expect(manifest.channel, 'release');
    expect(manifest.branch, 'release/v0.2.x');
    expect(manifest.platformioEnv, 'esp32s3_service');
    expect(manifest.commandProfile, 'service');
  });

  test('rejects invalid branch/channel and non-service manifests', () {
    expect(
      () => FirmwareUpdateManifest.fromJson(_manifestJson(branch: 'feature/x')),
      throwsFormatException,
    );
    expect(
      () => FirmwareUpdateManifest.fromJson(
        _manifestJson(channel: 'release', branch: 'release/v0.3.x'),
      ),
      throwsFormatException,
    );
    expect(
      () => FirmwareUpdateManifest.fromJson(
        _manifestJson(platformioEnv: 'esp32s3_release'),
      ),
      throwsFormatException,
    );
    expect(
      () => FirmwareUpdateManifest.fromJson(
        _manifestJson(commandProfile: 'release'),
      ),
      throwsFormatException,
    );
  });

  test('rejects unsafe URLs and invalid hashes', () {
    expect(
      () => FirmwareUpdateManifest.fromJson(
        _manifestJson(binaryUrl: 'http://example.com/firmware.bin'),
      ),
      throwsFormatException,
    );
    expect(
      () => FirmwareUpdateManifest.fromJson(
        _manifestJson(binaryUrl: 'http://127.0.0.1:8080/firmware.bin'),
      ),
      returnsNormally,
    );
    expect(
      () => FirmwareUpdateManifest.fromJson(_manifestJson(sha256: 'bad')),
      throwsFormatException,
    );
    expect(
      () => FirmwareUpdateManifest.fromJson(_manifestJson(size: 0)),
      throwsFormatException,
    );
  });
}
