import 'package:boatlock_ui/widgets/manual_control_sheet.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('latched vector and speed send refreshed manual target', (
    tester,
  ) async {
    final sent = <String>[];
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: ManualControlSheet(
            enabled: true,
            mode: 'IDLE',
            onManualControl:
                ({
                  required double angleDeg,
                  required int throttlePct,
                  int ttlMs = 1000,
                }) async {
                  sent.add(
                    'target:${angleDeg.toStringAsFixed(0)},$throttlePct,$ttlMs',
                  );
                  return true;
                },
            onManualOff: () async {
              sent.add('off');
              return true;
            },
          ),
        ),
      ),
    );

    await tester.tap(find.text('Правее'));
    await tester.pump();
    await tester.tap(find.text('Быстрее'));
    await tester.pump(const Duration(milliseconds: 300));

    expect(sent.first, 'target:10,0,1000');
    expect(sent, contains('target:10,20,1000'));
    expect(
      sent.where((line) => line == 'target:10,20,1000').length,
      greaterThan(1),
    );
  });

  testWidgets('stop resets target and sends manual off', (tester) async {
    final sent = <String>[];
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: ManualControlSheet(
            enabled: true,
            mode: 'MANUAL',
            onManualControl:
                ({
                  required double angleDeg,
                  required int throttlePct,
                  int ttlMs = 1000,
                }) async {
                  sent.add('target');
                  return true;
                },
            onManualOff: () async {
              sent.add('off');
              return true;
            },
          ),
        ),
      ),
    );

    expect(find.text('STOP'), findsNothing);
    expect(find.text('Стоп'), findsOneWidget);

    await tester.tap(find.text('Правее'));
    await tester.pump();
    await tester.tap(find.text('Стоп'));
    await tester.pump();

    expect(sent.last, 'off');
    expect(find.text('Остановлено'), findsOneWidget);
  });

  testWidgets('disabled manual controls do not send commands', (tester) async {
    final sent = <String>[];
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: ManualControlSheet(
            enabled: false,
            mode: 'IDLE',
            onManualControl:
                ({
                  required double angleDeg,
                  required int throttlePct,
                  int ttlMs = 1000,
                }) async {
                  sent.add('target');
                  return true;
                },
            onManualOff: () async {
              sent.add('off');
              return true;
            },
          ),
        ),
      ),
    );

    await tester.tap(find.text('Быстрее'));
    await tester.pump(const Duration(milliseconds: 300));

    expect(sent, isEmpty);
    expect(find.text('Нет BLE-связи'), findsOneWidget);
  });
}
