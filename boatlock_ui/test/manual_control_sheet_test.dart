import 'package:boatlock_ui/widgets/manual_control_sheet.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('hold sends manual set and release sends manual off', (
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
                  required int steer,
                  required int throttlePct,
                  int ttlMs = 1000,
                }) async {
                  sent.add('set:$steer,$throttlePct,$ttlMs');
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

    final gesture = await tester.press(find.text('Вперед'));
    await tester.pump(const Duration(milliseconds: 300));
    await gesture.up();
    await tester.pump();

    expect(sent.first, 'set:0,35,1000');
    expect(
      sent.where((line) => line == 'set:0,35,1000').length,
      greaterThan(1),
    );
    expect(sent.last, 'off');
  });

  testWidgets('disabled manual pad does not send commands', (tester) async {
    final sent = <String>[];
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: ManualControlSheet(
            enabled: false,
            mode: 'IDLE',
            onManualControl:
                ({
                  required int steer,
                  required int throttlePct,
                  int ttlMs = 1000,
                }) async {
                  sent.add('set');
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

    await tester.tap(find.text('Вперед'));
    await tester.pump(const Duration(milliseconds: 300));

    expect(sent, isEmpty);
    expect(find.text('Нет BLE-связи'), findsOneWidget);
  });

  testWidgets('manual off button is not labeled as emergency stop', (
    tester,
  ) async {
    final sent = <String>[];
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: ManualControlSheet(
            enabled: true,
            mode: 'MANUAL',
            onManualControl:
                ({
                  required int steer,
                  required int throttlePct,
                  int ttlMs = 1000,
                }) async {
                  sent.add('set');
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
    expect(find.text('MANUAL OFF'), findsOneWidget);

    await tester.tap(find.text('MANUAL OFF'));
    await tester.pump();

    expect(sent, ['off']);
    expect(find.text('MANUAL_OFF отправлен'), findsOneWidget);
  });
}
