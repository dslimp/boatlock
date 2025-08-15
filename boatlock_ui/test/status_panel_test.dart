import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:boatlock_ui/models/boat_data.dart';
import 'package:boatlock_ui/widgets/status_panel.dart';

void main() {
  testWidgets('shows data when provided', (WidgetTester tester) async {
    final data = BoatData(
      lat: 0,
      lon: 0,
      anchorLat: 0,
      anchorLon: 0,
      distance: 12.3,
      heading: 45,
      battery: 50,
      status: 'OK',
      holdHeading: true,
      emuCompass: 0,
      routeIdx: 0,
      stepSpr: 200,
      stepMaxSpd: 1000,
      stepAccel: 500,
    );
    await tester.pumpWidget(MaterialApp(home: StatusPanel(data: data)));
    expect(find.text('12.3 м'), findsOneWidget);
    expect(find.text('45°'), findsOneWidget);
    expect(find.text('50%'), findsOneWidget);
    expect(find.text('OK'), findsOneWidget);
  });

  testWidgets('renders empty when no data', (WidgetTester tester) async {
    await tester.pumpWidget(const MaterialApp(home: StatusPanel()));
    expect(find.byType(Container), findsNothing);
  });
}
