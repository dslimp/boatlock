import 'dart:typed_data';

import 'package:boatlock_ui/ble/ble_boatlock.dart';
import 'package:boatlock_ui/ble/ble_command_rejection.dart';
import 'package:boatlock_ui/ble/ble_command_scope.dart';
import 'package:boatlock_ui/ble/ble_ota_payload.dart';
import 'package:boatlock_ui/ble/ble_security_codec.dart';
import 'package:boatlock_ui/ota/firmware_update_client.dart';
import 'package:boatlock_ui/ota/firmware_update_manifest.dart';
import 'package:boatlock_ui/pages/settings_page.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

class ServiceFakeBleBoatLock extends BleBoatLock {
  ServiceFakeBleBoatLock() : super(onData: (_) {});

  bool rejectNextStepMaxSpeed = false;
  bool rejectNextOtaBegin = false;
  String? ownerSecretValue;
  double? stepMaxSpeedValue;

  @override
  void setOwnerSecret(String? secret) {
    ownerSecretValue = normalizeOwnerSecret(secret);
  }

  @override
  Future<bool> setStepMaxSpeed(double value) async {
    stepMaxSpeedValue = value;
    if (rejectNextStepMaxSpeed) {
      Future<void>.delayed(const Duration(milliseconds: 10), () {
        _emitProfileRejection('SET_STEP_MAXSPD:${value.round()}');
      });
    }
    return true;
  }

  @override
  Future<bool> uploadFirmwareOtaBytes({
    required List<int> firmware,
    required String sha256Hex,
    OtaProgressCallback? onProgress,
  }) async {
    onProgress?.call(0, firmware.length);
    if (rejectNextOtaBegin) {
      Future<void>.delayed(const Duration(milliseconds: 10), () {
        _emitProfileRejection('OTA_BEGIN:${firmware.length},$sha256Hex');
      });
      return false;
    }
    return true;
  }

  void _emitProfileRejection(String command) {
    const reason = 'profile';
    final rejection = BleCommandRejection(
      reason: reason,
      profile: 'release',
      scope: 'service',
      command: command,
    );
    diagnostics.value = diagnostics.value.copyWith(
      commandRejectEvents: diagnostics.value.commandRejectEvents + 1,
      lastCommandRejection: rejection,
      commandRejects: List<BleCommandRejection>.unmodifiable([
        ...diagnostics.value.commandRejects,
        rejection,
      ]),
    );
  }
}

class FakeFirmwareUpdateClient extends FirmwareUpdateClient {
  const FakeFirmwareUpdateClient(this.bundle)
    : super(manifestUrl: 'http://localhost/manifest.json');

  final FirmwareUpdateBundle bundle;

  @override
  bool get configured => true;

  @override
  Future<FirmwareUpdateManifest> fetchLatestManifest() async {
    return bundle.manifest;
  }

  @override
  Future<FirmwareUpdateBundle> downloadFirmware(
    FirmwareUpdateManifest manifest, {
    void Function(int receivedBytes, int totalBytes)? onProgress,
  }) async {
    onProgress?.call(bundle.firmware.length, bundle.firmware.length);
    return bundle;
  }
}

Widget _wrap(Widget child) => MaterialApp(home: child);

Future<void> _setDebugMenu(WidgetTester tester, bool enabled) async {
  final debugSwitch = find.widgetWithText(SwitchListTile, 'Debug');
  expect(debugSwitch, findsOneWidget);
  final visible = find.text('Макс. скорость').evaluate().isNotEmpty;
  if (visible == enabled) return;
  await tester.tap(debugSwitch);
  await tester.pumpAndSettle();
}

SettingsPage _page(
  ServiceFakeBleBoatLock ble, {
  FirmwareUpdateClient firmwareUpdateClient = const FirmwareUpdateClient(),
}) {
  return SettingsPage(
    ble: ble,
    firmwareUpdateClient: firmwareUpdateClient,
    holdHeading: false,
    stepMaxSpd: 1000,
    stepAccel: 500,
    compassOffset: 0,
    compassQ: 0,
    magQ: 0,
    gyroQ: 0,
    rvAcc: 0,
    magNorm: 0,
    gyroNorm: 0,
    pitch: 0,
    roll: 0,
    isConnected: true,
    secPaired: false,
    secAuth: false,
    secPairWindowOpen: true,
    secReject: 'NONE',
  );
}

