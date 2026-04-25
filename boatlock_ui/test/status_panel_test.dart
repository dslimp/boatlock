import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:boatlock_ui/models/boat_data.dart';
import 'package:boatlock_ui/widgets/status_panel.dart';

void main() {
  testWidgets('shows data when provided', (WidgetTester tester) async {
    final data = BoatData(
      lat: 0,
      lon: 0,
      anchorLat: 59.1234,
      anchorLon: 30.5678,
      anchorHeading: 87.0,
      distance: 12.3,
      heading: 45,
      battery: 50,
      status: 'WARN',
      statusReasons: 'NO_GPS,NO_COMPASS',
      mode: 'MANUAL',
      rssi: -63,
      holdHeading: true,
      stepSpr: 200,
      stepMaxSpd: 1000,
      stepAccel: 500,
      headingRaw: 0,
      compassOffset: 0,
      compassQ: 0,
      magQ: 0,
      gyroQ: 0,
      rvAcc: 0,
      magNorm: 0,
      gyroNorm: 0,
      pitch: 0,
      roll: 0,
      secPaired: false,
      secAuth: false,
      secPairWindowOpen: false,
      secReject: 'NONE',
      gnssQ: 0,
    );
    await tester.pumpWidget(MaterialApp(home: StatusPanel(data: data)));
    expect(find.text('WARN'), findsOneWidget);
    expect(find.text('NO_GPS,NO_COMPASS'), findsOneWidget);
    expect(find.text('59.123, 30.568'), findsOneWidget);
    expect(find.text('MANUAL'), findsOneWidget);
    expect(find.text('-63 дБ'), findsOneWidget);
  });

  testWidgets('renders empty when no data', (WidgetTester tester) async {
    await tester.pumpWidget(const MaterialApp(home: StatusPanel()));
    expect(find.byType(Container), findsNothing);
  });
}
