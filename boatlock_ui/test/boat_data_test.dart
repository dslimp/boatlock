import 'package:flutter_test/flutter_test.dart';
import 'package:boatlock_ui/models/boat_data.dart';

void main() {
  test('BoatData.fromJson parses values', () {
    final data = BoatData.fromJson({
      'lat': '1.23',
      'lon': '4.56',
      'anchorLat': '7.89',
      'anchorLon': '0.12',
      'anchorHead': '123.4',
      'distance': '3.4',
      'heading': '90',
      'battery': '75',
      'status': 'OK',
      'holdHeading': '1',
      'emuCompass': '0',
      'routeIdx': '2',
      'stepSpr': '400',
      'stepMaxSpd': '1500',
      'stepAccel': '600',
    });
    expect(data.lat, 1.23);
    expect(data.lon, 4.56);
    expect(data.anchorHeading, 123.4);
    expect(data.holdHeading, isTrue);
    expect(data.stepSpr, 400);
    expect(data.status, 'OK');
  });
}