void main() {
  testWidgets('debug switch gates service controls', (
    WidgetTester tester,
  ) async {
    if (!kBoatLockServiceUiEnabled) {
      expect(kBoatLockServiceUiEnabled, isFalse);
      return;
    }

    await tester.binding.setSurfaceSize(const Size(900, 1800));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final ble = ServiceFakeBleBoatLock();
    await tester.pumpWidget(_wrap(_page(ble)));

    await _setDebugMenu(tester, false);
    expect(find.text('Макс. скорость'), findsNothing);
    expect(find.text('Firmware OTA'), findsNothing);

    await _setDebugMenu(tester, true);
    expect(find.text('Макс. скорость'), findsOneWidget);
    expect(find.text('Firmware OTA'), findsOneWidget);
  });

  testWidgets('rolls back service setting when profile rejection arrives', (
    WidgetTester tester,
  ) async {
    if (!kBoatLockServiceUiEnabled) {
      expect(kBoatLockServiceUiEnabled, isFalse);
      return;
    }

    await tester.binding.setSurfaceSize(const Size(900, 1800));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final ble = ServiceFakeBleBoatLock()..rejectNextStepMaxSpeed = true;
    await tester.pumpWidget(_wrap(_page(ble)));
    await _setDebugMenu(tester, true);

    await tester.tap(find.text('Макс. скорость'));
    await tester.pumpAndSettle();
    await tester.enterText(
      find.descendant(
        of: find.byType(AlertDialog),
        matching: find.byType(TextField),
      ),
      '1200',
    );
    await tester.tap(find.text('OK'));
    await tester.pump(const Duration(milliseconds: 50));
    await tester.pump(const Duration(milliseconds: 700));

    expect(ble.stepMaxSpeedValue, 1200);
    expect(find.text('1000'), findsOneWidget);
    expect(find.text('1200'), findsNothing);
    expect(
      find.text(
        'Команда SET_STEP_MAXSPD отклонена профилем release: нужен service',
      ),
      findsOneWidget,
    );
  });

  testWidgets('shows structured OTA profile rejection after upload failure', (
    WidgetTester tester,
  ) async {
    if (!kBoatLockServiceUiEnabled) {
      expect(kBoatLockServiceUiEnabled, isFalse);
      return;
    }

    final firmware = Uint8List.fromList(List<int>.filled(64, 0x42));
    final sha = boatLockSha256Hex(firmware);
    final manifest = FirmwareUpdateManifest(
      schema: 1,
      channel: 'release',
      repo: 'dslimp/boatlock',
      branch: 'release/v0.2.x',
      gitSha: '14c43a5',
      workflowRunId: 123,
      firmwareVersion: '0.2.0',
      platformioEnv: 'esp32s3_service',
      commandProfile: 'service',
      artifactName: 'firmware-esp32s3-service',
      binaryUrl: Uri.parse('https://example.com/firmware.bin'),
      size: firmware.length,
      sha256: sha,
      builtAt: DateTime.utc(2026, 4, 26),
    );

    await tester.binding.setSurfaceSize(const Size(900, 1800));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final ble = ServiceFakeBleBoatLock()..rejectNextOtaBegin = true;
    await tester.pumpWidget(
      _wrap(
        _page(
          ble,
          firmwareUpdateClient: FakeFirmwareUpdateClient(
            FirmwareUpdateBundle(manifest: manifest, firmware: firmware),
          ),
        ),
      ),
    );
    await _setDebugMenu(tester, true);

    await tester.tap(find.text('Обновить до релиза'));
    await tester.pump(const Duration(milliseconds: 50));
    await tester.pump(const Duration(milliseconds: 700));

    expect(
      find.text('Команда OTA_BEGIN отклонена профилем release: нужен service'),
      findsWidgets,
    );
  });
}
