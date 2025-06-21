class BoatData {
  final double lat;
  final double lon;
  final double anchorLat;
  final double anchorLon;
  final double distance;
  final double heading;
  final int battery;
  final String status;
  final bool holdHeading;
  final int emuCompass;
  final int routeIdx;
  final int stepSpr;
  final double stepMaxSpd;
  final double stepAccel;

  BoatData({
    required this.lat,
    required this.lon,
    required this.anchorLat,
    required this.anchorLon,
    required this.distance,
    required this.heading,
    required this.battery,
    required this.status,
    required this.holdHeading,
    required this.emuCompass,
    required this.routeIdx,
    required this.stepSpr,
    required this.stepMaxSpd,
    required this.stepAccel,
  });

  factory BoatData.fromJson(Map<String, dynamic> json) {
    return BoatData(
      lat: double.tryParse(json['lat']?.toString() ?? '') ?? 0,
      lon: double.tryParse(json['lon']?.toString() ?? '') ?? 0,
      anchorLat: double.tryParse(json['anchorLat']?.toString() ?? '') ?? 0,
      anchorLon: double.tryParse(json['anchorLon']?.toString() ?? '') ?? 0,
      distance: double.tryParse(json['distance']?.toString() ?? '') ?? 0,
      heading: double.tryParse(json['heading']?.toString() ?? '') ?? 0,
      battery: int.tryParse(json['battery']?.toString() ?? '') ?? 0,
      status: json['status']?.toString() ?? '',
      holdHeading:
          int.tryParse(json['holdHeading']?.toString() ?? '') == 1,
      emuCompass: int.tryParse(json['emuCompass']?.toString() ?? '') ?? 0,
      routeIdx: int.tryParse(json['routeIdx']?.toString() ?? '') ?? 0,
      stepSpr: int.tryParse(json['stepSpr']?.toString() ?? '') ?? 200,
      stepMaxSpd: double.tryParse(json['stepMaxSpd']?.toString() ?? '') ?? 1000,
      stepAccel: double.tryParse(json['stepAccel']?.toString() ?? '') ?? 500,
    );
  }
}
